// Copyright (c) 2026 Jiraiya43
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// hmiapp/hold_machine.cpp: which iDrive event and which control message, scenario by scenario, against a recorder.
#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

#include "../hmiapp/hold_machine.hpp"
#include "../hmiapp/rhmi_ids.hpp"

namespace {

using evo::app::HoldIo;
using evo::app::HoldMachine;
using evo::app::kAaState;
using evo::app::kHelpState;
using evo::app::kMenuState;
using evo::app::ServiceState;

int g_failures = 0;

#define CHECK(x)                                                \
    do {                                                        \
        if (!(x)) {                                             \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); \
            ++g_failures;                                       \
        }                                                       \
    } while (0)

// What the machine did since the last check, in order: "event 52", control messages as sent, "app key removed".
#define CHECK_OUT(car, expected)                                                                               \
    do {                                                                                                       \
        const std::string got_ = (car).take();                                                                 \
        if (got_ != (expected)) {                                                                              \
            printf("FAIL %s:%d: did \"%s\", expected \"%s\"\n", __FILE__, __LINE__, got_.c_str(), (expected)); \
            ++g_failures;                                                                                      \
        }                                                                                                      \
    } while (0)

class Recorder : public HoldIo {
public:
    Recorder() : appKey(false), appKeyAt(0), reads(0) {}
    void triggerEvent(unsigned event) override {
        char text[16];
        snprintf(text, sizeof(text), "event %u", event);
        out.push_back(text);
    }
    void send(const char* message) override { out.push_back(message); }
    void log(const char* line) override { logs.push_back(line); }
    ServiceState readService() override {
        ++reads;
        return service;
    }
    bool appKeyWritten(time_t* when) override {
        if (!appKey) {
            return false;
        }
        *when = appKeyAt;
        return true;
    }
    void removeAppKey() override {
        appKey = false;
        out.push_back("app key removed");
    }

    std::vector<std::string> out;
    std::vector<std::string> logs;
    ServiceState service;
    bool appKey;
    time_t appKeyAt;
    int reads;
};

// The machine in the car: its clocks, the service, iDrive's notifications.
struct Car {
    Recorder io;
    HoldMachine hold;
    time_t now;  // wall clock, seconds
    long ms;     // monotonic clock, milliseconds

    Car() : hold(io), now(1000), ms(50000) {}

    // The status poll, a second after the previous one, with the service as it is now.
    void poll() {
        now += 1;
        ms += 1000;
        hold.tellService(io.service);
        hold.poll(io.service, now);
    }
    void polls(int n) {
        for (int i = 0; i < n; ++i) {
            poll();
        }
    }
    void service(const char* state, int pid, int overlay) { io.service = ServiceState(state, pid, overlay); }
    void overlay(int on) { io.service.overlay = on; }

    // iDrive's notifications.
    void menu(bool focused) { hold.menuFocus(kMenuState, focused, now, ms); }
    void blankFocus(bool focused) { hold.aaFocus(focused); }
    void blankVisible(bool visible) { hold.aaVisible(visible, now); }
    // iDrive on the blank state: the menu loses the focus, the blank state takes it and shows.
    void blankShown() {
        menu(false);
        blankFocus(true);
        blankVisible(true);
    }
    // iDrive on one of its own screens: the blank state loses the focus and hides.
    void blankLeft() {
        blankFocus(false);
        blankVisible(false);
    }
    void call(bool on) { hold.callChanged(on, now); }

    std::string take() {
        std::string all;
        for (size_t i = 0; i < io.out.size(); ++i) {
            all += (i ? ", " : "") + io.out[i];
        }
        io.out.clear();
        return all;
    }
    bool logged(const char* text) const {
        for (size_t i = 0; i < io.logs.size(); ++i) {
            if (strstr(io.logs[i].c_str(), text)) {
                return true;
            }
        }
        return false;
    }
};

// A connected phone, gear 1, nothing on screen yet; the service already told the call and the gear.
void connected(Car& c) {
    c.hold.gearChanged(1);
    c.service("connected", 42, 0);
    c.poll();
    CHECK_OUT(c, "call 0, gear 1");
}

// Android Auto on screen over the blank state, which has the focus: opened from the menu's Open row.
void heldOnBlank(Car& c) {
    connected(c);
    c.menu(true);
    c.hold.showAndHold("Open row", c.now);
    c.blankShown();
    c.overlay(1);
    c.poll();
    CHECK_OUT(c, "show, event 52");
    CHECK(c.hold.holding() && c.hold.blankShown());
    c.io.logs.clear();
}

