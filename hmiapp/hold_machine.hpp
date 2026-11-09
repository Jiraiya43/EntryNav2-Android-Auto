// Copyright (c) 2026 Jiraiya43
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Keeps iDrive on the app's blank state while Android Auto is on screen ("hold"), and decides when it steps aside.
//
// Android Auto is drawn in its own layer above iDrive, but the knob and the keys still act on whatever iDrive shows
// underneath. While Android Auto is on screen, iDrive must therefore sit on state 13, an inert blank state of the app,
// the way native CarPlay holds its own. Event 52 asks for it, event 51 opens the app's menu, event 50 the iDrive main
// menu (hmiapp/EntryAad.xml). What the machine does, case by case:
// - Open row, entry button: "show" and event 52 at once, not at the next status poll.
// - Android Auto comes up by itself (start automatically, phone reconnected): over our menu, event 52; during a call,
//   event 52 over iDrive's call screen; over one of iDrive's own screens, where event 52 can be refused, our menu first
//   (event 51), whose focus asks for event 52 at once. Up to 3 menu tries a second apart, then event 52 once, then
//   Android Auto steps aside ("hide") rather than stay over a screen the knob also reaches.
// - BACK or tilt to our menu: event 52 at once (at most every 300 ms), and at each poll while the menu keeps the focus;
//   at most 8 events 52 until the blank state shows again.
// - Focus given back to the blank state: the hold worked, its tries start over (a focus lost and given back without
//   "visible" used them up for good, and the next BACK left the menu under Android Auto).
// - iDrive popup: the blank state stays visible but loses the focus. Android Auto steps aside for it ("popup 1", then
//   "popup 0" when it closes, hmiapp/popup_peek.hpp) however long it stays, and no event 52 is fired over it.
// - Driving mode change: iDrive keeps its own display over the blank state. Its focus loss is not fought for 15 s, then
//   event 52 every two polls without focus.
// - Hard key (MENU, MEDIA, NAV, COM, MAP, OPTION): iDrive left the blank state. Confirmed on a second poll within 3 s,
//   Android Auto steps aside ("hide"), the way MENU leaves native CarPlay; the Android Auto entry brings it back.
// - NAV, MEDIA or COM opened an Android Auto app (the service wrote its app key file within the last 2 s): iDrive
//   switched to its own screen underneath, event 52 at once so the knob does not act on that hidden screen.
// - Android Auto steps aside by itself (exit icon, signal lost) over the blank state: event 50 (iDrive main menu), or
//   event 51 when OPTION opened iDrive's option panel over it. Nothing in reverse or after a hard key.
// - Open row, but Android Auto never came up (session dropped): after 3 s on the blank state, event 51 (menu back).
// - iDrive back on the blank state by itself while Android Auto is minimized (BACK walks its history): "show", like
//   CarPlay.
// - Reverse gear: the camera wins. No popup peek, no "show", no hard key step aside, no app key hold.
// - Phone call: iDrive's call screen is not fought; when the call ends, event 52 within 10 s.
// - Service (re)started, or phone connected: it is told the call and the gear again (started in reverse, Android Auto
//   would come up over the camera).
//
// Pure: no BMW call, no file, no clock. Events and the time come in; iDrive events, control messages and log lines go
// out through HoldIo. Single-threaded (the app thread). tests/test_hold_machine.cpp.
#ifndef ENTRY_AAD_HMIAPP_HOLD_MACHINE_HPP
#define ENTRY_AAD_HMIAPP_HOLD_MACHINE_HPP

#include <time.h>

#include "popup_peek.hpp"
#include "service_state.hpp"

