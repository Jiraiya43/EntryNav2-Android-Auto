// Copyright (c) 2026 Jiraiya43
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Which original BMW SPP UUID pairs the supervisor accepts to save and later restore exactly.
#ifndef ENTRY_AAD_PLATFORM_SPP_BASELINE_HPP
#define ENTRY_AAD_PLATFORM_SPP_BASELINE_HPP
#include "evo_spp_snapshot.hpp"
namespace evo {
inline std::string uuidPairHex(const std::vector<uint8_t>& bytes) {
    const char* digits = "0123456789abcdef";
    std::string out;
    for (size_t i = 0; i < bytes.size(); ++i) {
        out += digits[bytes[i] >> 4];
        out += digits[bytes[i] & 15];
    }
    return out;
}
inline bool baselineSupported(const std::vector<uint8_t>& payload) {
    const std::string encoded = uuidPairHex(payload);
    // Generic SPP, or the distinct initiator/record UUIDs observed on the car (both copies in agreement).
    // Never substitute either constant for the actual snapshot when saving/restoring.
    return encoded == "011101011101" ||
           encoded == "0300000000decafadedecadeafdecacaff0300000000decafadedecadeafdecacafe";
}
inline bool snapshotMatchesBaseline(const SppSlotSnapshot& slot, const std::string& savedPair) {
    return slot.idle() && slot.uuidPayload == slot.transportUuidPayload && baselineSupported(slot.uuidPayload) &&
           uuidPairHex(slot.uuidPayload) == savedPair;
}
}  // namespace evo
#endif
