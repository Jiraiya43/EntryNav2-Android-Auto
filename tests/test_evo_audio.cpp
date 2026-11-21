// Copyright (c) 2026 CallMeLewis
// Sound over Wi-Fi (platform/evo_audio.cpp, platform/evo_alsa_out.cpp): the settings file, and the playback thread
// against tests/fake_asound.c in place of the HU's libasound.so.2. No car, no sound card.
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <atomic>
#include <string>
#include <vector>

#include "../core/aa_protocol.hpp"

#include "../platform/audio_route.hpp"
#include "../platform/evo_alsa_out.hpp"
#include "../platform/evo_audio.hpp"
#include "../platform/evo_log.hpp"

namespace {

int g_failures = 0;

#define CHECK(x)                                                \
    do {                                                        \
        if (!(x)) {                                             \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); \
            ++g_failures;                                       \
        }                                                       \
    } while (0)

std::string g_dir;

std::string pathOf(const char* name) {
    return g_dir + "/" + name;
}

std::string readAll(const std::string& path) {
    std::string all;
    FILE* f = fopen(path.c_str(), "rb");
    if (f) {
        char buf[4096];
        size_t n;
        while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
            all.append(buf, n);
        }
        fclose(f);
    }
    return all;
}

void writeAll(const std::string& path, const char* text) {
    FILE* f = fopen(path.c_str(), "w");
    if (f) {
        fputs(text, f);
        fclose(f);
    }
}

void forget() {
    unlink(pathOf("log").c_str());
    unlink(pathOf("out").c_str());
}

// What the fake library was asked, one call per line.
std::string calls() {
    return readAll(pathOf("log"));
}

// The frames it was given, as samples.
std::vector<int16_t> played() {
    const std::string bytes = readAll(pathOf("out"));
    std::vector<int16_t> samples(bytes.size() / 2);
    if (!samples.empty()) {
        memcpy(&samples[0], bytes.data(), samples.size() * 2);
    }
    return samples;
}

// Waits until the fake library recorded `what` (the playback thread works on its own), two seconds at most.
bool waitFor(const char* what) {
    for (int i = 0; i < 400; ++i) {
        if (calls().find(what) != std::string::npos) {
            return true;
        }
        usleep(5000);
    }
    return false;
}

// How many of the samples played have this value.
size_t countOf(const std::vector<int16_t>& samples, int16_t value) {
    size_t n = 0;
    for (size_t i = 0; i < samples.size(); ++i) {
        n += samples[i] == value;
    }
    return n;
}

const size_t kBuffer = 4800;  // the stand-in PCM's buffer, in frames: filled with silence before each first sound

void send(evo::EvoAudio& audio, uint8_t channel, const std::vector<int16_t>& samples) {
    audio.data(channel, reinterpret_cast<const uint8_t*>(&samples[0]), samples.size() * 2);
}

