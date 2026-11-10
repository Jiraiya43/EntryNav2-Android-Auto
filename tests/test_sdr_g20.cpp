// Copyright (c) 2026 Jiraiya43
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// What the phone is told about this car must stay byte for byte the golden description (vehicleId included).
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <vector>

#include "dhu_sdp.h"
#include "evo_gps.hpp"
#include "sdr_builder.hpp"
#include "sdr_g20_golden.hpp"

namespace {

// With the car's GNSS ring (a fixture of its size) or without it (no file).
bool matches(bool carGps, const uint8_t* golden, size_t goldenLen) {
    const char* sensors = carGps ? "avec GPS voiture" : "sans GPS voiture";
    char ring[96];
    snprintf(ring, sizeof(ring), "/tmp/entry-sdr-g20-ring-%d", int(getpid()));
    unlink(ring);
    if (carGps) {
        FILE* f = fopen(ring, "wb");
        if (!f || ftruncate(fileno(f), 0x4258) != 0) {
            return false;
        }
        fclose(f);
    }
    evo::setCarGpsSourceForTest(ring);
    // "1.3.33": the software version the golden description was recorded with; the service announces its own.
    const std::vector<uint8_t> sdr =
        aa::buildCarServiceDiscovery(kDhuSdpProtobuf, kDhuSdpProtobufLen, nullptr, "1.3.33");
    if (sdr.size() != goldenLen) {
        printf("FAIL capteurs %s : %u octets, %u attendus\n", sensors, static_cast<unsigned>(sdr.size()),
               static_cast<unsigned>(goldenLen));
        return false;
    }
    for (size_t i = 0; i < sdr.size(); ++i) {
        if (sdr[i] != golden[i]) {
            printf("FAIL capteurs %s : octet %u different\n", sensors, static_cast<unsigned>(i));
            return false;
        }
    }
    unlink(ring);
    return true;
}

}  // namespace

int main() {
    // With the car GPS (LOCATION, NIGHT_MODE, DRIVING_STATUS) and without it.
    if (!matches(true, kSdrG20WithCarGps, sizeof(kSdrG20WithCarGps)) ||
        !matches(false, kSdrG20WithoutCarGps, sizeof(kSdrG20WithoutCarGps))) {
        return 1;
    }
    printf(
        "PASS description de la voiture identique au releve fige (%u octets avec GPS voiture, %u sans), seuls nom "
        "d'unite "
        "et build changent\n",
        static_cast<unsigned>(sizeof(kSdrG20WithCarGps)), static_cast<unsigned>(sizeof(kSdrG20WithoutCarGps)));
    return 0;
}