void manualOpen() {
    Car c;
    connected(c);
    c.menu(true);
    CHECK_OUT(c, "");  // the menu's focus alone does nothing while nothing is held
    CHECK(c.io.reads == 0);
    c.hold.showAndHold("Open row", c.now);
    CHECK_OUT(c, "show, event 52");  // at once, not at the next poll
    CHECK(c.logged("hold: event 52 -> state 13 (Open row, try 1)"));
    c.blankShown();
    CHECK_OUT(c, "");
    c.overlay(1);
    c.polls(10);
    CHECK_OUT(c, "");  // held: nothing more to do
    CHECK(c.hold.holding() && c.hold.blankShown());
}

void openButNeverCame() {
    Car c;
    connected(c);
    c.menu(true);
    c.hold.showAndHold("entry button", c.now);
    c.blankShown();
    CHECK_OUT(c, "show, event 52");
    c.polls(2);
    CHECK_OUT(c, "");  // the session dropped meanwhile: no overlay, wait 3 s
    c.poll();
    CHECK_OUT(c, "event 51");  // the menu back instead of a blank screen
    CHECK(!c.hold.holding());
    c.polls(3);
    CHECK_OUT(c, "");
}

void automaticStartOverIdriveScreen() {
    Car c;
    connected(c);  // iDrive shows one of its own screens: none of our states
    c.overlay(1);  // Android Auto came up by itself
    c.poll();
    CHECK_OUT(c, "event 51");  // our menu first: event 52 can be refused over iDrive's screens
    CHECK(c.logged("Android Auto came on screen over an iDrive screen, try 1"));
    c.menu(true);
    CHECK_OUT(c, "event 52");  // the menu's focus asks for the blank state at once
    CHECK(c.io.reads == 1);
    c.blankShown();
    c.polls(5);
    CHECK_OUT(c, "");
    CHECK(c.hold.holding() && c.hold.blankShown());
}

void automaticStartRefused() {
    {  // the menu never opens: three menu tries a second apart, event 52 once, then Android Auto steps aside
        Car c;
        connected(c);
        c.overlay(1);
        c.poll();
        CHECK_OUT(c, "event 51");
        c.poll();
        CHECK_OUT(c, "event 51");
        c.poll();
        CHECK_OUT(c, "event 51");
        c.poll();
        CHECK_OUT(c, "event 52");
        CHECK(c.logged("our menu did not open: state directly"));
        c.poll();
        CHECK_OUT(c, "hide");  // never over an iDrive screen that would get the knob too
        CHECK(!c.hold.holding());
        CHECK(c.logged("even through our menu: Android Auto steps aside"));
        c.overlay(0);
        c.poll();
        CHECK_OUT(c, "");  // stepped aside: no event over iDrive's screen
        c.polls(3);
        CHECK_OUT(c, "");
        c.hold.showAndHold("entry button", c.now);  // the Android Auto entry brings it back
        CHECK_OUT(c, "show, event 52");
    }
    {  // the menu never opens, event 52 alone works; once the blank state has the focus, the tries start over
        Car c;
        connected(c);
        c.overlay(1);
        c.polls(4);
        CHECK_OUT(c, "event 51, event 51, event 51, event 52");
        c.blankFocus(true);  // the focus, without a visibility notification
        c.polls(5);
        CHECK_OUT(c, "");
        CHECK(c.hold.holding());
        c.io.appKey = true;  // later, NAV: event 52, refused over iDrive's navigation
        c.io.appKeyAt = c.now;
        c.blankLeft();
        CHECK_OUT(c, "app key removed, event 52");
        c.poll();
        CHECK_OUT(c, "event 51");  // through our menu again, not given up
    }
    {  // the menu opens but refuses the blank state: event 52 at each poll, 8 at most
        Car c;
        connected(c);
        c.overlay(1);
        c.poll();
        CHECK_OUT(c, "event 51");
        c.menu(true);
        CHECK_OUT(c, "event 52");
        for (int i = 0; i < 7; ++i) {
            c.poll();
            CHECK_OUT(c, "event 52");
        }
        CHECK(c.logged("BACK returned to our menu, try 8"));
        c.polls(3);
        CHECK_OUT(c, "");
        CHECK(c.logged("hold: not entering state 13 (BACK returned to our menu): already tried 8 times"));
        CHECK(c.hold.holding());  // Android Auto stays: the menu is ours, not an iDrive screen
        c.menu(false);
        c.blankVisible(true);  // the blank state shows at last, without a focus notification: the tries start over
        c.ms += 1000;
        c.hold.menuFocus(kHelpState, true, c.now, c.ms);  // a stray push opened Help
        CHECK_OUT(c, "event 52");
    }
}

