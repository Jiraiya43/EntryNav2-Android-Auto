// Copyright (c) 2026 Jiraiya43
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// When the Android Auto picture steps aside or dims so that iDrive's popups show: pure timing, no clock, no lock.

// iDrive draws its popups (volume bar, driving mode, alerts) under Android Auto. During a peek the iDrive app is still
// told "Android Auto on screen", so it does not send iDrive to its main menu.
//  - volume: the pill over Android Auto once the car has reported a level; before that, or without the pill, the
//    picture is dimmed (kVolumeDim for kVolumePeekMs) so iDrive's own bar shows through. When the compositor refuses
//    the opacity, the picture steps aside instead.
//  - driving mode change (iDrive app, CDS driving.mode/mode35up): picture stepped aside kDriveModeShowMs after
//    kDriveModeDelayMs.
//  - iDrive popup (iDrive app, focus lost on our blank state): picture stepped aside kPopupShowMs at most, or until
//    the popup closes. Inputs go back to iDrive meanwhile, so the driver can answer it.
// Times are monotonic milliseconds (evo::monoMs), compared with wrap-around; 0 means "none".
// tests/test_peek_timing.cpp.
#ifndef ENTRY_AAD_PLATFORM_PEEK_TIMING_HPP
#define ENTRY_AAD_PLATFORM_PEEK_TIMING_HPP

#include <stdint.h>

namespace evo {

const unsigned kVolumePeekMs = 2500;     // volume without the pill: picture dimmed
const float kVolumeDim = 0.25f;          // its opacity
const unsigned kDriveModeShowMs = 3500;  // driving mode change: picture stepped aside...
const unsigned kDriveModeDelayMs = 400;  // ...after this delay
const unsigned kPopupShowMs = 120000;    // iDrive popup over the blank state: until it closes, at most this

class PeekTiming {
public:
    enum Popup { kPopupIgnored, kPopupShown, kPopupGone };
    // What one turn of the main loop shows.
    struct Picture {
        bool shown;     // the picture is on screen, not stepped aside
        float opacity;  // kVolumeDim while dimmed, 1 otherwise
    };

    PeekTiming();

    // Volume key (knob or steering wheel). pillReady: the pill can show a level. Once the car has reported a level and
    // the pill is ready, the level message that follows shows it and nothing happens here. True when a peek window
    // starts (none was running).
    bool volumeKey(uint32_t now, bool pillReady);
    // A level reported by the car. True when the pill shows it now (the picture is visible and the pill ready): a dim
    // started by the key just before gives way to it.
    bool volumeLevel(bool pillReady);
    // A hard key iDrive acts on: every peek ends. False when Android Auto was not on screen (nothing to do).
    bool hardKey();
    // Driving mode change: the picture steps aside after a delay. False when Android Auto is not on screen.
    bool driveMode(uint32_t now);
    // iDrive popup opened (open) or closed.
    Popup popup(uint32_t now, bool open);
    // The compositor refused an opacity: from now on the volume steps the picture aside, starting at from (the turn
    // that tried to dim).
    void refuseOpacity(uint32_t now, uint32_t from);
    // One turn of the main loop. onScreen: Android Auto is what the iDrive app is told is on screen (session up, focus,
    // no reverse gear). Ends the expired peeks, all of them when Android Auto is not on screen.
    Picture turn(uint32_t now, bool onScreen);

    bool onScreen() const { return shownNow_; }      // as of the last turn, peeks included
    bool visible() const { return visibleNow_; }     // as of the last turn, not in a peek either
    bool levelSeen() const { return levelSeen_; }    // the car reported a volume level
    bool dimRefused() const { return dimRefused_; }  // the compositor refused the opacity
    bool popupOpen() const { return popupHide_; }
    uint32_t hideFrom() const { return hideFrom_; }
    uint32_t hideUntil() const { return hideUntil_; }
    uint32_t dimUntil() const { return dimUntil_; }

private:
    static uint32_t nz(uint32_t t) { return t ? t : 1; }  // 0 means "no peek"
    // Hide the picture from `from` for ms (extends a hide in progress at now).
    void hide(uint32_t now, uint32_t from, unsigned ms);

    uint32_t hideFrom_, hideUntil_, dimUntil_;
    bool dimRefused_;
    bool levelSeen_;
    bool popupHide_;
    bool shownNow_;    // Android Auto on screen as the iDrive app is told (peeks included)
    bool visibleNow_;  // its picture actually shown: not in a peek either (the pill only goes over it)
};

}  // namespace evo

#endif
