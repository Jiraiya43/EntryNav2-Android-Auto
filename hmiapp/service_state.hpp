// Copyright (c) 2026 Jiraiya43
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// The service's state as the app sees it: the line "state pid overlay" the service writes, or what the app infers.
#ifndef ENTRY_AAD_HMIAPP_SERVICE_STATE_HPP
#define ENTRY_AAD_HMIAPP_SERVICE_STATE_HPP

#include <string.h>

namespace evo {
namespace app {

struct ServiceState {
    char state[24];  // "idle", "connecting", "running", "connected", ... as the service writes it
    int pid;         // > 0: the session process; -1: starting, or answering without a state file; 0: not running
    int overlay;     // 1: Android Auto is on screen (knob and touch go to the phone)

    ServiceState() : pid(0), overlay(0) { strcpy(state, "idle"); }
    ServiceState(const char* name, int processId, int onScreen) : pid(processId), overlay(onScreen) {
        strncpy(state, name, sizeof(state) - 1);
        state[sizeof(state) - 1] = 0;
    }

    bool is(const char* name) const { return !strcmp(state, name); }
    // Android Auto is on screen in a running session.
    bool onScreen() const { return pid > 0 && overlay; }
};

}  // namespace app
}  // namespace evo

#endif
