// Copyright (c) 2026 Jiraiya43
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Android Auto frame header and fragment reassembly (tests/test_aa_frame.cpp).
#include "aa_frame.hpp"

namespace aa {

FrameHeader decodeFrameHeader(const uint8_t* bytes) {
    FrameHeader header;
    header.channel = bytes[0];
    header.flags = bytes[1];
    header.length = static_cast<uint16_t>((bytes[2] << 8) | bytes[3]);
    header.totalSize = 0;
    return header;
}

uint32_t decodeTotalSize(const uint8_t* bytes) {
    uint32_t size = 0;
    for (int i = 0; i < 4; ++i) {
        size = (size << 8) | bytes[i];
    }
    return size;
}

bool encodeFrame(uint8_t channel, uint8_t flags, const uint8_t* payload, size_t length, std::vector<uint8_t>& out) {
    if (length > frame::kMaxPayload || (length > 0 && !payload)) {
        return false;
    }
    out.clear();
    out.reserve(frame::kHeaderSize + length);
    out.push_back(channel);
    out.push_back(flags);
    out.push_back(static_cast<uint8_t>(length >> 8));
    out.push_back(static_cast<uint8_t>(length & 0xFF));
    if (length > 0) {
        out.insert(out.end(), payload, payload + length);
    }
    return true;
}

MessageAssembler::Outcome MessageAssembler::add(const FrameHeader& header, const std::vector<uint8_t>& payload,
                                                std::vector<uint8_t>& message) {
    Pending& pending = pending_[header.channel];
    if (pending.data.size() + payload.size() > frame::kMaxMessage) {
        pending_.erase(header.channel);
        return kOverflow;
    }
    switch (header.position()) {
        case frame::kWhole:
            message = payload;
            return kComplete;
        case frame::kFirst:
            pending.data = payload;
            pending.sized = true;
            pending.size = header.totalSize;
            pending.onWire = header.length;
            return kPending;
        case frame::kMiddle:
            pending.data.insert(pending.data.end(), payload.begin(), payload.end());
            pending.onWire += header.length;
            return kPending;
        default:
            break;
    }
    // LAST. Fragments without a FIRST still count as a start once they brought data.
    const bool started = pending.sized || !pending.data.empty();
    const uint32_t onWire = pending.onWire + header.length;
    const uint32_t size = pending.sized ? pending.size : 0;
    pending.data.insert(pending.data.end(), payload.begin(), payload.end());
    message.swap(pending.data);
    pending_.erase(header.channel);
    if (!started) {
        return kNoFirst;
    }
    // The announced size counts plain bytes and the length fields count TLS records, which are larger: only a
    // shortfall tells that this LAST closed an incomplete message.
    if (size != 0 && onWire < size) {
        received_ = onWire;
        announced_ = size;
        return kShortOfSize;
    }
    return kComplete;
}

}  // namespace aa