void comesUpOverOurMenu() {
    Car c;
    connected(c);
    c.menu(true);  // Start in the menu, the phone connects meanwhile
    c.overlay(1);
    c.poll();
    CHECK_OUT(c, "event 52");
    CHECK(c.logged("Android Auto came on screen over our menu"));
}

void backToOurMenu() {
    Car c;
    heldOnBlank(c);
    c.blankVisible(false);
    c.menu(true);  // BACK / tilt-left popped iDrive to our menu
    CHECK_OUT(c, "event 52");
    CHECK(c.logged("BACK/tilt popped to our menu (immediate)"));
    c.ms += 250;
    c.hold.menuFocus(kHelpState, true, c.now, c.ms);  // a stray push opened Help within 300 ms
    CHECK_OUT(c, "");
    c.ms += 50;
    c.hold.menuFocus(kHelpState, true, c.now, c.ms);
    CHECK_OUT(c, "event 52");
    c.blankShown();
    c.polls(3);
    CHECK_OUT(c, "");
}

void popupOverBlank() {
    Car c;
    heldOnBlank(c);
    c.blankFocus(false);  // an iDrive popup takes the focus, the blank state stays visible
    CHECK_OUT(c, "");
    c.poll();
    CHECK_OUT(c, "popup 1");  // Android Auto steps aside so the driver sees it
    CHECK(c.hold.popupPeek());
    CHECK(c.logged("popup: iDrive took the focus over state 13 (gear 1): Android Auto picture removed"));
    c.polls(30);  // an alert longer than 10 s: still aside, no event 52 over it
    CHECK_OUT(c, "");
    c.blankFocus(true);  // the popup closes
    c.poll();
    CHECK_OUT(c, "popup 0");
    c.polls(3);
    CHECK_OUT(c, "");
    CHECK(c.hold.holding());
}

void drivingModeThenBack() {
    Car c;
    heldOnBlank(c);
    c.hold.driveModeChanged(c.now);
    c.blankFocus(false);  // iDrive's driving mode display, without a screen change
    c.polls(15);
    CHECK_OUT(c, "");  // its own shorter peek shows it (no popup 1), and its focus loss is not fought for 15 s
    c.poll();
    CHECK_OUT(c, "event 52");  // then after two polls without focus
    CHECK(c.logged("state lost focus, try 1"));
    for (int i = 2; i <= 8; ++i) {  // the focus does not come back: every two polls, 8 at most
        c.poll();
        CHECK_OUT(c, "");
        c.poll();
        CHECK_OUT(c, "event 52");
    }
    c.polls(4);
    CHECK_OUT(c, "");
    CHECK(c.logged("(state lost focus): already tried 8 times"));
    c.blankFocus(true);  // the focus comes back: the hold worked, its tries start over
    c.poll();
    CHECK_OUT(c, "");
    c.blankVisible(false);
    c.menu(true);              // the next BACK
    CHECK_OUT(c, "event 52");  // not left under Android Auto
}

void reverseGear() {
    {  // the camera: Android Auto steps aside by itself, iDrive is not moved
        Car c;
        heldOnBlank(c);
        c.hold.gearChanged(2);
        c.blankFocus(false);
        c.overlay(0);
        c.poll();
        CHECK_OUT(c, "");  // neither the menu nor the main menu over the camera
        CHECK(c.logged("stepped aside (state 13 visible 1, hard key 0, gear 2)"));
        CHECK(!c.hold.holding());
        c.blankVisible(true);  // iDrive back on the blank state while in reverse: no show
        CHECK_OUT(c, "");
        c.hold.gearChanged(1);
        c.blankFocus(true);
        c.overlay(1);  // reverse ended, Android Auto back over the blank state
        c.poll();
        CHECK_OUT(c, "");
        CHECK(c.hold.holding());
    }
    {  // in reverse, iDrive away from the blank state: no hard key step aside, no app key hold, no popup peek
        Car c;
        heldOnBlank(c);
        c.hold.gearChanged(2);
        c.io.appKey = true;
        c.io.appKeyAt = c.now;
        c.blankLeft();
        c.polls(5);
        CHECK_OUT(c, "");
        CHECK(c.io.appKey);
    }
    {  // in reverse, the blank state unfocused under Android Auto is not a popup
        Car c;
        heldOnBlank(c);
        c.hold.gearChanged(2);
        c.blankFocus(false);
        c.poll();
        CHECK_OUT(c, "");
        CHECK(!c.hold.popupPeek());
    }
}