void settingsFile() {
    evo::AudioSettings s;
    CHECK(s.pcm == "entertainment" && s.gainPercent == 100 && s.duckPercent == 30 && s.route == 0x315);
    CHECK(!evo::loadAudioSettings(pathOf("none").c_str(), s));  // no file: sound stays on Bluetooth
    writeAll(pathOf("settings"), "");
    CHECK(evo::loadAudioSettings(pathOf("settings").c_str(), s) && s.pcm == "entertainment" && s.gainPercent == 100);
    CHECK(s.route == 0x315);  // an empty file (Flare Drive's switch): CarPlay's media route to "entertainment"
    CHECK(s.promptPcm == "carplay_alt" && s.promptRoute == 0x31A);  // and its prompt route, to its own PCM
    {
        // The prompt route goes with CarPlay's media route; it can be set, or set to 0 (prompts in the mix).
        evo::AudioSettings p;
        writeAll(pathOf("settings"), "pcm=default\n");
        CHECK(evo::loadAudioSettings(pathOf("settings").c_str(), p) && p.promptRoute == 0);
        evo::AudioSettings q;
        writeAll(pathOf("settings"), "route=0\n");
        CHECK(evo::loadAudioSettings(pathOf("settings").c_str(), q) && q.promptRoute == 0);
        evo::AudioSettings r;
        writeAll(pathOf("settings"), "prompt-route=0\n");
        CHECK(evo::loadAudioSettings(pathOf("settings").c_str(), r) && r.route == 0x315 && r.promptRoute == 0);
        evo::AudioSettings t;
        writeAll(pathOf("settings"), "pcm=default\nprompt-pcm=tts\nprompt-route=332\nprompt-pcm=bad name\n");
        CHECK(evo::loadAudioSettings(pathOf("settings").c_str(), t) && t.promptPcm == "tts" && t.promptRoute == 0x332);
        evo::AudioSettings u;
        writeAll(pathOf("settings"), "prompt-route=x\nprompt-route=\n");
        CHECK(evo::loadAudioSettings(pathOf("settings").c_str(), u) && u.promptRoute == 0x31A);
    }
    {
        evo::AudioSettings other;
        writeAll(pathOf("settings"), "pcm=default\n");
        CHECK(evo::loadAudioSettings(pathOf("settings").c_str(), other) && other.route == 0);  // no route for it
        writeAll(pathOf("settings"), "pcm=tts\nroute=319\n");
        CHECK(evo::loadAudioSettings(pathOf("settings").c_str(), other) && other.route == 0x319);
        writeAll(pathOf("settings"), "route=0\n");
        CHECK(evo::loadAudioSettings(pathOf("settings").c_str(), other) && other.route == 0);
        writeAll(pathOf("settings"), "route=12345\nroute=x\n");
        evo::AudioSettings kept;
        CHECK(evo::loadAudioSettings(pathOf("settings").c_str(), kept) && kept.route == 0x315);
        uint8_t payload[4];
        evo::buildRoutePayload(payload, 0xA512, 0x315);
        CHECK(payload[0] == 0xA5 && payload[1] == 0x12 && payload[2] == 0x03 && payload[3] == 0x15);
        CHECK(evo::audioRouteMain("swap", "315") == 2 && evo::audioRouteMain("build", "") == 2 &&
              evo::audioRouteMain("build", "0") == 2 && evo::audioRouteMain("remove", "10000") == 2);
    }
    writeAll(pathOf("settings"), "pcm=entertainment\r\ngain=40\nduck=0\nother=1\n");
    CHECK(evo::loadAudioSettings(pathOf("settings").c_str(), s));
    CHECK(s.pcm == "entertainment" && s.gainPercent == 40 && s.duckPercent == 0);
    // Invalid lines keep what was there.
    writeAll(pathOf("settings"), "pcm=bad name;rm\ngain=101\ngain=-1\ngain=4x\ngain=\nduck=1000\npcm=\n");
    CHECK(evo::loadAudioSettings(pathOf("settings").c_str(), s));
    CHECK(s.pcm == "entertainment" && s.gainPercent == 40 && s.duckPercent == 0);
    CHECK(evo::validPcmName("plug:dmix_48000") && evo::validPcmName("hw:0,1") && evo::validPcmName("a=b.c-d_e"));
    CHECK(!evo::validPcmName("") && !evo::validPcmName(std::string(64, 'a')) && !evo::validPcmName("a b") &&
          !evo::validPcmName("a/b") && !evo::validPcmName("$(x)"));
}

void listing() {
    evo::AlsaOut out;
    std::vector<std::string> names;
    CHECK(out.list(names));
    // Playback only: the capture PCM is left out; the description on one line.
    CHECK(names.size() == 2 && names[0] == "entertainment  (Main mix)" && names[1] == "default");
}

