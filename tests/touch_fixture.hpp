// Copyright (c) 2026 Jiraiya43
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Builds a touch PDU for the input tests.
#ifndef ENTRY_AAD_TESTS_TOUCH_FIXTURE_HPP
#define ENTRY_AAD_TESTS_TOUCH_FIXTURE_HPP

#include <cstring>

#include "evo_touch_frame.hpp"

namespace touch_fixture {

const int kFrameRecords = 12;
const size_t kFrameBytes = aa::touchpdu::kHeader + aa::touchpdu::kRecord * kFrameRecords;

inline void buildTouchFrame(unsigned char* frame, const aa::TouchContact* contacts, int count, unsigned char seq0,
                            unsigned char counter = 0) {
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

    unsigned char stream[2 + pdu::kContactBytes * aa::kTouchMaxContacts] = {0};
    stream[0] = counter;
    stream[1] = static_cast<unsigned char>(count);
    for (int i = 0; i < count && i < aa::kTouchMaxContacts; ++i) {
        const aa::TouchContact& contact = contacts[i];
        unsigned char* at = stream + 2 + pdu::kContactBytes * i;
        at[0] = static_cast<unsigned char>(contact.x >> 8);
        at[1] = static_cast<unsigned char>(contact.x);
        at[2] = static_cast<unsigned char>(contact.y >> 8);
        at[3] = static_cast<unsigned char>(contact.y);
        at[4] = static_cast<unsigned char>(contact.size);
        at[5] = static_cast<unsigned char>((contact.state << 4) | (contact.id & 0x0F));
    }
    for (size_t i = 0; i < sizeof(stream); ++i) {
        frame[pdu::kHeader + pdu::kRecord * (pdu::kStreamRecord + i / 4) + 4 + i % 4] = stream[i];
    }
}

inline void buildTouchFrame(unsigned char* frame, const aa::TouchContact& contact, unsigned char seq0) {
    buildTouchFrame(frame, &contact, 1, seq0);
}

}  // namespace touch_fixture

// Compatibility for multi-contact tests added on the base branch after this fixture was extracted.
namespace aa {
inline void buildTouchFrame(unsigned char* frame, const TouchContact* contacts, int count, unsigned char counter,
                            unsigned char seq0 = 0) {
    touch_fixture::buildTouchFrame(frame, contacts, count, seq0, counter);
}
}  // namespace aa

#endif  // ENTRY_AAD_TESTS_TOUCH_FIXTURE_HPP
