// Copyright (c) 2026 Jiraiya43
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// The iDrive controller's status messages (MOST FBlock 0xE5) as Android Auto key and rotary events, two protocols.
#ifndef ENTRY_AAD_PLATFORM_EVO_CCS_HPP
#define ENTRY_AAD_PLATFORM_EVO_CCS_HPP

// CCS35up, the G20's controller (instance 0x03, Status op 0xC), as seen on the car:
//   0xC46 RotaryEncoderState, 8 bytes: [0] message counter, [1..2] BE16 detent position (+1 per clockwise detent,
//         wraps), [3] centre push (1 pressed, 0 released, no held value), [4] tilt on a 12-step clock (0 centred,
//         1 up, 4 right, 7 down, 10 left; seen 1, 7, 4, 10 for up, down, right, left), [5..6] 0, [7] 0xff.
//   0xC47 PushButtonsState (7 bytes) and 0xC4F PushButtonsState2 (8 bytes, [7] = 7): the same state, one byte per hard
//         key, 0 released or 1 pressed (a 1.5 s MEDIA hold stayed 1): the change between messages makes one event.
//         Slots in BMW key order: 0 MENU, 1 BACK, 2 OPTIONS, 3 COM, 4 MEDIA, 5 NAV, 6 MAP (BACK sets slot 1, never 2).
// The older CCS protocol, on the same FBlock (instance 0x01 for the front controller), as HmiMain3D reads it, which
// decodes both and follows whichever the controller speaks; C10 and C20 were seen in a G30 capture:
//   0xC10 RotaryEncoder, 7 bytes: [0] sender 0x01, [1] receiver 0x1D, [2] counter, +1 per message (0xFF: controller
//         not configured; dropped, the next message re-bases), [3..4] BE16 detent position (higher = clockwise),
//         [5] cluster 0x1E, [6] overflow (non-zero: the position re-based). The rear controller (instance 2) has one
//         more byte before the cluster.
//   0xC20 PushButton, 7 bytes, one event each: [0] sender, [1] receiver, [2] counter (0xFF: ignored), [3] status
//         0 released, 1 pressed, 2 held, 3 no change, [4] tilt 1 up, 2 right, 4 down, 8 left (0 on release),
//         [5] cluster: 0x00 hard keys, 0x1D tilt, 0x1E knob (sensor 1: centre push), [6] sensor: for a hard key, bit n
//         is slot n above (the same key table), so one slot mapping serves both protocols.
// No I/O; tested in tests/test_evo_input.cpp.

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "aa_input.hpp"

namespace aa {

struct CcsEvent {
    int kind;  // 1 a key (keycode, down), 2 relative steps (keycode, delta)
    uint32_t keycode;
    int down;
    int delta;
    const char* what;  // its source, for the log: "MEDIA", "ROTARY", "TILT_UP"...
};

class CcsDecoder {
public:
    // Controller instances on FBlock 0xE5: CCS35up, and the older protocol's front controller.
    enum { kInstance = 0x03, kCcsInstance = 0x01 };
    enum { kSlots = 7 };

    CcsDecoder() : position_(-1), tilt_(0), push_(false), ccsPosition_(-1), ccsTilt_(0), ccsPush_(false) {
        static const uint32_t kDefaultKeys[kSlots] = {AAK_HOME,       AAK_BACK,      0, AAK_CALL, AAK_MEDIA,
                                                      AAK_NAVIGATION, AAK_NAVIGATION};
        for (int i = 0; i < kSlots; ++i) {
            slotKeys_[i] = kDefaultKeys[i];
            slots_[i] = ccsSlots_[i] = false;
        }
    }

    // The Android key of a hard key slot; 0 leaves the key to iDrive (its events still come, with keycode 0).
    void setSlotKey(int slot, uint32_t key) {
        if (slot >= 0 && slot < kSlots) {
            slotKeys_[slot] = key;
        }
    }
    uint32_t slotKey(int slot) const { return slot >= 0 && slot < kSlots ? slotKeys_[slot] : 0; }
    static const char* slotName(int slot) {
        static const char* const kNames[kSlots] = {"MENU", "BACK", "OPTIONS", "COM", "MEDIA", "NAV", "MAP"};
        return slot >= 0 && slot < kSlots ? kNames[slot] : "?";
    }
    // The slot of an upper-case key name; -1 when unknown.
    static int slotByName(const char* name) {
        for (int i = 0; name && i < kSlots; ++i) {
            if (!strcmp(name, slotName(i))) {
                return i;
            }
        }
        return -1;
    }

