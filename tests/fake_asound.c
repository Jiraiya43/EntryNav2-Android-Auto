// Copyright (c) 2026 CallMeLewis
/* PC-only stand-in for the HU's libasound.so.2 (tests/test_evo_audio.cpp): the functions platform/evo_alsa_out.cpp
 * loads. Every call but the writes is recorded as a line in $FAKE_ASOUND_LOG, and the frames written are appended to
 * $FAKE_ASOUND_OUT. A PCM named "missing" does not open; "xrun" reports one underrun on its first write.
 * $FAKE_ASOUND_PACE=1: the PCM behaves like a sound card. It starts playing once its buffer is full, plays at its
 * rate on the real clock, makes a write wait while the buffer is full, and reports an underrun when it ran empty. */
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

struct pcm {
    char name[64];
    unsigned channels, rate;
    unsigned long buffer; /* frames */
    int xrun;
    int paced, playing;
    double startedAt;      /* when the buffer first filled */
    unsigned long written; /* frames since then, the first buffer included */
};

static void record(const char* line) {
    const char* path = getenv("FAKE_ASOUND_LOG");
    FILE* f = path ? fopen(path, "a") : NULL;
    if (f) {
        fprintf(f, "%s\n", line);
        fclose(f);
    }
}

static double now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

static void waitFor(double seconds) {
    if (seconds > 0) {
        struct timespec ts;
        ts.tv_sec = (time_t)seconds;
        ts.tv_nsec = (long)((seconds - (double)ts.tv_sec) * 1e9);
        nanosleep(&ts, NULL);
    }
}

int snd_pcm_open(void** handle, const char* name, int stream, int mode) {
    char line[128];
    snprintf(line, sizeof(line), "open %s stream=%d mode=%d", name, stream, mode);
    record(line);
    if (!strcmp(name, "missing")) {
        return -ENOENT;
    }
    struct pcm* p = calloc(1, sizeof(*p));
    if (!p) {
        return -ENOMEM;
    }
    snprintf(p->name, sizeof(p->name), "%s", name);
    p->xrun = !strcmp(name, "xrun");
    const char* paced = getenv("FAKE_ASOUND_PACE");
    p->paced = paced && !strcmp(paced, "1");
    *handle = p;
    return 0;
}

int snd_pcm_set_params(void* handle, int format, int access, unsigned channels, unsigned rate, int resample,
                       unsigned latency) {
    struct pcm* p = handle;
    char line[128];
    snprintf(line, sizeof(line), "params format=%d access=%d channels=%u rate=%u resample=%d latency=%u", format,
             access, channels, rate, resample, latency);
    record(line);
    p->channels = channels;
    p->rate = rate;
    p->buffer = (unsigned long)((double)rate * latency / 1e6);
    return 0;
}

int snd_pcm_get_params(void* handle, unsigned long* buffer, unsigned long* period) {
    struct pcm* p = handle;
    *buffer = p->buffer;
    *period = p->buffer / 10;
    return 0;
}

/* The frames still waiting in the buffer of a paced PCM that plays. */
static double waiting(const struct pcm* p) {
    return (double)p->written - (now() - p->startedAt) * p->rate;
}

long snd_pcm_writei(void* handle, const void* frames, unsigned long count) {
    struct pcm* p = handle;
    if (p->xrun) {
        p->xrun = 0;
        record("write underrun");
        return -EPIPE;
    }
    if (p->paced) {
        if (p->playing && waiting(p) < 0) {
            p->playing = 0;
            p->written = 0;
            record("write underrun (ran empty)");
            return -EPIPE;
        }
        if (p->playing) {
            waitFor((waiting(p) + (double)count - (double)p->buffer) / p->rate);
        }
        p->written += count;
        if (!p->playing && p->written >= p->buffer) {
            p->playing = 1;
            p->startedAt = now();
        }
    }
    const char* path = getenv("FAKE_ASOUND_OUT");
    FILE* f = path ? fopen(path, "ab") : NULL;
    if (f) {
        fwrite(frames, 2 * p->channels, count, f);
        fclose(f);
    }
    return (long)count;
}

int snd_pcm_recover(void* handle, int err, int silent) {
    (void)handle;
    (void)silent;
    record(err == -EPIPE ? "recover underrun" : "recover other");
    return 0;
}

int snd_pcm_drain(void* handle) {
    struct pcm* p = handle;
    if (p->paced) {
        if (p->playing) {
            waitFor(waiting(p) / p->rate);
        } else {
            waitFor((double)p->written / p->rate); /* less than a buffer: starts now, plays to its end */
        }
        p->playing = 0;
        p->written = 0;
    }
    record("drain");
    return 0;
}

int snd_pcm_prepare(void* handle) {
    (void)handle;
    record("prepare");
    return 0;
}

int snd_pcm_close(void* handle) {
    record("close");
    free(handle);
    return 0;
}

const char* snd_strerror(int err) {
    return strerror(err < 0 ? -err : err);
}

/* Three PCMs: one for playback, one both ways (no IOID), one for capture only. */
static const char* const kNames[] = {"entertainment", "default", "mic1"};
static const char* const kIo[] = {"Output", NULL, "Input"};
static const char* const kAbout[] = {"Main\nmix", NULL, "Microphone"};

int snd_device_name_hint(int card, const char* iface, void*** hints) {
    (void)card;
    if (strcmp(iface, "pcm")) {
        return -ENOENT;
    }
    void** list = calloc(4, sizeof(void*));
    if (!list) {
        return -ENOMEM;
    }
    for (long i = 0; i < 3; ++i) {
        list[i] = (void*)(i + 1);
    }
    *hints = list;
    return 0;
}

char* snd_device_name_get_hint(const void* hint, const char* id) {
    const long i = (long)hint - 1;
    const char* value = !strcmp(id, "NAME") ? kNames[i] : !strcmp(id, "IOID") ? kIo[i] : kAbout[i];
    return value ? strdup(value) : NULL;
}

int snd_device_name_free_hint(void** hints) {
    free(hints);
    return 0;
}