void mediaAndSpeech() {
    forget();
    evo::AudioSettings s;
    s.pcm = "entertainment";
    s.route = 0;  // no request to a car's audio manager here
    s.gainPercent = 50;
    evo::EvoAudio audio;
    CHECK(audio.start(s));
    CHECK(calls().empty());  // nothing opened before the first sound
    audio.stream(aa::channel::kMediaAudio, true, 0);
    send(audio, aa::channel::kMediaAudio, std::vector<int16_t>(48000 * 2 * 3 / 10, 2000));  // 300 ms
    audio.stream(aa::channel::kMediaAudio, false, 0);
    CHECK(waitFor("drain"));
    std::string log = calls();
    CHECK(log.find("open entertainment stream=0 mode=0\n"
                   "params format=2 access=3 channels=2 rate=48000 resample=1 latency=100000\n") == 0);
    // The PCM's buffer filled with silence first, then every frame, with the gain applied.
    std::vector<int16_t> out = played();
    CHECK(out.size() == (kBuffer + 14400) * 2);
    CHECK(countOf(out, 0) == kBuffer * 2 && countOf(out, 1000) == 14400 * 2);
    CHECK(!out.empty() && out[0] == 0 && out[kBuffer * 2] == 1000 && out[out.size() - 1] == 1000);
    CHECK(log.find("close") == std::string::npos);  // kept open for the next sound

    // Speech at 16 kHz mono comes out at 48 kHz on both sides, on the PCM already open.
    forget();
    audio.stream(aa::channel::kGuidanceAudio, true, 0);
    send(audio, aa::channel::kGuidanceAudio, std::vector<int16_t>(16000 / 10, 4000));  // 100 ms
    audio.stream(aa::channel::kGuidanceAudio, false, 0);
    CHECK(waitFor("drain"));
    out = played();
    CHECK(out.size() == (kBuffer + 4800) * 2 && countOf(out, 2000) == 4800 * 2);
    CHECK(!out.empty() && out[kBuffer * 2] == 2000 && out[kBuffer * 2 + 1] == 2000 && out[out.size() - 1] == 2000);
    CHECK(calls().find("open") == std::string::npos);

    // An unknown configuration is not played at the wrong speed; another channel's sound is nobody's.
    forget();
    audio.stream(aa::channel::kGuidanceAudio, true, 7);
    send(audio, aa::channel::kGuidanceAudio, std::vector<int16_t>(1600, 4000));
    send(audio, aa::channel::kVideo, std::vector<int16_t>(1600, 4000));
    audio.stream(aa::channel::kVideo, true, 0);
    usleep(150000);
    CHECK(played().empty());

    // The session ends with sound waiting: it is dropped, and stop() closes the PCM.
    audio.stream(aa::channel::kMediaAudio, true, 0);
    send(audio, aa::channel::kMediaAudio, std::vector<int16_t>(4800, 2000));  // 50 ms: still waiting for more
    audio.sessionEnded();
    usleep(150000);
    CHECK(played().empty());
    audio.stop();
    CHECK(calls().find("close") != std::string::npos);
    send(audio, aa::channel::kMediaAudio, std::vector<int16_t>(48000, 2000));  // after stop: ignored
    audio.stop();
}

void troubles() {
    // A PCM that does not open: one try, no sound, nothing piles up, and stop() still returns.
    forget();
    evo::AudioSettings s;
    s.pcm = "missing";
    s.route = 0;
    {
        evo::EvoAudio audio;
        CHECK(audio.start(s));
        audio.stream(aa::channel::kMediaAudio, true, 0);
        send(audio, aa::channel::kMediaAudio, std::vector<int16_t>(48000 * 2, 2000));  // one second
        CHECK(waitFor("open missing"));
        usleep(200000);
        const std::string log = calls();
        CHECK(log == "open missing stream=0 mode=0\n");  // not tried again at every period
        CHECK(played().empty());
    }
    // An underrun reported by the library is recovered and the frames are written again.
    forget();
    s.pcm = "xrun";
    {
        evo::EvoAudio audio;
        CHECK(audio.start(s));
        audio.stream(aa::channel::kMediaAudio, true, 0);
        send(audio, aa::channel::kMediaAudio, std::vector<int16_t>(48000 * 2 / 5, 2000));  // 200 ms
        audio.stream(aa::channel::kMediaAudio, false, 0);
        CHECK(waitFor("drain"));
        CHECK(calls().find("write underrun\nrecover underrun\n") != std::string::npos);
        // Nothing lost; the buffer was filled again after the underrun.
        const std::vector<int16_t> out = played();
        CHECK(countOf(out, 2000) == 9600 * 2 && out.size() == (2 * kBuffer + 9600) * 2);
    }
    // Without the library: no sound over Wi-Fi, and the service keeps Bluetooth.
    evo::AlsaOut::setLibraryForTest("/nonexistent/libasound.so.2");
    evo::EvoAudio none;
    CHECK(!none.start(s));
    none.stop();
}

