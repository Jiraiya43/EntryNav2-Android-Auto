// Copyright (c) 2026 Jiraiya43
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// The touch PDUs the iDrive HMI receives: records joined across datagrams, then the finger contacts read.
#ifndef ENTRY_AAD_PLATFORM_EVO_TOUCH_FRAME_HPP
#define ENTRY_AAD_PLATFORM_EVO_TOUCH_FRAME_HPP

// UDP 160.48.199.115:30501 -> 30567, identifier B0 04 80 02. A datagram is a 16-byte header ([0..3] identifier,
// [4..7] BE32 datagram size - 8), then 8-byte records [00][type][flags][seq][4 data bytes]: type 0xC5 opens a message,
// 0xC1 continues it, 0xC3 closes it (bit 2 start, bit 1 end); seq counts records, wrapping, across datagrams.
// A message is 12 records, in one 112-byte datagram or spread over 2 to 4 smaller ones (e.g. 32 + 56 + 56 bytes:
// records 0-1, 2-6, 7-11), so records are joined by type and seq first. The data bytes of records 4 and on, record
// headers left out, then form the touch stream:
//     [0] flags and a rolling counter (top 2 bits: 00, 40, 80, c0)   [1] contact count N
//     N contacts of 6 bytes: x hi, x lo, y hi, y lo, size, then one byte: bits 6-7 another field, bits 4-5 the
//     state, bits 0-3 the id
// state 1 down, 2 move, 3 up; id a stable finger number; x and y in panel pixels (0..1279, 0..479). A second finger
// straddles records 6 and 7. The last byte is split as iDrive splits it (CTouchDataClient::decodeTouchScreenPduData in
// HmiMain3D: bits 6-7, 4-5 and 0-3 stored apart in its FingerData). The reference G20 leaves bits 6-7 at 0; on a
// panel that sets them, reading bits 4-7 as the state dropped the contact, and a drag became a tap (F45, #30).
// No allocation; tested in tests/test_evo_input.cpp.

#include <stddef.h>
#include <stdint.h>

#include "aa_input.hpp"

namespace aa {

enum { kTouchMaxMsgs = 4 };  // messages one datagram completes, at most

struct TouchContact {
    int id;
    int state;
    int x;
    int y;
    int size;
};

// The data bytes of one joined message, 4 per record in record order.
struct TouchMsg {
    enum { kMaxRecords = 32 };
    unsigned char d[kMaxRecords * 4];
    int recs;
    unsigned char seq0, seq1;  // seq of its first and last records: the next message starts at seq1 + 1
};

namespace touchpdu {
const size_t kHeader = 16;
const size_t kRecord = 8;
const int kStreamRecord = 4;  // the stream starts in the data of the message's record 4
const int kContactBytes = 6;
inline bool isIdent(const unsigned char* p) {
    return p[0] == 0xB0 && p[1] == 0x04 && p[2] == 0x80 && p[3] == 0x02;
}
}  // namespace touchpdu

// The contacts of a joined message, up to max (contacts with an unknown state, or an id past our table, skipped).
// -1 when the message is too short for the stream's two first bytes. *declared gets the count the message claims.
inline int parseTouchStream(const TouchMsg& m, TouchContact* out, int max, int* declared) {
    if (declared) {
        *declared = 0;
    }
    const int start = touchpdu::kStreamRecord * 4;
    const int size = m.recs * 4 - start;
    if (size < 2) {
        return -1;
    }
    const unsigned char* stream = m.d + start;
    const int claimed = stream[1];
    if (declared) {
        *declared = claimed;
    }
    int found = 0;
    for (int c = 0; c < claimed && c < kTouchMaxContacts && found < max; ++c) {
        const unsigned char* at = stream + 2 + touchpdu::kContactBytes * c;
        if (2 + touchpdu::kContactBytes * c + 5 >= size) {
            break;
        }
        TouchContact& contact = out[found];
        contact.x = (at[0] << 8) | at[1];
        contact.y = (at[2] << 8) | at[3];
        contact.size = at[4];
        contact.state = (at[5] >> 4) & 0x03;  // bits 6-7 are another field (see above)
        contact.id = at[5] & 0x0F;
        if (contact.state >= 1 && contact.state <= 3 && contact.id < kTouchMaxContacts) {
            ++found;
        }
    }
    return found;
}

// Joins the records of each message across datagrams. A lost datagram loses its message; the next opening record
// starts afresh.
class TouchReassembler {
public:
    TouchReassembler() : open_(false), lastSeq_(0), dropped_(0) { cur_.recs = 0; }

    // One datagram: the messages it completed, written to out (maxMsgs at most). -1 when it is not a touch PDU.
    int feed(const unsigned char* buf, size_t n, TouchMsg* out, int maxMsgs) {
        if (!buf || n < touchpdu::kHeader + touchpdu::kRecord || !touchpdu::isIdent(buf)) {
            return -1;
        }
        const uint32_t length = (static_cast<uint32_t>(buf[4]) << 24) | (static_cast<uint32_t>(buf[5]) << 16) |
                                (static_cast<uint32_t>(buf[6]) << 8) | buf[7];
        if (length + 8 < n && length + 8 >= touchpdu::kHeader + touchpdu::kRecord) {
            n = length + 8;  // trailing bytes ignored
        }
        int done = 0;
        for (const unsigned char* record = buf + touchpdu::kHeader; record + touchpdu::kRecord <= buf + n;
             record += touchpdu::kRecord) {
            const unsigned char type = record[1], seq = record[3];
            if (record[0] != 0x00 || (type & 0xC0) != 0xC0) {
                drop();
                return done > 0 ? done : -1;
            }
            if (type & 0x04) {
                if (open_) {
                    ++dropped_;  // the previous message never closed
                }
                open_ = true;
                cur_.recs = 0;
                cur_.seq0 = seq;
            } else if (!open_) {
                continue;  // the end of a message whose start was missed
            } else if (seq != static_cast<unsigned char>(lastSeq_ + 1)) {
                drop();  // a datagram was lost
                continue;
            }
            lastSeq_ = seq;
            if (cur_.recs >= TouchMsg::kMaxRecords) {
                drop();
                continue;
            }
            for (int i = 0; i < 4; ++i) {
                cur_.d[cur_.recs * 4 + i] = record[4 + i];
            }
            ++cur_.recs;
            if (type & 0x02) {
                cur_.seq1 = seq;
                if (out && done < maxMsgs) {
                    out[done++] = cur_;
                }
                open_ = false;
                cur_.recs = 0;
            }
        }
        return done;
    }

    unsigned dropped() const { return dropped_; }

private:
    void drop() {
        if (open_) {
            ++dropped_;
        }
        open_ = false;
        cur_.recs = 0;
    }

    TouchMsg cur_;
    bool open_;
    unsigned char lastSeq_;
    unsigned dropped_;
};

}  // namespace aa

#endif  // ENTRY_AAD_PLATFORM_EVO_TOUCH_FRAME_HPP
