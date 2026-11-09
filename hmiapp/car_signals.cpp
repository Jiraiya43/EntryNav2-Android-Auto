// Copyright (c) 2026 Jiraiya43
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// The CDS handlers: car gear, phone calls, day/night and driving mode, to the service and the hold (car_signals.hpp).
#include "car_signals.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "../platform/service_contract.hpp"
#include "app_log.hpp"
#include "cds_parse.hpp"
#include "service_link.hpp"

namespace evo {
namespace app {

namespace {

const char* const kGearProperty = "driving.gear";
const char* const kCallProps[2] = {"communication.callStatus0", "communication.callStatus1"};
// Car day/night and driving mode (CDS names present in this HU's libhmiremoting).
const char* const kDayNightProperty = "vehicle.dayNightChangeOver";
const char* const kDriveModeProps[2] = {"driving.mode", "driving.mode35up"};

HoldMachine* g_hold = nullptr;

// ---- car gear -> service (reverse: the camera shows, Android Auto steps aside) --------------------------------------
int g_lastGear = kGearUnknown;
bool onGearChanged(void*, const bmw::BasicString<unsigned short>& value) {
    char text[160];
    narrow(value, text, sizeof(text));
    int gear = -1;
    for (size_t i = 0; text[i]; ++i) {
        if (text[i] >= '0' && text[i] <= '9') {
            gear = atoi(text + i);
            break;
        }
    }
    if (gear != g_lastGear) {
        alog("CDS %s = '%s' -> gear %d", kGearProperty, text, gear);
        g_lastGear = gear;
        g_hold->gearChanged(gear);
        char msg[32];
        snprintf(msg, sizeof(msg), "gear %d", gear);
        sendToService(msg);
    }
    return true;
}

// ---- phone calls: iDrive may show its own call screen; Android Auto shows the call too ------------------------------
bool g_callLine[2] = {false, false};
bool onCallStatus(int line, const bmw::BasicString<unsigned short>& value) {
    char text[200];
    narrow(value, text, sizeof(text));
    bool active = aa::cdsCallActive(text);
    bool was = callActive();
    g_callLine[line] = active;
    g_hold->callChanged(callActive(), time(nullptr));
    alog("CDS %s = '%s' -> %s", kCallProps[line], text, active ? "in a call" : "idle");
    if (was != callActive()) {
        sendToService(callActive() ? "call 1" : "call 0");  // phone keys follow iDrive's call
    }
    return true;
}
bool onCallStatus0(void*, const bmw::BasicString<unsigned short>& v) {
    return onCallStatus(0, v);
}
bool onCallStatus1(void*, const bmw::BasicString<unsigned short>& v) {
    return onCallStatus(1, v);
}

// ---- day and night --------------------------------------------------------------------------------------------------
int g_lastDayNight = -1000;
bool onDayNightChanged(void*, const bmw::BasicString<unsigned short>& value) {
    char text[160];
    narrow(value, text, sizeof(text));
    int v = aa::cdsIntField(text, "dayNightChangeOver", -1);
    if (v == g_lastDayNight) {
        return true;
    }
    g_lastDayNight = v;
    alog("CDS %s = '%s' -> %d (1 day, 2 night, 0 no change, 3 invalid)", kDayNightProperty, text, v);
    if (v != 1 && v != 2) {
        return true;
    }
    FILE* f = fopen(contract::kDayNightFile, "w");
    if (f) {
        fprintf(f, "daynight %d\n", v);
        fclose(f);
    }
    char msg[32];
    snprintf(msg, sizeof(msg), "daynight %d", v);
    sendToService(msg);
    return true;
}

// ---- driving mode ---------------------------------------------------------------------------------------------------
int g_lastDriveMode[2] = {-1000, -1000};
long g_driveModeSubAt = -1;
long monoSec() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<long>(ts.tv_sec);
}
bool onDriveMode(int prop, const bmw::BasicString<unsigned short>& value) {
    char text[160];
    narrow(value, text, sizeof(text));
    int v = aa::cdsIntField(text, "mode", -1);
    int was = g_lastDriveMode[prop];
    if (v == was) {
        return true;
    }
    const int invalid = prop == 1 ? 63 : 15;  // "not valid" values
    alog("CDS %s = '%s' -> mode %d%s", kDriveModeProps[prop], text, v, v == invalid || v < 0 ? " (not valid)" : "");
    g_lastDriveMode[prop] = v;
    const bool valid = v >= 0 && v != invalid;
    // A change between two valid modes, or a first valid value well after subscribing (some variants send none).
    const bool change = valid && ((was >= 0 && was != invalid) ||
                                  (was < 0 && g_driveModeSubAt >= 0 && monoSec() - g_driveModeSubAt >= 5));
    if (!change) {
        return true;
    }
    char msg[32];
    snprintf(msg, sizeof(msg), "drivemode %d", v);
    sendToService(msg);
    alog("drive mode change -> service (%s)", msg);
    g_hold->driveModeChanged(time(nullptr));
    return true;
}
bool onDriveMode0(void*, const bmw::BasicString<unsigned short>& v) {
    return onDriveMode(0, v);
}
bool onDriveMode1(void*, const bmw::BasicString<unsigned short>& v) {
    return onDriveMode(1, v);
}

}  // namespace

void subscribeCarSignals(bmw::PluginApp* app, HoldMachine& hold) {
    g_hold = &hold;
    addCdsHandler(app, kGearProperty, &onGearChanged);
    alog("CDS handler for %s registered", kGearProperty);
    for (int i = 0; i < 2; ++i) {
        addCdsHandler(app, kCallProps[i], i == 0 ? &onCallStatus0 : &onCallStatus1);
        alog("CDS handler for %s registered", kCallProps[i]);
    }
    addCdsHandler(app, kDayNightProperty, &onDayNightChanged);
    alog("CDS handler for %s registered", kDayNightProperty);
    g_driveModeSubAt = monoSec();
    for (int i = 0; i < 2; ++i) {
        addCdsHandler(app, kDriveModeProps[i], i == 0 ? &onDriveMode0 : &onDriveMode1);
        alog("CDS handler for %s registered", kDriveModeProps[i]);
    }
}

bool callActive() {
    return g_callLine[0] || g_callLine[1];
}

int lastGear() {
    return g_lastGear;
}

}  // namespace app
}  // namespace evo