void calls() {
    {  // Android Auto comes up during a call, over iDrive's call screen: event 52 directly, the call screen not fought
        Car c;
        connected(c);
        c.call(true);
        c.overlay(1);
        c.poll();
        CHECK_OUT(c, "event 52");
        CHECK(c.logged("Android Auto came on screen during a call"));
        c.polls(20);
        CHECK_OUT(c, "");  // no menu route, no step aside during the call
        c.call(false);
        c.poll();
        CHECK_OUT(c, "event 52");  // the call ended: back on the blank state
        CHECK(c.logged("call ended"));
        c.blankShown();
        c.polls(3);
        CHECK_OUT(c, "");
    }
    {  // a call while Android Auto is held: no popup peek, no event 52, no immediate hold from the menu
        Car c;
        heldOnBlank(c);
        c.call(true);
        c.blankFocus(false);
        c.polls(5);
        CHECK_OUT(c, "");
        c.blankVisible(false);
        c.menu(true);
        CHECK_OUT(c, "");
        CHECK(c.io.reads == 0);
        c.blankShown();  // the call screen closed by itself before the call ended
        c.call(false);
        c.poll();
        CHECK_OUT(c, "");  // the blank state shows: the ended call needs nothing
        c.blankLeft();     // later, a hard key
        c.poll();
        CHECK_OUT(c, "");
        c.poll();
        CHECK_OUT(c, "hide");  // a hard key, not "call ended"
    }
    {  // Android Auto shown again by iDrive returning to the blank state: not during a call
        Car c;
        connected(c);
        c.call(true);
        c.blankVisible(true);
        CHECK_OUT(c, "");
    }
}

void appKeys() {
    {  // NAV opened Android Auto's navigation: iDrive switched to its own underneath, event 52 at once
        Car c;
        heldOnBlank(c);
        c.io.appKey = true;
        c.io.appKeyAt = c.now;
        c.now += 1;
        c.blankLeft();
        CHECK_OUT(c, "app key removed, event 52");
        CHECK(c.logged("key opened an Android Auto app, try 1"));
        c.blankShown();
        c.polls(3);
        CHECK_OUT(c, "");
    }
    {  // the key file appears after iDrive left the blank state: the poll catches it
        Car c;
        heldOnBlank(c);
        c.blankLeft();
        CHECK_OUT(c, "");
        c.io.appKey = true;
        c.io.appKeyAt = c.now + 1;
        c.poll();
        CHECK_OUT(c, "app key removed, event 52");
        CHECK(c.logged("key opened an Android Auto app (poll)"));
    }
    {  // a key file older than 2 s is a hard key
        Car c;
        heldOnBlank(c);
        c.io.appKey = true;
        c.io.appKeyAt = c.now - 3;
        c.blankLeft();
        CHECK_OUT(c, "");
        c.poll();
        CHECK_OUT(c, "");
        c.poll();
        CHECK_OUT(c, "hide");
        CHECK(c.io.appKey);
    }
    {  // during a call the key file is left alone
        Car c;
        heldOnBlank(c);
        c.call(true);
        c.io.appKey = true;
        c.io.appKeyAt = c.now;
        c.blankLeft();
        CHECK_OUT(c, "");
        CHECK(c.io.appKey);
    }
}

void hardKeys() {
    {  // MENU: confirmed on a second poll, Android Auto steps aside; no event when it does
        Car c;
        heldOnBlank(c);
        c.blankLeft();
        CHECK_OUT(c, "");
        c.poll();
        CHECK_OUT(c, "");
        c.poll();
        CHECK_OUT(c, "hide");
        CHECK(c.logged("hold: iDrive left state 13 (hard key): asked the service to step aside"));
        CHECK(!c.hold.holding());
        c.overlay(0);
        c.poll();
        CHECK_OUT(c, "");  // iDrive already went elsewhere: no main menu
        CHECK(c.logged("stepped aside (state 13 visible 0, hard key 1, gear 1)"));
    }
    {  // two sightings more than 3 s apart start over
        Car c;
        heldOnBlank(c);
        c.blankLeft();
        c.poll();
        c.now += 4;
        c.poll();
        CHECK_OUT(c, "");
        c.poll();
        CHECK_OUT(c, "hide");
    }
    {  // iDrive back on the blank state before the service stepped aside: still no main menu, it was asked to go
        Car c;
        heldOnBlank(c);
        c.blankLeft();
        c.polls(2);
        CHECK_OUT(c, "hide");
        c.blankShown();
        CHECK_OUT(c, "");  // Android Auto still on screen: no show
        c.overlay(0);
        c.poll();
        CHECK_OUT(c, "");
    }
}