// The stand-in PCM as a sound card on the real clock: sound sent as a phone does, 20 ms at a time, comes out whole
// and without a gap. (Without the buffer filled first, the empty buffer took the first sound at once, the queue ran
// dry and the stream stopped and started ten times a second.)
void realTime() {
    forget();
    setenv("FAKE_ASOUND_PACE", "1", 1);
    evo::AudioSettings s;
    s.pcm = "entertainment";
    s.route = 0;  // no request to a car's audio manager here
    {
        evo::EvoAudio audio;
        CHECK(audio.start(s));
        audio.stream(aa::channel::kMediaAudio, true, 0);
        const uint32_t start = evo::monoMs();
        for (unsigned i = 0; i < 75; ++i) {  // 1.5 s
            send(audio, aa::channel::kMediaAudio, std::vector<int16_t>(960 * 2, 3000));
            if (i == 25) {
                audio.stream(aa::channel::kGuidanceAudio, true, 0);
            }
            if (i >= 25 && i < 45) {  // a prompt of 400 ms over the music
                send(audio, aa::channel::kGuidanceAudio, std::vector<int16_t>(320, 500));
            }
            if (i == 44) {
                audio.stream(aa::channel::kGuidanceAudio, false, 0);
            }
            const uint32_t due = start + (i + 1) * 20, now = evo::monoMs();
            if (due > now) {
                usleep((due - now) * 1000);
            }
        }
        audio.stream(aa::channel::kMediaAudio, false, 0);
        CHECK(waitFor("drain"));
        const evo::AudioMixer::Stats stats = audio.stats();
        CHECK(stats.framesIn[evo::AudioMixer::kMedia] == 72000 && stats.framesIn[evo::AudioMixer::kSpeech] == 19200);
        CHECK(stats.dropped[evo::AudioMixer::kMedia] == 0 && stats.dropped[evo::AudioMixer::kSpeech] == 0);
        // One gap allowed for a PC that stalls the test; the fault this guards against gave dozens.
        CHECK(stats.underruns[evo::AudioMixer::kMedia] <= 1 && stats.underruns[evo::AudioMixer::kSpeech] <= 1);
        const std::string log = calls();
        size_t emptied = 0;
        for (size_t at = log.find("ran empty"); at != std::string::npos; at = log.find("ran empty", at + 1)) {
            ++emptied;
        }
        CHECK(emptied <= 1);
        // Every frame of music came out: alone (3000), or lowered under the prompt and summed with it.
        const std::vector<int16_t> out = played();
        CHECK(out.size() >= (kBuffer + 72000) * 2);
        // 1399: 3000 at 30 % in the mixer's fixed point (899), plus the prompt's 500.
        CHECK(countOf(out, 3000) > 15000 * 2 && countOf(out, 1399) > 10000 * 2);
    }
    unsetenv("FAKE_ASOUND_PACE");
}

std::atomic<size_t> g_reported(0);  // bytes of media reported played: from the test's thread and the playback thread
void onMediaPlayed(size_t bytes, void*) {
    g_reported += bytes;
}

bool waitForReported(size_t bytes) {
    for (int i = 0; i < 400 && g_reported != bytes; ++i) {
        usleep(5000);
    }
    return g_reported == bytes;
}

