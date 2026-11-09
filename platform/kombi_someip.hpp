// Copyright (c) 2026 Jiraiya43
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// SOME/IP framing of the cluster's route-guidance service (BMW.IC.Guidance, service 0xB006). No I/O.

// A request is a 16-byte header and a body: service, method, length (8 + body), client, session, protocol version 1,
// interface version 1, message type, return code; all fields big-endian. The cluster answers each request with a
// RESPONSE (0x80) of the same method, client and session, return code E_OK (0) when it took it. Between answers it
// may send magic cookies (message id 0xFFFF8000), about every 10 s.
#ifndef ENTRY_AAD_PLATFORM_KOMBI_SOMEIP_HPP
#define ENTRY_AAD_PLATFORM_KOMBI_SOMEIP_HPP

#include <stddef.h>
#include <stdint.h>

#include <vector>

namespace evo {
namespace kombi {

using Bytes = std::vector<uint8_t>;

const uint16_t kGuidanceService = 0xB006;
const uint16_t kOurClient = 0x7E01;  // the head unit's own navigation uses another client id
const size_t kHeaderSize = 16;
const uint32_t kMaxLength = 4096;  // the largest length field accepted in a message from the cluster

// One request of the guidance service: its method and body; the header is added when it is sent.
struct Request {
    uint16_t method;
    Bytes body;
};

// The methods the arrows may send; never 0x0001 (create and dispose): the instance belongs to the factory navigation.
bool isAllowedMethod(uint16_t method);

// The request as sent: header (REQUEST, E_OK, our client id) then body.
Bytes encodeRequest(uint16_t method, const Bytes& body, uint16_t session);

// The fields of a received header that the client checks.
struct Header {
    uint16_t service;
    uint16_t method;
    uint32_t length;  // of what follows the length field: 8 + body
    uint16_t client;
    uint16_t session;
    uint8_t type;
    uint8_t code;
};
Header decodeHeader(const uint8_t* header);
bool isMagicCookie(const Header& h);
// The cluster took the request: a RESPONSE to it with E_OK.
bool acknowledges(const Header& reply, uint16_t method, uint16_t session);

// Big-endian fields.
uint32_t readBE32(const uint8_t* p);
void writeBE32(uint8_t* p, uint32_t value);
void appendBE32(Bytes& b, uint32_t value);

}  // namespace kombi
}  // namespace evo

#endif
