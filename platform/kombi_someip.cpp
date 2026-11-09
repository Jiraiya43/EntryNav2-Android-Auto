// Copyright (c) 2026 Jiraiya43
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// SOME/IP headers of the guidance service: the requests the arrows send, the answers they check.
#include "kombi_someip.hpp"

namespace evo {
namespace kombi {

namespace {
const uint8_t kProtocolVersion = 1;
const uint8_t kInterfaceVersion = 1;
const uint8_t kTypeRequest = 0x00;
const uint8_t kTypeResponse = 0x80;
const uint8_t kCodeOk = 0x00;
const uint16_t kCookieService = 0xFFFF;
const uint16_t kCookieMethod = 0x8000;

uint16_t readBE16(const uint8_t* p) {
    return static_cast<uint16_t>(p[0] << 8 | p[1]);
}

void appendBE16(Bytes& b, uint16_t value) {
    b.push_back(static_cast<uint8_t>(value >> 8));
    b.push_back(static_cast<uint8_t>(value));
}
}  // namespace

bool isAllowedMethod(uint16_t method) {
    static const uint16_t kAllowed[] = {0x0002, 0x0003, 0x0004, 0x0009, 0x000A, 0x000B, 0x000C, 0x000E, 0x0010};
    for (size_t i = 0; i < sizeof(kAllowed) / sizeof(kAllowed[0]); ++i) {
        if (kAllowed[i] == method) {
            return true;
        }
    }
    return false;
}

Bytes encodeRequest(uint16_t method, const Bytes& body, uint16_t session) {
    Bytes out;
    out.reserve(kHeaderSize + body.size());
    appendBE16(out, kGuidanceService);
    appendBE16(out, method);
    appendBE32(out, static_cast<uint32_t>(8 + body.size()));
    appendBE16(out, kOurClient);
    appendBE16(out, session);
    out.push_back(kProtocolVersion);
    out.push_back(kInterfaceVersion);
    out.push_back(kTypeRequest);
    out.push_back(kCodeOk);
    out.insert(out.end(), body.begin(), body.end());
    return out;
}

Header decodeHeader(const uint8_t* header) {
    Header h;
    h.service = readBE16(header);
    h.method = readBE16(header + 2);
    h.length = readBE32(header + 4);
    h.client = readBE16(header + 8);
    h.session = readBE16(header + 10);
    h.type = header[14];
    h.code = header[15];
    return h;
}

bool isMagicCookie(const Header& h) {
    return h.service == kCookieService && h.method == kCookieMethod;
}

bool acknowledges(const Header& reply, uint16_t method, uint16_t session) {
    return reply.service == kGuidanceService && reply.method == method && reply.client == kOurClient &&
           reply.session == session && reply.type == kTypeResponse && reply.code == kCodeOk;
}

uint32_t readBE32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) << 24 | static_cast<uint32_t>(p[1]) << 16 | static_cast<uint32_t>(p[2]) << 8 |
           p[3];
}

void writeBE32(uint8_t* p, uint32_t value) {
    for (int i = 3; i >= 0; --i, value >>= 8) {
        p[i] = static_cast<uint8_t>(value);
    }
}

void appendBE32(Bytes& b, uint32_t value) {
    b.resize(b.size() + 4);
    writeBE32(&b[b.size() - 4], value);
}

}  // namespace kombi
}  // namespace evo