// Every byte of media handed over is reported, once played or at once when it never will be: the session acks the
// phone's music by it, so a byte never reported would hold the phone's stream back.
void playedReports() {
    forget();
    evo::AudioSettings s;
    s.pcm = "entertainment";
    s.route = 0;  // no request to a car's audio manager here
    {
        evo::EvoAudio audio;
        audio.setMediaPlayed(onMediaPlayed, nullptr);
        // Before start(), and before the phone started the stream: none of it will be played.
        send(audio, aa::channel::kMediaAudio, std::vector<int16_t>(100, 1));
        CHECK(g_reported == 200);
        CHECK(audio.start(s));
        send(audio, aa::channel::kMediaAudio, std::vector<int16_t>(100, 1));
        CHECK(g_reported == 400);
        // Played: nothing at once, all of it by the time it has gone to the PCM. Speech is nobody's to report.
        g_reported = 0;
        audio.stream(aa::channel::kMediaAudio, true, 0);
        audio.stream(aa::channel::kGuidanceAudio, true, 0);
        send(audio, aa::channel::kMediaAudio, std::vector<int16_t>(1920, 2000));  // below the prebuffer
        CHECK(g_reported == 0);
        send(audio, aa::channel::kMediaAudio, std::vector<int16_t>(26880, 2000));  // 300 ms total
        send(audio, aa::channel::kGuidanceAudio, std::vector<int16_t>(1600, 4000));
        audio.stream(aa::channel::kGuidanceAudio, false, 0);
        audio.stream(aa::channel::kMediaAudio, false, 0);
        CHECK(waitFor("drain") && waitForReported(57600));
        // Bytes that make no whole frame are reported at once, the rest once played.
        g_reported = 0;
        audio.stream(aa::channel::kMediaAudio, true, 0);
        const uint8_t odd[7] = {0};
        audio.data(aa::channel::kMediaAudio, odd, sizeof(odd));
        CHECK(g_reported == 3);
        audio.data(aa::channel::kMediaAudio, odd, 1);
        CHECK(g_reported == 4);
        audio.stream(aa::channel::kMediaAudio, false, 0);
        CHECK(waitForReported(8));
        // The session ends with sound waiting: it is dropped, and there is nobody left to tell.
        g_reported = 0;
        audio.stream(aa::channel::kMediaAudio, true, 0);
        send(audio, aa::channel::kMediaAudio, std::vector<int16_t>(4800, 2000));  // 50 ms: still waiting for more
        audio.sessionEnded();
        usleep(150000);
        CHECK(g_reported == 0);
        audio.stop();
        send(audio, aa::channel::kMediaAudio, std::vector<int16_t>(100, 1));  // after stop
        CHECK(g_reported == 200);
    }
    // A PCM that does not open plays nothing, but the sound still leaves the queue: the phone is not held back.
    forget();
    g_reported = 0;
    s.pcm = "missing";
    s.route = 0;
    {
        evo::EvoAudio audio;
        audio.setMediaPlayed(onMediaPlayed, nullptr);
        CHECK(audio.start(s));
        audio.stream(aa::channel::kMediaAudio, true, 0);
        send(audio, aa::channel::kMediaAudio, std::vector<int16_t>(48000 * 2 / 5, 2000));  // 200 ms
        audio.stream(aa::channel::kMediaAudio, false, 0);
        CHECK(waitForReported(38400));
        CHECK(played().empty());
    }
}

// The prompt route's requests, as the helper would get them; a build's answer is given by the test.
struct RouteCall {
    bool build;
    uint16_t route;
    evo::RouteDone done;
    void* user;
};
std::vector<RouteCall> g_routes;  // the prompt thread adds to it too: under g_routesMu
pthread_mutex_t g_routesMu = PTHREAD_MUTEX_INITIALIZER;
void recordRoute(bool build, uint16_t route, evo::RouteDone done, void* user) {
    RouteCall c = {build, route, done, user};
    pthread_mutex_lock(&g_routesMu);
    g_routes.push_back(c);
    pthread_mutex_unlock(&g_routesMu);
}
void clearRoutes() {
    pthread_mutex_lock(&g_routesMu);
    g_routes.clear();
    pthread_mutex_unlock(&g_routesMu);
}
size_t routeCalls(bool build, uint16_t route = 0x31A) {
    size_t n = 0;
    pthread_mutex_lock(&g_routesMu);
    for (size_t i = 0; i < g_routes.size(); ++i) {
        n += g_routes[i].build == build && g_routes[i].route == route;
    }
    pthread_mutex_unlock(&g_routesMu);
    return n;
}
// Answers the last build of the prompt route.
void answerBuild(bool granted) {
    evo::RouteDone done = nullptr;
    void* user = nullptr;
    pthread_mutex_lock(&g_routesMu);
    for (size_t i = g_routes.size(); i-- > 0;) {
        if (g_routes[i].build && g_routes[i].done) {
            done = g_routes[i].done;
            user = g_routes[i].user;
            g_routes[i].done = nullptr;
            break;
        }
    }
    pthread_mutex_unlock(&g_routesMu);
    if (done) {
        done(granted, user);
    }
}
bool waitForRoutes(bool build, size_t n) {
    for (int i = 0; i < 400 && routeCalls(build) < n; ++i) {
        usleep(5000);
    }
    return routeCalls(build) >= n;
}

