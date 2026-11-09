// Copyright (c) 2026 Jiraiya43
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Car input as the session carries it to the phone: touch events, and the key codes the input channel declares.
#ifndef ENTRY_AAD_CORE_AA_INPUT_HPP
#define ENTRY_AAD_CORE_AA_INPUT_HPP

namespace aa {

// Key codes, all declared on input channel 7 by the service discovery: Android's, then Android Auto's own.
enum AndroidKey {
    AAK_HOME = 3,
    AAK_BACK = 4,
    AAK_CALL = 5,
    AAK_SEARCH = 84,
    // The direction pad, clockwise: the controller's tilt, then its push.
    AAK_DPAD_UP = 19,
    AAK_DPAD_RIGHT = 22,
    AAK_DPAD_DOWN = 20,
    AAK_DPAD_LEFT = 21,
    AAK_DPAD_CENTER = 23,
    // Above 65535: the rotary knob (sent as relative steps) and the keys that open an Android Auto app.
    AAK_ROTARY_CONTROLLER = 65536,
    AAK_MEDIA = 65537,
    AAK_NAVIGATION = 65538,
    AAK_TEL = 65540
};

enum { kTouchMaxContacts = 5 };

// A finger on the screen, in the phone's coordinates.
struct TouchPointer {
    int id, x, y;
};

// One touch event. p lists every finger down at that moment, the lifting one included in an UP event, and index is
// the position in p of the finger the action is about. action: 0 PRESS (first finger), 1 RELEASE (last finger),
// 2 DRAG, 5 POINTER_DOWN (one more finger), 6 POINTER_UP (one finger of several lifted).
struct TouchEventOut {
    int n;
    TouchPointer p[kTouchMaxContacts];
    int action;
    int index;
};

}  // namespace aa

#endif  // ENTRY_AAD_CORE_AA_INPUT_HPP
