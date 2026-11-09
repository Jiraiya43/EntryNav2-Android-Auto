// Copyright (c) 2026 Jiraiya43
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Android Auto frame header and fragment reassembly (core/aa_frame.hpp), on the PC.
#include <stdio.h>
#include <stdlib.h>

#include <vector>

#include "aa_frame.hpp"

#define CHECK(x)                                                \
    do {                                                        \
        if (!(x)) {                                             \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); \
            exit(1);                                            \
        }                                                       \
    } while (0)

namespace {

typedef std::vector<uint8_t> Bytes;

aa::FrameHeader header(uint8_t channel, uint8_t position, uint16_t length, uint32_t total = 0) {
    aa::FrameHeader h;
    h.channel = channel;
    h.flags = static_cast<uint8_t>(position | aa::frame::kEncrypted);
    h.length = length;
    h.totalSize = total;
    return h;
}

void headers() {
    const uint8_t first[] = {0x06, 0x09, 0x12, 0x34, 0x00, 0x01, 0x02, 0x03};
    const aa::FrameHeader h = aa::decodeFrameHeader(first);
    CHECK(h.channel == 6 && h.flags == 0x09 && h.length == 0x1234 && h.totalSize == 0);
    CHECK(h.announcesSize() && h.encrypted() && h.position() == aa::frame::kFirst);
    CHECK(aa::decodeTotalSize(first + 4) == 0x00010203u);
    const uint8_t whole[] = {0x00, 0x03, 0x00, 0x08};
    CHECK(!aa::decodeFrameHeader(whole).announcesSize() && !aa::decodeFrameHeader(whole).encrypted());

    Bytes out;
    const uint8_t body[] = {0xaa, 0xbb};
    CHECK(aa::encodeFrame(7, 0x0b, body, 2, out) && out == Bytes({7, 0x0b, 0x00, 0x02, 0xaa, 0xbb}));
    CHECK(aa::encodeFrame(0, 0x03, nullptr, 0, out) && out == Bytes({0, 0x03, 0, 0}));
    CHECK(!aa::encodeFrame(0, 0x03, nullptr, 1, out));
    const Bytes biggest(0xFFFF, 1);
    CHECK(aa::encodeFrame(1, 0x0b, &biggest[0], biggest.size(), out) && out.size() == 0xFFFF + 4 && out[2] == 0xFF);
    const Bytes tooBig(0x10000, 1);
    CHECK(!aa::encodeFrame(1, 0x0b, &tooBig[0], tooBig.size(), out));
}

void reassembly() {
    aa::MessageAssembler a;
    Bytes m;
    CHECK(a.add(header(6, aa::frame::kWhole, 9), Bytes({1, 2}), m) == aa::MessageAssembler::kComplete);
    CHECK(m == Bytes({1, 2}));

    // FIRST, MIDDLE, LAST: the wire lengths (TLS records) are larger than the announced plain size.
    CHECK(a.add(header(6, aa::frame::kFirst, 30, 6), Bytes({1, 2}), m) == aa::MessageAssembler::kPending);
    CHECK(a.add(header(6, aa::frame::kMiddle, 30), Bytes({3, 4}), m) == aa::MessageAssembler::kPending);
    // A whole message on the same channel passes through and leaves the fragments as they are.
    CHECK(a.add(header(6, aa::frame::kWhole, 30), Bytes({9}), m) == aa::MessageAssembler::kComplete);
    CHECK(m == Bytes({9}));
    CHECK(a.add(header(8, aa::frame::kFirst, 30, 2), Bytes({7}), m) == aa::MessageAssembler::kPending);
    CHECK(a.add(header(6, aa::frame::kLast, 30), Bytes({5, 6}), m) == aa::MessageAssembler::kComplete);
    CHECK(m == Bytes({1, 2, 3, 4, 5, 6}));
    CHECK(a.add(header(8, aa::frame::kLast, 30), Bytes({8}), m) == aa::MessageAssembler::kComplete);
    CHECK(m == Bytes({7, 8}));

    // A LAST alone is dropped; MIDDLE fragments before it count as a start.
    CHECK(a.add(header(6, aa::frame::kLast, 30), Bytes({1}), m) == aa::MessageAssembler::kNoFirst);
    CHECK(a.add(header(6, aa::frame::kMiddle, 30), Bytes({1}), m) == aa::MessageAssembler::kPending);
    CHECK(a.add(header(6, aa::frame::kLast, 30), Bytes({2}), m) == aa::MessageAssembler::kComplete);
    CHECK(m == Bytes({1, 2}));

    // Announced more than the wire brought: dropped. Announced 0: no check.
    CHECK(a.add(header(6, aa::frame::kFirst, 10, 100), Bytes({1}), m) == aa::MessageAssembler::kPending);
    CHECK(a.add(header(6, aa::frame::kLast, 20), Bytes({2}), m) == aa::MessageAssembler::kShortOfSize);
    CHECK(a.receivedBytes() == 30 && a.announcedBytes() == 100);
    CHECK(a.add(header(6, aa::frame::kFirst, 10, 0), Bytes({1}), m) == aa::MessageAssembler::kPending);
    CHECK(a.add(header(6, aa::frame::kLast, 20), Bytes({2}), m) == aa::MessageAssembler::kComplete);

    // A new FIRST replaces an unfinished message; drop() forgets one.
    CHECK(a.add(header(6, aa::frame::kFirst, 10, 1), Bytes({1}), m) == aa::MessageAssembler::kPending);
    CHECK(a.add(header(6, aa::frame::kFirst, 10, 1), Bytes({2}), m) == aa::MessageAssembler::kPending);
    CHECK(a.add(header(6, aa::frame::kLast, 10), Bytes({3}), m) == aa::MessageAssembler::kComplete);
    CHECK(m == Bytes({2, 3}));
    CHECK(a.add(header(6, aa::frame::kFirst, 10, 1), Bytes({1}), m) == aa::MessageAssembler::kPending);
    a.drop(6);
    CHECK(a.add(header(6, aa::frame::kLast, 10), Bytes({3}), m) == aa::MessageAssembler::kNoFirst);

    // Bounded: past 8 MiB on one channel the fragments go, whatever the frame.
    const Bytes big(aa::frame::kMaxMessage - 10, 0);
    CHECK(a.add(header(6, aa::frame::kFirst, 1, 9), big, m) == aa::MessageAssembler::kPending);
    CHECK(a.add(header(6, aa::frame::kMiddle, 1), Bytes(10, 0), m) == aa::MessageAssembler::kPending);
    CHECK(a.add(header(6, aa::frame::kWhole, 1), Bytes(1, 0), m) == aa::MessageAssembler::kOverflow);
    CHECK(a.add(header(6, aa::frame::kLast, 1), Bytes(1, 0), m) == aa::MessageAssembler::kNoFirst);
    a.clear();
}

}  // namespace

int main() {
    headers();
    reassembly();
    puts("PASS frames: header both ways, 64 KiB limit, reassembly per channel, LAST alone, short of size, 8 MiB bound");
    return 0;
}