// Without the phone's music, a prompt plays on its own PCM, mono at its own rate, once the car answers for its route;
// the route is given back after the hold. With the music, it stays in the mix. Refused, it falls back to the mix.
void mediaRoutes() {
    clearRoutes();
    evo::setAudioRouteRequesterForTest(recordRoute);
    evo::AudioSettings settings;
    evo::EvoAudio audio;
    CHECK(audio.start(settings));
    const auto play = [&]() {
        audio.stream(aa::channel::kMediaAudio, true, 0);
        send(audio, aa::channel::kMediaAudio, std::vector<int16_t>(19200, 1000));
        audio.stream(aa::channel::kMediaAudio, false, 0);
    };
    const auto waitCount = [&](bool build, unsigned count) {
        for (unsigned i = 0; i < 650 && routeCalls(build, 0x315) < count; ++i) {
            usleep(10000);
        }
        CHECK(routeCalls(build, 0x315) == count);
    };
    audio.stream(aa::channel::kMediaAudio, true, 0);
    send(audio, aa::channel::kMediaAudio, std::vector<int16_t>(38400, 1000));
    waitCount(true, 1);
    CHECK(!audio.holdsRoute());
    answerBuild(false);
    waitCount(false, 1);
    usleep(2100000);
    send(audio, aa::channel::kMediaAudio, std::vector<int16_t>(38400, 1000));
    waitCount(true, 2);
    audio.stream(aa::channel::kMediaAudio, false, 0);
    answerBuild(true);
    CHECK(audio.holdsRoute());
    play();  // short pause: the PCM may still be open
    waitCount(false, 2);
    waitCount(true, 3);
    answerBuild(true);
    waitCount(false, 3);  // idle release
    CHECK(!audio.holdsRoute());
    play();
    waitCount(true, 4);
    audio.sessionEnded();
    answerBuild(true);  // late answer cannot resurrect the session
    CHECK(!audio.holdsRoute() && routeCalls(false, 0x315) == 4);
    audio.stop();
    evo::setAudioRouteRequesterForTest(nullptr);
}

