// Copyright (c) 2026 CallMeLewis
// evo-sound: finds out, by hand on the car, where sound over Wi-Fi can play. Not installed, not part of a release.
//
//   evo-sound list                          the playback PCMs the car's sound configuration declares
//   evo-sound tone <pcm> [seconds] [gain]   a 440 Hz tone on that PCM: left, then right, then both
//   evo-sound demo <pcm> [seconds] [gain]   the service's own path (platform/evo_audio.hpp) without a phone: a tone
//                                           as media, a higher beep every 3 s as speech, media lowered under it
//
// seconds: 1 to 30 (6 when absent). gain: 1 to 100 percent of full scale (5 when absent: quiet on purpose, the car's
// volume may not apply to this PCM). Opens one playback PCM through the car's libasound and writes sound to it;
// nothing is sent on vMOST/MOST, nothing is written to the flash.
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <string>
#include <vector>

#include "aa_protocol.hpp"
#include "evo_alsa_out.hpp"
#include "evo_audio.hpp"
#include "evo_log.hpp"

namespace {

const unsigned kRate = 48000;
const double kPi = 3.14159265358979323846;

int usage() {
    fputs(
        "usage: evo-sound list\n"
        "       evo-sound tone <pcm> [seconds 1-30] [gain 1-100]\n"
        "       evo-sound tone-mono <pcm> <16000|48000> [seconds 1-30] [gain 1-100]\n"
        "       evo-sound demo <pcm> [seconds 1-30] [gain 1-100]\n",
        stderr);
    return 2;
}

// A whole number between low and high, else -1.
int numberIn(const char* text, int low, int high) {
    char* end = nullptr;
    const long value = strtol(text, &end, 10);
    return (end != text && !*end && value >= low && value <= high) ? static_cast<int>(value) : -1;
}

int list() {
    evo::AlsaOut out;
    std::vector<std::string> names;
    if (!out.list(names)) {
        printf("FAIL: %s\n", out.lastError().c_str());
        return 1;
    }
    printf("%u playback PCM(s):\n", static_cast<unsigned>(names.size()));
    for (size_t i = 0; i < names.size(); ++i) {
        printf("  %s\n", names[i].c_str());
    }
    return 0;
}

// `frames` of a sine at `hz`, `amplitude` of full scale, 20 ms fades at both ends; mono.
std::vector<int16_t> sine(unsigned rate, double hz, double amplitude, size_t frames) {
    std::vector<int16_t> out(frames);
    const size_t fade = rate / 50;
    for (size_t i = 0; i < frames; ++i) {
        double level = amplitude;
        if (i < fade) {
            level *= static_cast<double>(i) / fade;
        } else if (frames - i <= fade) {
            level *= static_cast<double>(frames - i) / fade;
        }
        out[i] = static_cast<int16_t>(32767.0 * level * sin(2 * kPi * hz * i / rate));
    }
    return out;
}

int tone(const char* pcm, int seconds, int gain) {
    evo::AlsaOut out;
    if (!out.open(pcm, kRate, 2, 100000)) {
        printf("FAIL: cannot open '%s': %s\n", pcm, out.lastError().c_str());
        return 1;
    }
    printf("'%s' open at 48 kHz stereo; 440 Hz at %d %% of full scale: left, right, both (%d s)\n", pcm, gain, seconds);
    const size_t part = static_cast<size_t>(kRate) * seconds / 3;
    const std::vector<int16_t> mono = sine(kRate, 440.0, gain / 100.0, part);
    for (int side = 0; side < 3; ++side) {
        std::vector<int16_t> stereo(part * 2, 0);
        for (size_t i = 0; i < part; ++i) {
            stereo[i * 2] = side != 1 ? mono[i] : 0;
            stereo[i * 2 + 1] = side != 0 ? mono[i] : 0;
        }
        printf("  %s\n", side == 0 ? "left" : side == 1 ? "right" : "both");
        fflush(stdout);
        if (!out.write(&stereo[0], part)) {
            printf("FAIL: write: %s\n", out.lastError().c_str());
            return 1;
        }
    }
    out.drain();
    out.close();
    printf("done: %u underrun(s). Heard it? At the car's volume, or always as loud?\n", out.underruns());
    return 0;
}

int monoTone(const char* pcm, unsigned rate, int seconds, int gain) {
    evo::AlsaOut out;
    if (!out.open(pcm, rate, 1, 100000)) {
        printf("FAIL: %s\n", out.lastError().c_str());
        return 1;
    }
    printf("'%s' open at %u Hz mono, %d s, gain %d %%\n", pcm, rate, seconds, gain);
    const std::vector<int16_t> frames = sine(rate, 440.0, gain / 100.0, rate * seconds);
    if (!out.write(frames.data(), frames.size())) {
        printf("FAIL: %s\n", out.lastError().c_str());
        return 1;
    }
    out.drain();
    return 0;
}

int demo(const char* pcm, int seconds, int gain) {
    evo::AudioSettings settings;
    settings.pcm = pcm;
    settings.gainPercent = gain;
    settings.route = 0;  // only the service asks the car's audio manager for a route
    evo::EvoAudio audio;
    if (!audio.start(settings)) {
        return 1;
    }
    printf("media: 330 Hz; speech: a 880 Hz beep every 3 s, media lowered under it (%d s)\n", seconds);
    fflush(stdout);
    audio.stream(aa::channel::kMediaAudio, true, 0);
    // Sent as a phone does: 20 ms at a time, in real time. The tones are at full scale, the gain is the mixer's.
    const size_t mediaChunk = kRate / 50, speechChunk = 16000 / 50;
    std::vector<int16_t> media(mediaChunk * 2);
    size_t mediaAt = 0;
    const std::vector<int16_t> beep = sine(16000, 880.0, 0.9, 16000 * 6 / 10);
    const uint32_t start = evo::monoMs();
    for (int tick = 0; tick < seconds * 50; ++tick) {
        for (size_t i = 0; i < mediaChunk; ++i, ++mediaAt) {
            const int16_t v = static_cast<int16_t>(32767.0 * 0.6 * sin(2 * kPi * 330.0 * mediaAt / kRate));
            media[i * 2] = media[i * 2 + 1] = v;
        }
        audio.data(aa::channel::kMediaAudio, reinterpret_cast<const uint8_t*>(&media[0]), media.size() * 2);
        const int inBeep = tick % 150;  // every 3 s, after the first second
        if (tick >= 50 && inBeep == 50) {
            audio.stream(aa::channel::kGuidanceAudio, true, 0);
        }
        const size_t from = static_cast<size_t>(inBeep - 50) * speechChunk;
        if (tick >= 50 && inBeep >= 50 && from < beep.size()) {
            const size_t count = from + speechChunk <= beep.size() ? speechChunk : beep.size() - from;
            audio.data(aa::channel::kGuidanceAudio, reinterpret_cast<const uint8_t*>(&beep[from]), count * 2);
            if (from + count == beep.size()) {
                audio.stream(aa::channel::kGuidanceAudio, false, 0);
            }
        }
        // On the clock, not 20 ms after whatever this turn took: a phone does not fall behind.
        const uint32_t due = start + static_cast<uint32_t>(tick + 1) * 20, now = evo::monoMs();
        if (due > now) {
            usleep((due - now) * 1000);
        }
    }
    audio.stream(aa::channel::kMediaAudio, false, 0);
    usleep(500000);
    audio.sessionEnded();
    audio.stop();
    printf("done. Heard the tone, and the beeps over it with the tone lowered?\n");
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc == 2 && !strcmp(argv[1], "list")) {
        return list();
    }
    if (argc >= 4 && argc <= 6 && !strcmp(argv[1], "tone-mono") && evo::validPcmName(argv[2])) {
        const int rate = numberIn(argv[3], 16000, 48000);
        const int seconds = argc > 4 ? numberIn(argv[4], 1, 30) : 6;
        const int gain = argc > 5 ? numberIn(argv[5], 1, 100) : 5;
        if ((rate != 16000 && rate != 48000) || seconds < 0 || gain < 0) {
            return usage();
        }
        return monoTone(argv[2], rate, seconds, gain);
    }
    const bool isTone = argc >= 3 && !strcmp(argv[1], "tone"), isDemo = argc >= 3 && !strcmp(argv[1], "demo");
    if ((!isTone && !isDemo) || argc > 5 || !evo::validPcmName(argv[2])) {
        return usage();
    }
    const int seconds = argc > 3 ? numberIn(argv[3], 1, 30) : 6;
    const int gain = argc > 4 ? numberIn(argv[4], 1, 100) : 5;
    if (seconds < 0 || gain < 0) {
        return usage();
    }
    return isTone ? tone(argv[2], seconds, gain) : demo(argv[2], seconds, gain);
}
