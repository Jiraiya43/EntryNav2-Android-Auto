// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Where an Android Auto prompt plays with sound over Wi-Fi, and the car's route for it.
//
// Mixed into the music, a prompt takes CarPlay's media route (0x315) and with it the car's music source: the radio
// stops. BMW's CarPlay plays its Maps prompts on a PCM of their own (pcm.carplay_alt) under route 0x31A, built while
// the iPhone asks the car to lower its sound and removed after; BMW's CarLife does the same with 0x332 for Android
// phones (/usr/local/bin/CARPLAY and CARLIFE, read on 14 November 2026). The car then lowers whatever plays instead of
// replacing it. A prompt goes that way while the phone's own music isn't playing on the car; with it, the prompt stays
// in the mix, which lowers the music itself.
//
// The route is asked for at the prompt's start, and its sound waits for the car's answer. The car can take seconds:
// after kGrantWaitMs the prompt plays anyway, since the music route would take the radio again. Only a car that
// refuses the route sends prompts back to the mix, for kBackoffMs. The route is removed kHoldMs after the last prompt,
// so prompts close together keep it.
//
// Pure: no clock, no thread, no I/O. The caller gives the time, holds its own lock, and sends the route requests the
// calls return. tests/test_prompt_path.cpp.
#ifndef AA_ENTRYNAV_PLATFORM_PROMPT_PATH_HPP
#define AA_ENTRYNAV_PLATFORM_PROMPT_PATH_HPP

#include <stdint.h>

namespace evo {

class PromptPath {
public:
    enum Action { kNothing = 0, kBuild, kRemove };
    enum Play { kWait = 0, kPlay, kFallBack };
    enum : uint32_t { kGrantWaitMs = 3000, kHoldMs = 1000, kBackoffMs = 60000 };

    // enabled: false keeps every prompt in the mix, as before this path existed.
    explicit PromptPath(bool enabled = false)
        : enabled_(enabled),
          route_(kOff),
          askedAt_(0),
          asks_(0),
          inPrompt_(false),
          doneAt_(0),
          held_(false),
          backoffAt_(0),
          backoff_(false) {}

    // A prompt starts. True: it goes to the prompt PCM (play() says when); false: into the mix. action: the route
    // request to send.
    bool start(bool musicPlaying, uint32_t nowMs, Action& action) {
        action = kNothing;
        if (backoff_ && nowMs - backoffAt_ >= kBackoffMs) {
            backoff_ = false;
        }
        if (!enabled_ || musicPlaying || backoff_) {
            return false;
        }
        inPrompt_ = true;
        held_ = false;
        if (route_ == kOff) {
            route_ = kAsked;
            askedAt_ = nowMs;
            ++asks_;
            action = kBuild;
        }
        return true;
    }

    // Which build the next answer belongs to: the one start() just asked for.
    uint32_t ask() const { return asks_; }

    // The car's answer to build `ask`. Refused: the prompt falls back to the mix, and the next ones for kBackoffMs.
    void answered(uint32_t ask, bool granted, uint32_t nowMs) {
        if (route_ != kAsked || ask != asks_) {
            return;  // an earlier build's, or the route was removed meanwhile
        }
        if (granted) {
            route_ = kOn;
            return;
        }
        route_ = kOff;
        backoff_ = true;
        backoffAt_ = nowMs;
    }

    // During a prompt on this path: may its sound go to the PCM now?
    Play play(uint32_t nowMs) const {
        if (!inPrompt_) {
            return kWait;
        }
        if (route_ == kOn) {
            return kPlay;
        }
        if (route_ == kAsked) {
            return nowMs - askedAt_ < kGrantWaitMs ? kWait : kPlay;
        }
        return kFallBack;
    }

    // The prompt has played out, or went back to the mix: the route stays kHoldMs for the next one.
    void done(uint32_t nowMs) {
        if (!inPrompt_) {
            return;
        }
        inPrompt_ = false;
        doneAt_ = nowMs;
        held_ = true;
    }

    // kRemove once the route has been held kHoldMs after the last prompt.
    Action tick(uint32_t nowMs) {
        if (inPrompt_ || !held_ || route_ == kOff || nowMs - doneAt_ < kHoldMs) {
            return kNothing;
        }
        held_ = false;
        route_ = kOff;
        return kRemove;
    }

    // The session ended (or the service stops): kRemove when the route is asked for or on.
    Action stop() {
        inPrompt_ = false;
        held_ = false;
        if (route_ == kOff) {
            return kNothing;
        }
        route_ = kOff;
        return kRemove;
    }

    bool inPrompt() const { return inPrompt_; }
    bool routeOn() const { return route_ == kOn; }
    bool routeAsked() const { return route_ == kAsked; }
    bool backingOff() const { return backoff_; }

private:
    enum Route { kOff, kAsked, kOn };

    bool enabled_;
    Route route_;
    uint32_t askedAt_;
    uint32_t asks_;  // builds asked for so far
    bool inPrompt_;
    uint32_t doneAt_;
    bool held_;  // the last prompt is done and the route waits kHoldMs for another
    uint32_t backoffAt_;
    bool backoff_;
};

}  // namespace evo

#endif  // AA_ENTRYNAV_PLATFORM_PROMPT_PATH_HPP
