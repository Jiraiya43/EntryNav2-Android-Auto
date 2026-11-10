// Copyright (c) 2026 Jiraiya43
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// What the car's inputs do during a run, shared by the touchscreen (evo_touch) and the controller (evo_knob).
#ifndef ENTRY_AAD_PLATFORM_INPUT_MODE_HPP
#define ENTRY_AAD_PLATFORM_INPUT_MODE_HPP

#include <string.h>

namespace evo {

enum InputMode { INPUT_OFF = 0, INPUT_OBSERVE = 1, INPUT_FORWARD = 2 };

// --input forward | observe | off of entry-aad (set by aa-supervisor); null or empty = off; anything else: false.
inline bool parseInputMode(const char* text, InputMode& mode) {
    if (!text || !*text || !strcmp(text, "off")) {
        mode = INPUT_OFF;
        return true;
    }
    if (!strcmp(text, "observe")) {
        mode = INPUT_OBSERVE;
        return true;
    }
    if (!strcmp(text, "forward")) {
        mode = INPUT_FORWARD;
        return true;
    }
    return false;
}

inline const char* inputModeName(InputMode mode) {
    return mode == INPUT_FORWARD ? "forwarded to AA" : mode == INPUT_OBSERVE ? "logged only" : "off";
}

}  // namespace evo

#endif