void stepsAsideByItself() {
    {  // the phone's exit icon: iDrive main menu
        Car c;
        heldOnBlank(c);
        c.overlay(0);
        c.poll();
        CHECK_OUT(c, "event 50");
        CHECK(!c.hold.holding());
    }
    {  // OPTION: iDrive's option panel over the blank state, the app's menu
        Car c;
        heldOnBlank(c);
        c.blankFocus(false);
        c.overlay(0);
        c.poll();
        CHECK_OUT(c, "event 51");
        CHECK(c.logged("hold: OPTION: event 51 -> Android Auto menu"));
    }
}

void backToBlankWhileMinimized() {
    Car c;
    heldOnBlank(c);
    c.blankLeft();  // a hard key: Android Auto steps aside
    c.polls(2);
    c.overlay(0);
    c.poll();
    CHECK_OUT(c, "hide");
    const int reads = c.io.reads;
    c.blankShown();  // BACK walked iDrive's history back to the blank state
    CHECK_OUT(c, "show");
    CHECK(c.io.reads == reads + 1);
    CHECK(c.logged("iDrive went back to the Android Auto screen"));
    c.overlay(1);
    c.poll();
    CHECK_OUT(c, "");  // over the blank state already
    CHECK(c.hold.holding());
    {
        Car d;  // not while the phone is not connected
        d.service("running", 42, 0);
        d.poll();
        d.take();
        d.blankVisible(true);
        CHECK_OUT(d, "");
    }
}

void startWhileRunning() {
    Car c;
    connected(c);
    c.menu(true);
    c.hold.requestShow();  // Start while the service runs
    CHECK_OUT(c, "show");
    c.overlay(1);
    c.poll();
    CHECK_OUT(c, "event 52");
}

void serviceRestart() {
    {  // started in reverse: the call and the gear when it becomes reachable, again when the phone connects
        Car c;
        c.hold.gearChanged(2);
        c.service("connecting", -1, 0);  // the supervisor in its preflight
        c.poll();
        CHECK_OUT(c, "");
        c.service("running", 42, 0);
        c.poll();
        CHECK_OUT(c, "call 0, gear 2");
        c.poll();
        CHECK_OUT(c, "");
        c.service("connected", 42, 0);
        c.poll();
        CHECK_OUT(c, "call 0, gear 2");
        c.poll();
        CHECK_OUT(c, "");
        c.call(true);
        c.service("idle", 0, 0);  // the service ends and starts again
        c.poll();
        CHECK_OUT(c, "");
        c.service("running", 43, 0);
        c.poll();
        CHECK_OUT(c, "call 1, gear 2");
    }
    {  // no gear received yet: only the call
        Car c;
        c.service("running", 42, 0);
        c.poll();
        CHECK_OUT(c, "call 0");
    }
    {  // restarted while Android Auto was held: iDrive main menu, then the new overlay is held again
        Car c;
        heldOnBlank(c);
        c.service("idle", 0, 0);
        c.poll();
        CHECK_OUT(c, "event 50");
        c.blankLeft();  // iDrive on its main menu
        c.service("connected", 43, 1);
        c.poll();
        CHECK_OUT(c, "call 0, gear 1, event 51");
        c.menu(true);
        CHECK_OUT(c, "event 52");
    }
}

}  // namespace

int main() {
    manualOpen();
    openButNeverCame();
    automaticStartOverIdriveScreen();
    automaticStartRefused();
    comesUpOverOurMenu();
    backToOurMenu();
    popupOverBlank();
    drivingModeThenBack();
    reverseGear();
    calls();
    appKeys();
    hardKeys();
    stepsAsideByItself();
    backToBlankWhileMinimized();
    startWhileRunning();
    serviceRestart();
    if (g_failures) {
        printf("FAIL hold machine: %d checks failed\n", g_failures);
        return 1;
    }
    puts(
        "PASS hold machine: manual open, automatic start through the menu, refusals, BACK, popups, driving mode, "
        "reverse, calls, app keys, hard keys, stepping aside, service restart");
    return 0;
}