    // One MOST message: the events it makes, written to out (max at most). Returns how many.
    int feed(uint8_t fblock, uint8_t inst, uint16_t fkt, uint8_t op, const uint8_t* d, size_t n, CcsEvent* out,
             int max) {
        Events events(out, max);
        if (fblock != 0xE5 || op != 0xC || !d) {
            return 0;
        }
        if ((fkt == 0xC10 || fkt == 0xC20) && inst == kCcsInstance) {
            return fkt == 0xC10 ? ccsRotary(d, n, events) : ccsButton(d, n, events);
        }
        if (inst != kInstance) {
            return 0;
        }
        if (fkt == 0xC46 && n >= 5) {
            const int position = (d[1] << 8) | d[2];
            if (position_ >= 0 && position != position_) {
                rotate(steps(position, position_), events);
            }
            position_ = position;
            setTilt(clockTilt(d[4]), tilt_, events);
            const bool push = d[3] != 0;
            if (push != push_) {
                events.add(1, AAK_DPAD_CENTER, push, 0, "PUSH");
                push_ = push;
            }
        } else if ((fkt == 0xC47 || fkt == 0xC4F) && n >= 1) {
            for (int slot = 0; slot < kSlots && static_cast<size_t>(slot) < n; ++slot) {
                setSlot(slot, d[slot] != 0, slots_, events);
            }
        }
        return events.count;
    }

private:
    // The events of one message, as many as fit.
    struct Events {
        CcsEvent* out;
        int max;
        int count;
        Events(CcsEvent* to, int room) : out(to), max(room), count(0) {}
        void add(int kind, uint32_t keycode, int down, int delta, const char* what) {
            if (out && count < max) {
                const CcsEvent event = {kind, keycode, down, delta, what};
                out[count++] = event;
            }
        }
    };

    // Detents between two positions of a wrapping 16-bit counter.
    static int steps(int now, int before) { return static_cast<int16_t>(static_cast<uint16_t>(now - before)); }

    static void rotate(int delta, Events& events) {
        if (delta != 0) {
            events.add(2, AAK_ROTARY_CONTROLLER, 0, delta, "ROTARY");
        }
    }

    // CCS35up tilt on the 12-step clock; 0 centred or unknown.
    static uint32_t clockTilt(unsigned hour) {
        if (hour == 0 || hour > 12) {
            return 0;
        }
        return hour == 12 || hour <= 2 ? AAK_DPAD_UP
               : hour <= 5             ? AAK_DPAD_RIGHT
               : hour <= 8             ? AAK_DPAD_DOWN
                                       : AAK_DPAD_LEFT;
    }

    // Older protocol tilt, one bit per direction.
    static uint32_t bitTilt(unsigned bits) {
        switch (bits) {
            case 1:
                return AAK_DPAD_UP;
            case 2:
                return AAK_DPAD_RIGHT;
            case 4:
                return AAK_DPAD_DOWN;
            case 8:
                return AAK_DPAD_LEFT;
            default:
                return 0;
        }
    }

    static const char* tiltName(uint32_t key) {
        switch (key) {
            case AAK_DPAD_UP:
                return "TILT_UP";
            case AAK_DPAD_DOWN:
                return "TILT_DOWN";
            case AAK_DPAD_LEFT:
                return "TILT_LEFT";
            case AAK_DPAD_RIGHT:
                return "TILT_RIGHT";
            default:
                return "TILT_?";
        }
    }

    // A new tilt direction: the old one released, the new one pressed.
    static void setTilt(uint32_t key, uint32_t& held, Events& events) {
        if (key == held) {
            return;
        }
        if (held) {
            events.add(1, held, 0, 0, tiltName(held));
        }
        if (key) {
            events.add(1, key, 1, 0, tiltName(key));
        }
        held = key;
    }

    void setSlot(int slot, bool down, bool* state, Events& events) {
        if (down != state[slot]) {
            state[slot] = down;
            events.add(1, slotKeys_[slot], down ? 1 : 0, 0, slotName(slot));
        }
    }

