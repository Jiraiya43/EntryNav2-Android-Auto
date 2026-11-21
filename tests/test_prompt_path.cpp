// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Tests platform/prompt_path.hpp: where a prompt plays, and when its route is asked for and given back.
#include <stdio.h>

#include "prompt_path.hpp"

namespace {

int g_failures = 0;

#define CHECK(x)                                                \
    do {                                                        \
        if (!(x)) {                                             \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); \
            ++g_failures;                                       \
        }                                                       \
    } while (0)

using evo::PromptPath;

void offOrMusic() {
    PromptPath::Action a = PromptPath::kBuild;
    PromptPath off(false);
    CHECK(!off.start(false, 0, a) && a == PromptPath::kNothing);  // switched off: every prompt in the mix
    PromptPath on(true);
    CHECK(!on.start(true, 0, a) && a == PromptPath::kNothing);  // the phone's music plays: in the mix
    CHECK(!on.inPrompt() && on.stop() == PromptPath::kNothing);
}

void grantedThenHeld() {
    PromptPath p(true);
    PromptPath::Action a = PromptPath::kNothing;
    CHECK(p.start(false, 1000, a) && a == PromptPath::kBuild && p.routeAsked());
    const uint32_t ask = p.ask();
    CHECK(p.play(1000) == PromptPath::kWait);  // its sound waits for the car's answer
    CHECK(p.play(3999) == PromptPath::kWait);
    p.answered(ask, true, 1500);
    CHECK(p.routeOn() && p.play(1500) == PromptPath::kPlay);
    CHECK(p.tick(1600) == PromptPath::kNothing);  // still playing
    p.done(2000);
    CHECK(p.tick(2999) == PromptPath::kNothing);  // held for the next prompt
    // The next prompt within the hold: no new build, it plays at once.
    CHECK(p.start(false, 2500, a) && a == PromptPath::kNothing && p.play(2500) == PromptPath::kPlay);
    p.done(3000);
    CHECK(p.tick(3999) == PromptPath::kNothing);
    CHECK(p.tick(4000) == PromptPath::kRemove && !p.routeOn());
    CHECK(p.tick(5000) == PromptPath::kNothing);  // given back once
    // A prompt after that asks again.
    CHECK(p.start(false, 6000, a) && a == PromptPath::kBuild && p.ask() == ask + 1);
}

void slowCar() {
    PromptPath p(true);
    PromptPath::Action a = PromptPath::kNothing;
    CHECK(p.start(false, 0, a) && a == PromptPath::kBuild);
    const uint32_t ask = p.ask();
    // No answer within kGrantWaitMs: the prompt plays anyway rather than go to the music route.
    CHECK(p.play(PromptPath::kGrantWaitMs - 1) == PromptPath::kWait);
    CHECK(p.play(PromptPath::kGrantWaitMs) == PromptPath::kPlay);
    p.done(4000);
    // Asked but never answered: given back after the hold all the same.
    CHECK(p.tick(4000 + PromptPath::kHoldMs) == PromptPath::kRemove);
    // Its late answer is not taken for a later build's.
    CHECK(p.start(false, 6000, a) && a == PromptPath::kBuild);
    p.answered(ask, true, 6100);
    CHECK(!p.routeOn() && p.play(6100) == PromptPath::kWait);
    p.answered(p.ask(), true, 6200);
    CHECK(p.routeOn());
}

void refused() {
    PromptPath p(true);
    PromptPath::Action a = PromptPath::kNothing;
    CHECK(p.start(false, 0, a) && a == PromptPath::kBuild);
    p.answered(p.ask(), false, 800);
    CHECK(p.play(800) == PromptPath::kFallBack && p.backingOff());  // into the mix
    p.done(800);
    CHECK(p.tick(5000) == PromptPath::kNothing);                   // nothing to give back
    CHECK(!p.start(false, 1000, a) && a == PromptPath::kNothing);  // the next ones too, for a while
    CHECK(!p.start(false, 800 + PromptPath::kBackoffMs - 1, a));
    CHECK(p.start(false, 800 + PromptPath::kBackoffMs, a) && a == PromptPath::kBuild && !p.backingOff());
}

void sessionEnd() {
    PromptPath p(true);
    PromptPath::Action a = PromptPath::kNothing;
    CHECK(p.start(false, 0, a) && a == PromptPath::kBuild);
    CHECK(p.stop() == PromptPath::kRemove && !p.inPrompt());  // asked for: given back
    CHECK(p.stop() == PromptPath::kNothing);
    p.answered(p.ask(), true, 100);  // the answer to a build given back meanwhile
    CHECK(!p.routeOn());
    CHECK(p.start(false, 200, a) && a == PromptPath::kBuild);
    p.answered(p.ask(), true, 300);
    p.done(400);
    CHECK(p.stop() == PromptPath::kRemove && p.tick(5000) == PromptPath::kNothing);
}

}  // namespace

int main() {
    offOrMusic();
    grantedThenHeld();
    slowCar();
    refused();
    sessionEnd();
    if (g_failures) {
        return 1;
    }
    puts(
        "PASS prompt path: in the mix while the music plays, route asked at the start, waited for, held between "
        "prompts and given back, a slow car, a refusal and its pause, the session's end");
    return 0;
}
