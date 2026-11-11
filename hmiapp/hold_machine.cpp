// Copyright (c) 2026 Jiraiya43
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// The hold's decisions (hmiapp/hold_machine.hpp): which iDrive event, which control message, at which event or poll.
#include "hold_machine.hpp"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "rhmi_ids.hpp"

namespace evo {
namespace app {

HoldMachine::HoldMachine(HoldIo& io)
    : io_(io),
      menuFocused_(false),
      aaVisible_(false),
      aaFocused_(false),
      holding_(false),
      hideSent_(false),
      aaSeen_(false),
      holdGaveUp_(false),
      prevOverlay_(false),
      holdTime_(0),
      holdShownAt_(0),
      holdTries_(0),
      menuRouteTries_(0),
      unfocusedPolls_(0),
      leftSeen_(0),
      menuHoldMs_(-1000),
      waiting_(false),
      unheld_(false),
      popupOpen_(false),
      underPopup_(false),
      waitSince_(0),
      askedAt_(0),
      asks_(0),
      inCall_(false),
      callEnd_(0),
      gear_(kGearUnknown),
      driveModeAt_(0),
      serviceReachable_(false) {
    memset(prevState_, 0, sizeof(prevState_));
    strcpy(prevState_, "idle");
}

void HoldMachine::say(const char* fmt, ...) {
    char line[384];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);
    io_.log(line);
}

// ---- the car --------------------------------------------------------------------------------------------------------
void HoldMachine::callChanged(bool inCall, long now) {
    if (inCall_ && !inCall) {
        callEnd_ = now;
    }
    inCall_ = inCall;
}

void HoldMachine::gearChanged(int gear) {
    gear_ = gear;
}

void HoldMachine::driveModeChanged(long now) {
    driveModeAt_ = now;
}

// ---- events 52 and 51 -----------------------------------------------------------------------------------------------
void HoldMachine::fireHold(const char* why, long now) {
    if (holdTries_ >= kMaxHoldTries) {
        say("hold: not entering state %u (%s): already tried %d times", kAaState, why, holdTries_);
        return;
    }
    ++holdTries_;
    aaSeen_ = false;
    holdTime_ = now;
    io_.triggerEvent(kEvHoldAa);
    say("hold: event %u -> state %u (%s, try %d)", kEvHoldAa, kAaState, why, holdTries_);
}

// Event 52 works from our own states (menu, Help, the blank state). Over one of iDrive's own screens it can be
// refused: Android Auto that comes up by itself (start automatically, phone reconnected) then either stays over that
// screen with the knob reaching both, or steps aside after three refusals. Both were seen on the car, and the same is
// reported on NBT Evo after an automatic start. What the driver does by hand works: our menu first (event 51), whose
// focus asks for the blank state at once (menuFocus).
void HoldMachine::holdViaMenu(const char* why, long now) {
    ++menuRouteTries_;
    aaSeen_ = false;
    holdTime_ = now;
    io_.triggerEvent(kEvOpenMenu);
    say("hold: event %u -> our menu, then state %u (%s, try %d)", kEvOpenMenu, kAaState, why, menuRouteTries_);
}

// ---- the app's own requests -----------------------------------------------------------------------------------------
// Android Auto comes to the front: iDrive goes to the blank state at once, not at the next status poll, so the knob
// never reaches the screen underneath (entry button and the menu's Open row).
void HoldMachine::showAndHold(const char* why, long now) {
    waiting_ = false;
    unheld_ = false;
    if (popupOpen_) {
        popupOpen_ = false;
        io_.ownPopup(false);  // our menu opened by itself meanwhile
    }
    io_.send("show");
    prevOverlay_ = false;  // the coming overlay is new
    holding_ = true;
    hideSent_ = false;
    holdTries_ = 0;
    holdShownAt_ = now;
    fireHold(why, now);
}

void HoldMachine::requestShow() {
    waiting_ = false;
    unheld_ = false;
    io_.send("show");
    prevOverlay_ = false;  // the coming overlay is new: hold iDrive on the blank state
}

// ---- NAV / MEDIA / COM keys -----------------------------------------------------------------------------------------
// NAV / MEDIA / COM / steering wheel phone opened an Android Auto app (service file, written within the last 2 s):
// iDrive switched to its own navigation / media / phone screen underneath Android Auto; put it back on our blank
// state at once, so the knob does not act on that hidden screen.
bool HoldMachine::appKeyRecent() {
    long age = 0;
    return io_.appKeyAge(&age) && age <= kAppKeyS;
}

bool HoldMachine::keepForAppKey(const char* why, long now) {
    if (!appKeyRecent() || inCall_ || gear_ == kReverseGear) {
        return false;
    }
    const ServiceState d = io_.readService();
    if (!d.onScreen()) {
        return false;
    }
    io_.removeAppKey();
    holdTries_ = 0;
    fireHold(why, now);
    return true;
}

// ---- iDrive's focus -------------------------------------------------------------------------------------------------
void HoldMachine::aaFocus(bool focused) {
    aaFocused_ = focused;
    if (focused) {
        // The blank state has the focus: it is shown, whatever visibility notification was or was not sent. The hold
        // worked, so its tries start over (a focus lost and given back without "visible" used them up for good, and
        // the next BACK then left the menu under Android Auto for the rest of the session).
        menuFocused_ = false;
        aaVisible_ = true;
        aaSeen_ = true;
        holdTries_ = 0;
        menuRouteTries_ = 0;
        holdGaveUp_ = false;
        underPopup_ = false;
    }
    say("hold: state %u focus -> %d", kAaState, static_cast<int>(focused));
}

void HoldMachine::aaVisible(bool visible, long now) {
    aaVisible_ = visible;
    if (visible) {
        aaSeen_ = true;
        holdTries_ = 0;
        menuRouteTries_ = 0;
    }
    say("hold: state %u visible -> %d", kAaState, static_cast<int>(visible));
    if (!visible && keepForAppKey("key opened an Android Auto app", now)) {
        return;
    }
    // iDrive came back to our blank state by itself while Android Auto is minimized (BACK walks its history): bring
    // Android Auto to the front, like CarPlay. Not in reverse or during a call.
    if (visible && !holding_ && !inCall_ && gear_ != kReverseGear) {
        const ServiceState d = io_.readService();
        if (d.pid > 0 && !d.onScreen() && d.is("connected")) {
            say("hold: iDrive went back to the Android Auto screen: asking the service to come to the front");
            requestShow();
        }
    }
}

void HoldMachine::menuFocus(unsigned state, bool focused, long now, long nowMs) {
    menuFocused_ = focused;
    if (focused) {
        aaFocused_ = false;
    }
    say("menu state %u focus -> %d", state, static_cast<int>(focused));
    if (focused && unheld_ && !waiting_) {
        say("hold: iDrive opened our menu after %d request(s): taking the hold", asks_);
    }
    if (focused && waiting_ && !inCall_ && gear_ != kReverseGear) {
        // Our menu has the focus: iDrive performs our events. Android Auto was waiting for exactly that.
        say("hold: our menu is open after %ld s and %d request(s): Android Auto comes up", now - waitSince_, asks_);
        showAndHold("our menu opened for Android Auto", now);
        return;
    }
    // BACK / tilt-left pop iDrive from the blank state to our menu, or a stray push opened Help under Android Auto:
    // put it straight back (the knob goes to the phone).
    if (focused && holding_ && !inCall_) {
        const ServiceState d = io_.readService();
        if (d.onScreen() && nowMs - menuHoldMs_ >= kMenuRefireMs) {
            menuHoldMs_ = nowMs;
            fireHold("BACK/tilt popped to our menu (immediate)", now);
        }
    }
}

// ---- the status poll ------------------------------------------------------------------------------------------------
// A (re)started service knows neither the call nor the gear: started in reverse, Android Auto would come up over the
// camera. Both are sent when the service becomes reachable, and again when the phone connects.
void HoldMachine::tellService(const ServiceState& service) {
    const bool reachable = service.pid > 0;
    if (reachable && !serviceReachable_) {
        // From now on a phone that connects waits aside for our "show": iDrive is brought to our pages first. A
        // service that does not know the word shows Android Auto by itself, as before.
        io_.send("await");
    }
    if ((reachable && !serviceReachable_) || (service.is("connected") && strcmp(prevState_, "connected"))) {
        io_.send(inCall_ ? "call 1" : "call 0");
        if (gear_ != kGearUnknown) {
            char msg[32];
            snprintf(msg, sizeof(msg), "gear %d", gear_);
            io_.send(msg);
        }
    }
    serviceReachable_ = reachable;
    strncpy(prevState_, service.state, sizeof(prevState_) - 1);
}

// iDrive popup over our blank state: the service steps aside so the driver sees it (hmiapp/popup_peek.hpp).
void HoldMachine::updatePopupPeek(bool covered, long now) {
    const PopupPeek::Action a = popup_.step(covered, now, driveModeAt_);
    if (a == PopupPeek::kStart) {
        io_.send("popup 1");
        say("popup: iDrive took the focus over state %u (gear %d): Android Auto picture removed", kAaState, gear_);
    } else if (a == PopupPeek::kEnd) {
        io_.send("popup 0");
        say("popup: focus given back: Android Auto picture back");
    }
}

// ---- Android Auto and an iDrive that refuses our pages -------------------------------------------------------------
// HmiMain3D performs no focus event of an app while one of its own popups is the topmost one
// (CFocusEvent::isCurrentlyAllowed: neither a remote state nor CarPlay's; the event returns 215 and nothing else
// happens, it is not even counted). On the car, opened but not started, that lasted until a key was pressed. The
// answer never reaches the app (CLocalCommAdapter::sendEvent drops it) and nothing tells an app when it ends, so the
// only way to know is to ask: our menu (event 51) once a second. Its focus is the answer (menuFocus).
void HoldMachine::beginWait(long now) {
    waiting_ = true;
    waitSince_ = now;
    askedAt_ = 0;
    asks_ = 0;
}

// The service keeps Android Auto aside for us: iDrive on our pages first, when it lets us.
void HoldMachine::bringIdrive(const ServiceState& service, long now) {
    if (service.onScreen() || service.pid <= 0 || !service.is("connected") || !service.asks()) {
        waiting_ = false;  // shown another way, the session is over, or the service no longer waits
        if (popupOpen_) {
            popupOpen_ = false;
            io_.ownPopup(false);
        }
        return;
    }
    if (inCall_ || gear_ == kReverseGear) {
        return;  // iDrive's call screen or the camera: not now
    }
    if (menuFocused_ || aaFocused_) {
        say("hold: one of our pages has the focus: Android Auto comes up");
        showAndHold("Android Auto was waiting aside", now);
        return;
    }
    if (askedAt_ && now - askedAt_ < 1) {
        return;
    }
    if (asks_ == 1) {
        // Refused, and iDrive may refuse until a key is pressed. With a popup of ours on top it performs our
        // focus events: opened now, used at the next poll.
        askedAt_ = now;
        ++asks_;
        popupOpen_ = true;
        io_.ownPopup(true);
        say("hold: iDrive refused our menu: our own popup opened (event %u) to ask under it", kEvOwnPopup);
        return;
    }
    if (asks_ >= 2) {
        io_.triggerEvent(kEvHoldAa);
        io_.ownPopup(false);
        popupOpen_ = false;
        underPopup_ = true;
        askedAt_ = now;
        waiting_ = false;
        unheld_ = true;
        holding_ = true;
        hideSent_ = false;
        holdGaveUp_ = false;
        prevOverlay_ = false;
        io_.send("show");
        say("hold: event %u -> state %u asked under our popup, popup closed: Android Auto comes up after %ld s",
            kEvHoldAa, kAaState, now - waitSince_);
        return;
    }
    askedAt_ = now;
    ++asks_;
    io_.triggerEvent(kEvOpenMenu);
    if (asks_ == 1) {
        say("hold: Android Auto waits aside: event %u -> our menu", kEvOpenMenu);
    }
}

// Android Auto is on screen over an iDrive screen that refused our pages: asked again, once a second.
void HoldMachine::askWithoutHold(long now) {
    if (inCall_ || gear_ == kReverseGear || (askedAt_ && now - askedAt_ < 1)) {
        return;
    }
    askedAt_ = now;
    ++asks_;
    if (menuFocused_) {
        holdTries_ = 0;
        fireHold("our menu opened under Android Auto", now);
        return;
    }
    io_.triggerEvent(kEvOpenMenu);
}

// Status poll (1 s). service.onScreen(): Android Auto is on screen (knob and touch go to the phone).
void HoldMachine::poll(const ServiceState& service, long now) {
    const bool overlay = service.onScreen();
    const bool rose = overlay && !prevOverlay_, fell = !overlay && prevOverlay_;
    prevOverlay_ = overlay;
    const bool blank = blankShown();
    // Covered: the blank state is still shown but none of our states has the focus (an iDrive popup over it).
    // Not while the blank state, taken under our own popup, has not had the focus yet: what refused our pages is
    // still on top of it, and it is not a popup the driver can see.
    updatePopupPeek(!rose && !fell && overlay && holding_ && blank && !aaFocused_ && !inCall_ &&
                        gear_ != kReverseGear && !underPopup_,
                    now);
    if (rose) {
        waiting_ = false;
        if (unheld_) {
            holding_ = true;  // shown without the hold: our menu goes on being asked for below, from the next poll
            return;
        }
        holdTries_ = 0;
        menuRouteTries_ = 0;
        hideSent_ = false;
        holdGaveUp_ = false;
        holding_ = true;
        // The blank state shown but unfocused is an iDrive popup: the peek handles it, no event over it.
        if (menuFocused_) {
            fireHold("Android Auto came on screen over our menu", now);
        } else if (!aaVisible_ && inCall_) {
            fireHold("Android Auto came on screen during a call", now);
        } else if (!aaVisible_) {
            holdViaMenu("Android Auto came on screen over an iDrive screen", now);
        }
        return;
    }
    if (fell) {
        // Android Auto stepped aside by itself (phone's exit icon, signal lost) while iDrive shows our blank state:
        // give the driver the iDrive main menu. Not in reverse, not when a hard key already took iDrive elsewhere.
        if (blank && !aaFocused_ && gear_ != kReverseGear) {
            // OPTION while Android Auto is on screen (the service stepped aside, iDrive opened its option panel over
            // our blank state): the app's menu (Open / Stop / start automatically).
            io_.triggerEvent(kEvOpenMenu);
            say("hold: OPTION: event %u -> Android Auto menu", kEvOpenMenu);
        } else if (blank && !hideSent_ && gear_ != kReverseGear) {
            io_.triggerEvent(kEvGoHome);
            say("hold: Android Auto stepped aside: event %u -> iDrive main menu", kEvGoHome);
        } else {
            say("hold: Android Auto stepped aside (state %u visible %d, hard key %d, gear %d)", kAaState,
                static_cast<int>(aaVisible_), static_cast<int>(hideSent_), gear_);
        }
        holding_ = false;
        unheld_ = false;
        underPopup_ = false;
        return;
    }
    if (unheld_) {
        if (service.pid <= 0 || !service.is("connected")) {
            unheld_ = false;  // the session is over
        } else if (blank || aaFocused_) {
            unheld_ = false;
            say("hold: iDrive is on our blank state after %d request(s): Android Auto is held", asks_);
        } else {
            if (overlay) {
                askWithoutHold(now);
            }
            return;
        }
    }
    if (service.asks() && !waiting_) {
        beginWait(now);
    }
    if (waiting_) {
        bringIdrive(service, now);
        return;
    }
    if (blank) {
        callEnd_ = 0;
    }
    if (overlay && holding_ && blank && !aaFocused_ && !inCall_) {
        if (underPopup_) {
            return;  // shown, and the focus still with what refused our pages: held, nothing to ask again
        }
        if (popup_.active() || (driveModeAt_ && now - driveModeAt_ < kDriveModeQuietS)) {
            return;  // an iDrive popup or the driving mode display: Android Auto steps aside, no event over it
        }
        if (gear_ == kReverseGear) {
            unfocusedPolls_ = 0;
            return;  // the camera took the focus before Android Auto stepped aside: no event over it
        }
        if (++unfocusedPolls_ >= 2 && now - holdTime_ >= 1) {
            unfocusedPolls_ = 0;
            fireHold("state lost focus", now);
        }
        return;
    }
    unfocusedPolls_ = 0;
    // Event 52 fired for the Open row or the entry button, but Android Auto never came up (session dropped meanwhile):
    // give the driver the menu back instead of a blank screen.
    if (!overlay && holding_ && blank && holdShownAt_ && now - holdShownAt_ >= 3) {
        holdShownAt_ = 0;
        holding_ = false;
        io_.triggerEvent(kEvOpenMenu);
        say("hold: Android Auto did not come up after the blank state: event %u -> menu", kEvOpenMenu);
        return;
    }
    if (overlay) {
        holdShownAt_ = 0;
    }
    if (!overlay || !holding_ || blank) {
        return;
    }
    if (now - holdTime_ < 1) {
        return;
    }
    if (menuFocused_) {
        fireHold("BACK returned to our menu", now);
        return;
    }
    if (inCall_) {
        aaSeen_ = true;
        return;
    }
    if (callEnd_ && now - callEnd_ < kCallEndS) {
        callEnd_ = 0;
        fireHold("call ended", now);
        return;
    }
    if (!aaSeen_) {
        // Still on an iDrive screen a second after asking: through our menu (holdViaMenu), then event 52 once if the
        // menu never opened.
        if (menuRouteTries_ < kMenuRouteTries) {
            holdViaMenu("state not shown over an iDrive screen", now);
        } else if (holdTries_ == 0) {
            fireHold("our menu did not open: state directly", now);
        } else if (!holdGaveUp_) {
            // iDrive may refuse until a key is pressed: Android Auto stays, and our menu is asked for every second.
            holdGaveUp_ = true;
            unheld_ = true;
            askedAt_ = now;
            asks_ = 0;
            say("hold: iDrive did not show state %u, even through our menu: Android Auto stays, without the hold",
                kAaState);
        }
        return;
    }
    // A hard key (MENU / MEDIA / NAV / COM / MAP / OPTION) took iDrive elsewhere, the way MENU leaves native CarPlay:
    // Android Auto steps aside; the Android Auto entry brings it back. Confirmed on a second poll.
    if (gear_ == kReverseGear) {
        return;
    }
    if (keepForAppKey("key opened an Android Auto app (poll)", now)) {
        return;
    }
    if (!leftSeen_ || now - leftSeen_ > 3) {
        leftSeen_ = now;
        return;
    }
    leftSeen_ = 0;
    hideSent_ = true;
    holding_ = false;
    io_.send("hide");
    say("hold: iDrive left state %u (hard key): asked the service to step aside", kAaState);
}

}  // namespace app
}  // namespace evo
