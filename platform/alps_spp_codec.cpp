// Copyright (c) 2026 Jiraiya43
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// UUID parsing and BtApplSppSetUuidReq payload building, without D-Bus (shared with the PC tests).
#include <cstring>

#include "alps_spp.hpp"

namespace evo {
static int hex(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}
bool parseUuid128(const char* s, uint8_t out[16]) {
    if (!s || !out || strlen(s) != 36) {
        return false;
    }
    uint8_t bytes[16];
    size_t j = 0;
    for (size_t i = 0; i < 36;) {
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            if (s[i++] != '-') {
                return false;
            }
        } else {
            int a = hex(s[i]), b = hex(s[i + 1]);
            if (a < 0 || b < 0) {
                return false;
            }
            bytes[j++] = static_cast<uint8_t>((a << 4) | b);
            i += 2;
        }
    }
    memcpy(out, bytes, sizeof(bytes));
    return true;
}
std::vector<uint8_t> buildSetUuidPayload(const uint8_t init[16], const uint8_t rec[16]) {
    std::vector<uint8_t> v;
    if (!init || !rec) {
        return v;
    }
    v.push_back(3);
    v.insert(v.end(), init, init + 16);
    v.push_back(3);
    v.insert(v.end(), rec, rec + 16);
    return v;
}
}  // namespace evo