namespace evo {
namespace app {

const int kReverseGear = 2;      // driving.gear
const int kGearUnknown = -1000;  // no driving.gear value received yet

// What the machine does and reads: the app (iDrive, the service's control port, files, app.log), a recorder in tests.
class HoldIo {
public:
    virtual void triggerEvent(unsigned event) = 0;  // a focusEvent of EntryAad.xml: 50, 51 or 52
    virtual void send(const char* message) = 0;     // a control message to the service ("show", "hide", ...)
    virtual void log(const char* line) = 0;         // one line of the app's log
    // Read only when a decision needs them, at that moment: the service's state (reading it may ask its control port),
    // and the app key file the service writes when NAV / MEDIA / COM opened an Android Auto app.
    virtual ServiceState readService() = 0;
    virtual bool appKeyWritten(time_t* when) = 0;  // false: no app key file
    virtual void removeAppKey() = 0;

protected:
    // Never deleted through this interface: no virtual destructor, so the app's instance has nothing to run at exit
    // inside HmiMain3D.
    ~HoldIo() = default;
};

class HoldMachine {
public:
    static const int kMaxHoldTries = 8;      // events 52 until the blank state shows again
    static const int kMenuRouteTries = 3;    // events 51 to reach it through our menu
    static const int kDriveModeQuietS = 15;  // after a driving mode change, its display's focus loss is not fought
    static const int kAppKeyS = 2;           // an app key file this recent means NAV / MEDIA / COM was just pressed
    static const int kCallEndS = 10;         // a call that ended this recently takes iDrive back to the blank state
    static const long kMenuRefireMs = 300;   // our menu's focus fires event 52 at most this often

    explicit HoldMachine(HoldIo& io);

    // ---- the car (CDS properties) ----
    void callChanged(bool inCall, time_t now);  // after each update of either phone line
    void gearChanged(int gear);
    void driveModeChanged(time_t now);  // a real change between two valid driving modes

    // ---- the app's own requests ----
    void showAndHold(const char* why, time_t now);  // Open row, entry button: Android Auto to the front, blank at once
    void requestShow();  // Android Auto to the front; the overlay that follows is new and held at the next poll

    // ---- iDrive's focus on the app's states ----
    void aaFocus(bool focused);                                            // the blank state
    void aaVisible(bool visible, time_t now);                              // the blank state
    void menuFocus(unsigned state, bool focused, time_t now, long nowMs);  // the menu or Help; nowMs: monotonic

    // ---- the status poll, every second, with the service's state just read ----
    // First: tells a (re)started service, or one whose phone just connected, the call and the gear.
    void tellService(const ServiceState& service);
    // Then, once the menu rows are up to date: the hold itself.
    void poll(const ServiceState& service, time_t now);

    bool holding() const { return holding_; }
    // The blank state is shown: iDrive gives the focus to one of our states at a time, the last one to take it wins,
    // so a notification that never came for the other one cannot leave a stale flag behind.
    bool blankShown() const { return aaVisible_ && !menuFocused_; }
    bool popupPeek() const { return popup_.active(); }

private:
    void fireHold(const char* why, time_t now);
    void holdViaMenu(const char* why, time_t now);
    bool appKeyRecent(time_t now);
    bool keepForAppKey(const char* why, time_t now);
    void updatePopupPeek(bool covered, time_t now);
    void say(const char* fmt, ...) __attribute__((format(printf, 2, 3)));

    HoldIo& io_;
    PopupPeek popup_;

    // iDrive's focus
    bool menuFocused_;  // our menu (state 10) or its Help page (12) has the focus
    bool aaVisible_;    // the blank state is visible
    bool aaFocused_;    // the blank state has the focus

    // The hold
    bool holding_;        // Android Auto is (or is coming) on screen: iDrive is kept on the blank state
    bool hideSent_;       // the service was asked to step aside (hard key, or iDrive never showed the blank state)
    bool aaSeen_;         // the blank state was seen since the last event 51 or 52
    bool holdGaveUp_;     // Android Auto stepped aside because iDrive never showed the blank state
    bool prevOverlay_;    // Android Auto was on screen at the previous poll
    time_t holdTime_;     // last event 51 or 52
    time_t holdShownAt_;  // when showAndHold asked for the blank state before Android Auto confirmed
    int holdTries_;       // events 52 since the blank state last showed
    int menuRouteTries_;  // events 51 since the blank state last showed
    int unfocusedPolls_;  // polls with the blank state shown but unfocused
    time_t leftSeen_;     // first poll that saw iDrive away from the blank state (hard key), 0: none
    long menuHoldMs_;     // last event 52 fired by our menu's focus

    // The car
    bool inCall_;
    time_t callEnd_;  // when the last call ended, 0: handled
    int gear_;
    time_t driveModeAt_;  // last driving mode change, 0: none

    // The service, as told about the car
    bool serviceReachable_;
    char prevState_[24];
};

}  // namespace app
}  // namespace evo

#endif
