// Copyright (c) 2026 Jiraiya43
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Builds a touch PDU carrying one contact for the input tests.
#ifndef ENTRY_AAD_TESTS_TOUCH_FIXTURE_HPP
#define ENTRY_AAD_TESTS_TOUCH_FIXTURE_HPP

#include <cstring>

#include "evo_touch_frame.hpp"

namespace touch_fixture {

const int kFrameRecords = 12;
const size_t kFrameBytes = aa::touchpdu::kHeader + aa::touchpdu::kRecord * kFrameRecords;

inline void buildTouchFrame(unsigned char* frame, const aa::TouchContact& contact, unsigned char seq0) {
    namespace pdu = aa::touchpdu;
    std::memset(frame, 0, kFrameBytes);
    const unsigned char ident[] = {0xB0, 0x04, 0x80, 0x02};
    std::memcpy(frame, ident, sizeof(ident));
    frame[7] = static_cast<unsigned char>(kFrameBytes - 8);
    for (int r = 0; r < kFrameRecords; ++r) {
        unsigned char* record = frame + pdu::kHeader + pdu::kRecord * r;
        record[1] = r == 0 ? 0xC5 : (r == kFrameRecords - 1 ? 0xC3 : 0xC1);
        record[2] = 0x40;
        record[3] = static_cast<unsigned char>(seq0 + r);
    }

    const unsigned char stream[] = {
        0,
        1,
        static_cast<unsigned char>(contact.x >> 8),
        static_cast<unsigned char>(contact.x),
        static_cast<unsigned char>(contact.y >> 8),
        static_cast<unsigned char>(contact.y),
        static_cast<unsigned char>(contact.size),
        static_cast<unsigned char>((contact.state << 4) | (contact.id & 0x0F)),
    };
    for (size_t i = 0; i < sizeof(stream); ++i) {
        frame[pdu::kHeader + pdu::kRecord * (pdu::kStreamRecord + i / 4) + 4 + i % 4] = stream[i];
    }
}

}  // namespace touch_fixture

#endif  // ENTRY_AAD_TESTS_TOUCH_FIXTURE_HPP
