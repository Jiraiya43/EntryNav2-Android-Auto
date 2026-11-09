// Copyright (c) 2026 Jiraiya43
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// The picture's peeks for iDrive's popups (platform/peek_timing): dim, step aside, popup, hard key, clock wrap-around.
#include <stdio.h>

#include "peek_timing.hpp"

#define CHECK(c)                                                 \
    do {                                                         \
        if (!(c)) {                                              \
            fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #c); \
            return false;                                        \
        }                                                        \
    } while (0)

namespace {

// Android Auto on screen at t, nothing else running.
evo::PeekTiming shownAt(uint32_t t) {
    evo::PeekTiming p;
    p.turn(t, true);
    return p;
}

bool volumeDims(uint32_t t) {
    evo::PeekTiming off;
    CHECK(!off.volumeKey(t, false) && !off.dimUntil() && !off.hideUntil());  // not on screen: nothing
    evo::PeekTiming p = shownAt(t);
    CHECK(p.volumeKey(t, false) && p.dimUntil() == t + evo::kVolumePeekMs);
    CHECK(!p.volumeKey(t + 1000, false) && p.dimUntil() == t + 1000 + evo::kVolumePeekMs);  // extended, no new start
    evo::PeekTiming::Picture picture = p.turn(t + 1100, true);
    CHECK(picture.shown && picture.opacity == evo::kVolumeDim && p.visible());
    picture = p.turn(t + 1000 + evo::kVolumePeekMs, true);
    CHECK(picture.shown && picture.opacity == 1.f && !p.dimUntil());
    CHECK(p.volumeKey(t + 4000, false));  // a new window
    return true;
}

bool volumePill(uint32_t t) {
    evo::PeekTiming p = shownAt(t);
    CHECK(!p.volumeLevel(false) && p.levelSeen());  // a level, but no pill: the key still dims
    CHECK(p.volumeKey(t, false) && p.dimUntil());
    CHECK(p.volumeLevel(true) && !p.dimUntil());  // the pill takes over from the dim
    CHECK(!p.volumeKey(t + 10, true) && !p.dimUntil() && !p.hideUntil());
    p.turn(t + 20, false);
    CHECK(!p.volumeLevel(true));  // picture gone: no pill over iDrive
    evo::PeekTiming fresh = shownAt(t);
    CHECK(fresh.volumeKey(t, true));  // no level seen yet: dimmed even with the pill ready
    return true;
}

bool opacityRefused(uint32_t t) {
    evo::PeekTiming p = shownAt(t);
    CHECK(p.volumeKey(t, false));
    p.refuseOpacity(t + 1500, t + 100);  // the turn at t + 100 tried to dim; LayerManagerControl answered later
    CHECK(p.dimRefused() && !p.dimUntil() && p.hideFrom() == t + 100 && p.hideUntil() == t + 100 + evo::kVolumePeekMs);
    evo::PeekTiming::Picture picture = p.turn(t + 1600, true);
    CHECK(!picture.shown && picture.opacity == 1.f && p.onScreen() && !p.visible());
    CHECK(!p.volumeKey(t + 1700, false) && p.hideFrom() == t + 100 && p.hideUntil() == t + 1700 + evo::kVolumePeekMs);
    picture = p.turn(t + 1700 + evo::kVolumePeekMs, true);
    CHECK(picture.shown && !p.hideFrom() && !p.hideUntil());
    CHECK(p.volumeKey(t + 5000, false) && p.hideFrom() == t + 5000 && !p.dimUntil());  // steps aside from now on
    return true;
}

bool driveMode(uint32_t t) {
    evo::PeekTiming off;
    CHECK(!off.driveMode(t) && !off.hideUntil());
    evo::PeekTiming p = shownAt(t);
    CHECK(p.driveMode(t));
    CHECK(p.hideFrom() == t + evo::kDriveModeDelayMs);
    CHECK(p.hideUntil() == t + evo::kDriveModeDelayMs + evo::kDriveModeShowMs);
    CHECK(p.turn(t + evo::kDriveModeDelayMs - 1, true).shown);  // not yet
    CHECK(!p.turn(t + evo::kDriveModeDelayMs, true).shown);
    CHECK(p.onScreen());           // the iDrive app is still told Android Auto is on screen
    CHECK(p.driveMode(t + 1000));  // a second change extends the one in progress, from where it started
    CHECK(p.hideFrom() == t + evo::kDriveModeDelayMs);
    CHECK(p.hideUntil() == t + 1000 + evo::kDriveModeDelayMs + evo::kDriveModeShowMs);
    CHECK(p.turn(t + 1000 + evo::kDriveModeDelayMs + evo::kDriveModeShowMs, true).shown && !p.hideUntil());
    return true;
}

bool popup(uint32_t t) {
    evo::PeekTiming off;
    CHECK(off.popup(t, true) == evo::PeekTiming::kPopupIgnored && !off.popupOpen() && !off.hideUntil());
    evo::PeekTiming p = shownAt(t);
    CHECK(p.popup(t, false) == evo::PeekTiming::kPopupIgnored);
    CHECK(p.popup(t, true) == evo::PeekTiming::kPopupShown && p.popupOpen());
    CHECK(p.hideFrom() == t && p.hideUntil() == t + evo::kPopupShowMs);
    CHECK(!p.turn(t + 100, true).shown);
    CHECK(p.popup(t + 200, false) == evo::PeekTiming::kPopupGone && !p.popupOpen() && !p.hideUntil());
    CHECK(p.turn(t + 300, true).shown);
    CHECK(p.popup(t + 400, false) == evo::PeekTiming::kPopupIgnored);
    CHECK(p.popup(t + 500, true) == evo::PeekTiming::kPopupShown);
    CHECK(p.turn(t + 500 + evo::kPopupShowMs, true).shown && !p.hideUntil());  // never closed: 2 min at most
    CHECK(p.popupOpen());                                                      // until the iDrive app says it closed
    CHECK(p.popup(t + 600 + evo::kPopupShowMs, false) == evo::PeekTiming::kPopupGone);
    return true;
}

bool hardKeyAndScreen(uint32_t t) {
    evo::PeekTiming off;
    CHECK(!off.hardKey());
    evo::PeekTiming p = shownAt(t);
    p.volumeKey(t, false);
    p.driveMode(t);
    CHECK(p.hardKey() && !p.hideFrom() && !p.hideUntil() && !p.dimUntil());
    p.volumeKey(t, false);
    p.popup(t, true);
    evo::PeekTiming::Picture picture = p.turn(t + 10, false);  // camera, menu, session gone: every peek ends
    CHECK(!picture.shown && picture.opacity == 1.f && !p.onScreen() && !p.visible());
    CHECK(!p.hideFrom() && !p.hideUntil() && !p.dimUntil());
    return true;
}

// Every case, at an ordinary time and across the 32-bit wrap of the millisecond clock.
bool all(uint32_t t) {
    return volumeDims(t) && volumePill(t) && opacityRefused(t) && driveMode(t) && popup(t) && hardKeyAndScreen(t);
}

bool zeroMeansNone() {
    // A deadline landing exactly on 0 is kept as 1, never as "no peek".
    const uint32_t t = 0u - evo::kVolumePeekMs;
    evo::PeekTiming p = shownAt(t);
    CHECK(p.volumeKey(t, false) && p.dimUntil() == 1);
    CHECK(p.turn(0, true).opacity == evo::kVolumeDim);
    CHECK(p.turn(1, true).opacity == 1.f && !p.dimUntil());
    return true;
}

}  // namespace

int main() {
    if (!all(1000) || !all(0xFFFFF000u) || !all(0xFFFFFFFFu - evo::kDriveModeDelayMs) || !zeroMeansNone()) {
        return 1;
    }
    puts("PASS peeks: volume dim then pill, refused opacity steps aside, driving mode, popup, hard key, clock wrap");
    return 0;
}
