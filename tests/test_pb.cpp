// Copyright (c) 2026 Jiraiya43
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// core/pb.hpp: protobuf writing, reading, and hostile phone messages. An alarm turns a reader that loops for ever
// into a failure. Built for the PC and for the car's ARM, where size_t is 32 bits.
#include <signal.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "pb.hpp"

#define CHECK(x)                                                \
    do {                                                        \
        if (!(x)) {                                             \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); \
            exit(1);                                            \
        }                                                       \
    } while (0)

namespace {

void hung(int) {
    static const char msg[] = "FAIL test-pb: a reader did not return (endless loop)\n";
    ssize_t w = write(1, msg, sizeof(msg) - 1);  // printf is not safe in a signal handler and would stay buffered
    (void)w;
    _exit(1);
}

bool find(const std::vector<uint8_t>& m, uint32_t field, uint64_t& v) {
    return aa::pb::findVarint(m.data(), m.size(), field, v);
}

bool same(const std::vector<uint8_t>& got, const std::vector<uint8_t>& want) {
    return got == want;
}

void writing() {
    using namespace aa::pb;
    std::vector<uint8_t> out;
    appendVarint(out, 0);
    appendVarint(out, 150);
    appendVarint(out, 0xffffffffffffffffull);
    CHECK(same(out, {0x00, 0x96, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x01}));

    out.clear();
    appendUint(out, 1, 150);
    appendString(out, 2, "hi");
    appendUint(out, 16, 1);                                  // two-byte key
    appendUint(out, 3, static_cast<uint64_t>(int64_t(-1)));  // negative int: ten bytes
    CHECK(same(out, {0x08, 0x96, 0x01, 0x12, 0x02, 'h',  'i',  0x80, 0x01, 0x01, 0x18,
                     0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x01}));

    std::vector<uint8_t> inner;
    appendUint(inner, 1, 7);
    out.clear();
    appendMessage(out, 5, inner);
    appendMessage(out, 6, std::vector<uint8_t>());
    CHECK(same(out, {0x2a, 0x02, 0x08, 0x07, 0x32, 0x00}));
}

void fieldByField() {
    using namespace aa::pb;
    // A string, a fixed32, a fixed64, then a varint.
    const std::vector<uint8_t> m = {0x0a, 0x02, 'h', 'i', 0x15, 1, 2, 3,    4,    0x19, 1,
                                    2,    3,    4,   5,   6,    7, 8, 0x20, 0x96, 0x01};
    FieldReader reader(m.data(), m.size());
    Field f;
    CHECK(reader.next(f) && f.number == 1 && f.type == kBytes && f.size == 2 && f.data[0] == 'h' && f.begin == 0 &&
          f.end == 4);
    CHECK(reader.next(f) && f.number == 2 && f.type == kFixed32 && f.value == 0x04030201u);
    CHECK(reader.next(f) && f.number == 3 && f.type == kFixed64 && f.value == 0x0807060504030201ull);
    CHECK(reader.next(f) && f.number == 4 && f.type == kVarint && f.value == 150 && f.end == m.size());
    CHECK(!reader.next(f) && !reader.malformed());

    // Wire types 3, 4, 6 and 7 are not read: malformed.
    const std::vector<uint8_t> group = {0x0b, 0x0c};
    FieldReader groups(group.data(), group.size());
    CHECK(!groups.next(f) && groups.malformed());

    // A rewrite drops field 1, replaces field 4, copies the fixed fields as they were.
    std::vector<uint8_t> out;
    CHECK(rewriteMessage(m.data(), m.size(), out, [](const Field& field, std::vector<uint8_t>& to) -> bool {
        if (field.number == 4) {
            appendUint(to, 4, 1);
        }
        return field.number == 1 || field.number == 4;
    }));
    CHECK(same(out, {0x15, 1, 2, 3, 4, 0x19, 1, 2, 3, 4, 5, 6, 7, 8, 0x20, 0x01}));
    const std::vector<uint8_t> cut = {0x08, 0x01, 0x12, 0x05, 'a'};
    out.clear();
    CHECK(!rewriteMessage(cut.data(), cut.size(), out, [](const Field&, std::vector<uint8_t>&) { return false; }));
    CHECK(same(out, {0x08, 0x01}));
}

void hostile() {
    uint64_t v = 0;
    const std::vector<uint8_t> normal = {0x0a, 0x02, 'h', 'i', 0x15, 1, 2, 3,    4,    0x19, 1,
                                         2,    3,    4,   5,   6,    7, 8, 0x20, 0x96, 0x01};
    CHECK(find(normal, 4, v) && v == 150);
    CHECK(!find(normal, 5, v));
    CHECK(!find(std::vector<uint8_t>(), 1, v));
    const std::vector<uint8_t> big = {0x08, 0xff, 0xff, 0xff, 0xff, 0x1f};  // over 32 bits
    CHECK(find(big, 1, v) && v == 0x1ffffffffull);

    // A 32-bit length that would wrap the position back, and a 64-bit length.
    const std::vector<uint8_t> wrap32 = {0x80, 0x01, 0x12, 0xfa, 0xff, 0xff, 0xff, 0x0f};
    CHECK(!find(wrap32, 1, v));
    const std::vector<uint8_t> wrap64 = {0x12, 0xf5, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x01};
    CHECK(!find(wrap64, 3, v));

    // Cut short: a fixed64 with 3 bytes left, a varint without its last byte, an 11-byte varint, field number 0.
    const std::vector<uint8_t> shortFixed = {0x19, 1, 2, 3};
    CHECK(!find(shortFixed, 1, v));
    const std::vector<uint8_t> shortVarint = {0x08, 0x96};
    CHECK(!find(shortVarint, 1, v));
    const std::vector<uint8_t> longVarint = {0x08, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x01};
    CHECK(!find(longVarint, 1, v));
    const std::vector<uint8_t> fieldZero = {0x00, 0x05, 0x08, 0x01};
    CHECK(!find(fieldZero, 1, v));
    CHECK(!find(normal, 0, v));
}

}  // namespace

int main() {
    signal(SIGALRM, hung);
    alarm(5);
    writing();
    fieldByField();
    hostile();
    printf("PASS test-pb (%u-bit size_t)\n", static_cast<unsigned>(sizeof(size_t) * 8));
    return 0;
}
