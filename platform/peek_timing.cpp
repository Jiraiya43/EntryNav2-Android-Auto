// Copyright (c) 2026 Jiraiya43
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// When the Android Auto picture steps aside or dims for iDrive's popups (see peek_timing.hpp).
#include "peek_timing.hpp"

namespace evo {

PeekTiming::PeekTiming(bool volumeKeyRequired)
    : hideFrom_(0),
      hideUntil_(0),
      dimUntil_(0),
      volumeKeyRequired_(volumeKeyRequired),
      volumeKeyUntil_(0),
      dimRefused_(false),
      levelSeen_(false),
      popupHide_(false),
      shownNow_(false),
      visibleNow_(false) {}

void PeekTiming::hide(uint32_t now, uint32_t from, unsigned ms) {
    const bool hiding =
        hideFrom_ && static_cast<int32_t>(now - hideFrom_) >= 0 && static_cast<int32_t>(hideUntil_ - now) > 0;
    if (!hiding) {
        hideFrom_ = nz(from);
    }
    const uint32_t until = nz(from + ms);
    if (!hideUntil_ || static_cast<int32_t>(until - hideUntil_) > 0) {
        hideUntil_ = until;
    }
}

bool PeekTiming::volumeKey(uint32_t now, bool pillReady) {
    if (!shownNow_) {
        return false;
    }
    if (volumeKeyRequired_) {
        volumeKeyUntil_ = nz(now + 500);
        if (volumeActivity(pillReady)) {
            return false;
        }
    } else if (levelSeen_ && pillReady) {
        return false;
    }
    if (!dimRefused_) {
        const bool start = !dimUntil_;
        dimUntil_ = nz(now + kVolumePeekMs);
        return start;
    }
    const bool start = !hideUntil_;
    hide(now, now, kVolumePeekMs);
    return start;
}

bool PeekTiming::volumeLevel(bool pillReady, uint32_t now) {
    levelSeen_ = true;
    if (!visibleNow_ || !pillReady) {
        return false;
    }
    // In USB mode BMW also sends unchanged levels after touch input. Only show a level after a real volume key.
    if (volumeKeyRequired_ &&
        (!volumeKeyUntil_ || static_cast<int32_t>(volumeKeyUntil_ - now) <= 0)) {
        return false;
    }
    volumeKeyUntil_ = 0;
    dimUntil_ = 0;
    return true;
}

bool PeekTiming::hardKey() {
    if (!shownNow_) {
        return false;
    }
    hideFrom_ = hideUntil_ = dimUntil_ = volumeKeyUntil_ = 0;
    return true;
}

bool PeekTiming::driveMode(uint32_t now) {
    if (!shownNow_) {
        return false;
    }
    hide(now, now + kDriveModeDelayMs, kDriveModeShowMs);
    return true;
}

PeekTiming::Popup PeekTiming::popup(uint32_t now, bool open) {
    if (open && shownNow_) {
        hide(now, now, kPopupShowMs);
        popupHide_ = true;
        return kPopupShown;
    }
    if (!open && popupHide_) {
        popupHide_ = false;
        hideFrom_ = hideUntil_ = 0;
        return kPopupGone;
    }
    return kPopupIgnored;
}

void PeekTiming::refuseOpacity(uint32_t now, uint32_t from) {
    dimRefused_ = true;
    dimUntil_ = 0;
    hide(now, from, kVolumePeekMs);
}

PeekTiming::Picture PeekTiming::turn(uint32_t now, bool onScreen) {
    const bool hiding =
        hideFrom_ && static_cast<int32_t>(now - hideFrom_) >= 0 && static_cast<int32_t>(hideUntil_ - now) > 0;
    if (hideUntil_ && static_cast<int32_t>(hideUntil_ - now) <= 0) {
        hideFrom_ = hideUntil_ = 0;
    }
    const bool dimming = dimUntil_ && static_cast<int32_t>(dimUntil_ - now) > 0;
    if (dimUntil_ && !dimming) {
        dimUntil_ = 0;
    }
    if (!onScreen) {
        hideFrom_ = hideUntil_ = dimUntil_ = volumeKeyUntil_ = 0;
    }
    Picture picture;
    picture.shown = onScreen && !hiding;
    picture.opacity = picture.shown && dimming ? kVolumeDim : 1.f;
    shownNow_ = onScreen;
    visibleNow_ = picture.shown;
    return picture;
}

}  // namespace evo