void prompts() {
    evo::setAudioRouteRequesterForTest(recordRoute);
    evo::AudioSettings s;
    s.pcm = "entertainment";
    s.route = 0;  // the music's route is not asked for here
    s.promptPcm = "alt";
    s.gainPercent = 50;
    {
        forget();
        clearRoutes();
        evo::EvoAudio audio;
        CHECK(audio.start(s));
        audio.stream(aa::channel::kGuidanceAudio, true, 0);  // 16 kHz mono
        CHECK(routeCalls(true) == 1);
        send(audio, aa::channel::kGuidanceAudio, std::vector<int16_t>(1600, 4000));  // 100 ms
        audio.stream(aa::channel::kGuidanceAudio, false, 0);
        usleep(150000);
        CHECK(played().empty());  // waits for the car's answer
        answerBuild(true);
        CHECK(waitFor("drain"));
        const std::string log = calls();
        CHECK(log.find("open alt stream=0 mode=0\n"
                       "params format=2 access=3 channels=1 rate=16000 resample=1 latency=100000\n") == 0);
        CHECK(log.find("open entertainment") == std::string::npos);  // the music's PCM and route left alone
        const std::vector<int16_t> out = played();
        CHECK(out.size() == 1600 + 1600 && countOf(out, 0) == 1600 && countOf(out, 2000) == 1600);
        CHECK(routeCalls(false) == 0);
        CHECK(waitForRoutes(false, 1));  // given back after the hold
        CHECK(waitFor("close"));
        // A refusal: the prompt goes to the mix, and so does the next one, without asking again.
        forget();
        audio.stream(aa::channel::kGuidanceAudio, true, 0);
        CHECK(routeCalls(true) == 2);
        send(audio, aa::channel::kGuidanceAudio, std::vector<int16_t>(1600, 4000));
        audio.stream(aa::channel::kGuidanceAudio, false, 0);
        answerBuild(false);
        CHECK(waitFor("open entertainment"));
        CHECK(waitFor("drain"));
        CHECK(countOf(played(), 2000) == 4800 * 2);  // in the mix: 48 kHz on both sides
        audio.stream(aa::channel::kGuidanceAudio, true, 0);
        CHECK(routeCalls(true) == 2);
        audio.stream(aa::channel::kGuidanceAudio, false, 0);
        audio.stop();
    }
    {
        // The phone's music plays: the prompt stays in the mix, no route asked for.
        forget();
        clearRoutes();
        evo::EvoAudio audio;
        CHECK(audio.start(s));
        audio.stream(aa::channel::kMediaAudio, true, 0);
        send(audio, aa::channel::kMediaAudio, std::vector<int16_t>(9600, 1000));
        audio.stream(aa::channel::kGuidanceAudio, true, 0);
        send(audio, aa::channel::kGuidanceAudio, std::vector<int16_t>(1600, 4000));
        audio.stream(aa::channel::kGuidanceAudio, false, 0);
        CHECK(routeCalls(true) + routeCalls(false) == 0);
        CHECK(waitFor("open entertainment"));
        audio.stop();
        CHECK(calls().find("open alt") == std::string::npos);
    }
    {
        // The session ends while the route is asked for: it is given back.
        clearRoutes();
        evo::EvoAudio audio;
        CHECK(audio.start(s));
        audio.stream(aa::channel::kGuidanceAudio, true, 0);
        audio.sessionEnded();
        CHECK(routeCalls(true) == 1 && routeCalls(false) == 1);
        answerBuild(true);  // late: changes nothing
        audio.stop();
        CHECK(routeCalls(false) == 1);
    }
    {
        // Switched off in the settings: every prompt in the mix, as before.
        forget();
        clearRoutes();
        evo::AudioSettings mixed = s;
        mixed.promptRoute = 0;
        evo::EvoAudio audio;
        CHECK(audio.start(mixed));
        audio.stream(aa::channel::kGuidanceAudio, true, 0);
        send(audio, aa::channel::kGuidanceAudio, std::vector<int16_t>(1600, 4000));
        audio.stream(aa::channel::kGuidanceAudio, false, 0);
        CHECK(waitFor("open entertainment") && routeCalls(true) + routeCalls(false) == 0);
        audio.stop();
    }
    evo::setAudioRouteRequesterForTest(nullptr);
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: test-evo-audio <fake-libasound.so>\n");
        return 2;
    }
    char dir[] = "/tmp/entry-audio-test-XXXXXX";
    if (!mkdtemp(dir)) {
        perror("mkdtemp");
        return 1;
    }
    g_dir = dir;
    setenv("FAKE_ASOUND_LOG", pathOf("log").c_str(), 1);
    setenv("FAKE_ASOUND_OUT", pathOf("out").c_str(), 1);
    evo::AlsaOut::setLibraryForTest(argv[1]);
    settingsFile();
    listing();
    mediaAndSpeech();
    playedReports();
    realTime();
    mediaRoutes();
    prompts();
    troubles();
    forget();
    unlink(pathOf("settings").c_str());
    rmdir(dir);
    if (g_failures) {
        printf("FAIL sound over Wi-Fi: %d checks failed\n", g_failures);
        return 1;
    }
    puts(
        "PASS sound over Wi-Fi: settings file, PCM list, media and speech played through a stand-in libasound, "
        "unknown format silent, session end, media reported once played, sound in real time without a gap, "
        "PCM that does not open, underrun, no "
        "library, prompts on their own PCM and route (granted, refused, with the music, at the session's end)");
    return 0;
}