    int ccsRotary(const uint8_t* d, size_t n, Events& events) {
        if (n < 7) {
            return 0;
        }
        const int position = (d[3] << 8) | d[4];
        if (d[2] == 0xFF || d[5] != 0x1E) {
            ccsPosition_ = -1;
            return 0;
        }
        // The counter changes with every message: only the overflow flag re-bases (re-basing on the counter lost
        // every detent).
        if (d[6] != 0 || ccsPosition_ < 0) {
            ccsPosition_ = position;
            return 0;
        }
        const int delta = steps(position, ccsPosition_);
        ccsPosition_ = position;
        rotate(delta, events);
        return events.count;
    }

    int ccsButton(const uint8_t* d, size_t n, Events& events) {
        if (n < 7 || d[2] == 0xFF || d[3] > 2) {  // status 3: no change; above: invalid
            return 0;
        }
        const bool down = d[3] != 0;
        const uint8_t cluster = d[5], sensor = d[6];
        if (cluster == 0x1E) {
            if (sensor == 1 && down != ccsPush_) {
                ccsPush_ = down;
                events.add(1, AAK_DPAD_CENTER, down ? 1 : 0, 0, "PUSH");
            }
        } else if (cluster == 0x1D) {
            setTilt(down && d[4] ? bitTilt(d[4]) : 0, ccsTilt_, events);
        } else if (cluster == 0x00) {
            for (int slot = 0; slot < kSlots; ++slot) {
                if (sensor == (1u << slot)) {
                    setSlot(slot, down, ccsSlots_, events);
                }
            }
        }
        return events.count;
    }

    int position_;
    uint32_t tilt_;
    bool push_;
    bool slots_[kSlots];     // CCS35up hard keys
    bool ccsSlots_[kSlots];  // older protocol hard keys: a press on one must not hide the other
    uint32_t slotKeys_[kSlots];
    int ccsPosition_;
    uint32_t ccsTilt_;
    bool ccsPush_;
};

// Which of the two protocols the car's controller speaks. vmostd hands a client a function's status only once the
// client has read that function (evo_knob.hpp), so each protocol must be asked for. CCS35up is read first; the older
// functions are read only on a car whose controller stays silent on CCS35up for kSilenceMs (the G20 answers within
// 20 ms and is asked nothing more). The first protocol heard is kept and the other one's status messages are left
// out, so a car that would speak both never counts one action twice.
class CcsProtocol {
public:
    enum Kind { kUnknown, kCcs35up, kOlder };
    enum { kSilenceMs = 2000 };

    CcsProtocol() : kind_(kUnknown), asked_(false), olderAsked_(false), askedMs_(0) {}

    Kind kind() const { return kind_; }
    static const char* name(Kind kind) {
        return kind == kCcs35up ? "CCS35up (E5.03)" : kind == kOlder ? "older CCS (E5.01, C10/C20)" : "unknown";
    }
    // The protocol a status message of FBlock 0xE5 belongs to; kUnknown for any other function.
    static Kind of(uint8_t inst, uint16_t fkt) {
        if (inst == CcsDecoder::kInstance && (fkt == 0xC46 || fkt == 0xC47 || fkt == 0xC4F)) {
            return kCcs35up;
        }
        if (inst == CcsDecoder::kCcsInstance && (fkt == 0xC10 || fkt == 0xC20)) {
            return kOlder;
        }
        return kUnknown;
    }

    // The CCS35up functions have just been read (monotonic milliseconds, compared with wrap-around).
    void ccs35upAsked(uint32_t nowMs) {
        asked_ = true;
        askedMs_ = nowMs;
    }
    // True once: CCS35up has stayed silent since its reads, the older functions are to be read now.
    bool olderDue(uint32_t nowMs) {
        if (!asked_ || olderAsked_ || kind_ != kUnknown || nowMs - askedMs_ < static_cast<uint32_t>(kSilenceMs)) {
            return false;
        }
        olderAsked_ = true;
        return true;
    }
    // A status message (op 0xC) of FBlock 0xE5: false when it belongs to the protocol that was not kept.
    bool accept(uint8_t inst, uint16_t fkt) {
        const Kind kind = of(inst, fkt);
        if (kind == kUnknown) {
            return true;
        }
        if (kind_ == kUnknown) {
            kind_ = kind;
        }
        return kind == kind_;
    }

private:
    Kind kind_;
    bool asked_, olderAsked_;
    uint32_t askedMs_;
};

}  // namespace aa

#endif  // ENTRY_AAD_PLATFORM_EVO_CCS_HPP
