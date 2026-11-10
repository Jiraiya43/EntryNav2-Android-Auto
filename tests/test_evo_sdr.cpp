// Copyright (c) 2026 Jiraiya43
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Sensor filter on the real DHU template, as entry-aad registers it with the core.

// Linked with platform/evo_sdr_sensors.cpp: channel 9 is still dropped by the core, the sensor channel is
// reduced to NIGHT_MODE (10) + DRIVING_STATUS (13) (plus LOCATION with the car's GNSS ring), and every other channel
// stays byte-identical. Also checks the vehicle file read by SdrIdentity.
#include <stdio.h>
#include <stdlib.h>

#include <map>
#include <vector>

#include "dhu_sdp.h"
#include "evo_gps.hpp"
#include "evo_sdr_sensors.hpp"
#include "sdr_builder.hpp"

#define CHECK(c)                                                 \
    do {                                                         \
        if (!(c)) {                                              \
            fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #c); \
            return 1;                                            \
        }                                                        \
    } while (0)

static bool varint(const std::vector<uint8_t>& b, size_t& i, uint64_t& v) {
    v = 0;
    for (int s = 0; i < b.size() && s < 64; s += 7) {
        uint8_t c = b[i++];
        v |= uint64_t(c & 0x7f) << s;
        if (!(c & 0x80)) {
            return true;
        }
    }
    return false;
}
struct F {
    int num, wt;
    uint64_t v;
    std::vector<uint8_t> body, raw;
};
static std::vector<F> fields(const std::vector<uint8_t>& b) {
    std::vector<F> out;
    size_t i = 0;
    while (i < b.size()) {
        size_t st = i;
        uint64_t k;
        if (!varint(b, i, k)) {
            break;
        }
        F f;
        f.num = int(k >> 3);
        f.wt = int(k & 7);
        f.v = 0;
        if (f.wt == 0) {
            varint(b, i, f.v);
        } else if (f.wt == 2) {
            uint64_t l;
            varint(b, i, l);
            f.body.assign(b.begin() + i, b.begin() + i + l);
            i += l;
        } else if (f.wt == 5) {
            i += 4;
        } else if (f.wt == 1) {
            i += 8;
        } else {
            break;
        }
        f.raw.assign(b.begin() + st, b.begin() + i);
        out.push_back(f);
    }
    return out;
}
// channel id -> raw bytes of its Service field; sensors of channel 1.
static std::map<uint64_t, std::vector<uint8_t>> channels(const std::vector<uint8_t>& sdr,
                                                         std::vector<uint64_t>& sensors) {
    std::map<uint64_t, std::vector<uint8_t>> m;
    sensors.clear();
    std::vector<F> top = fields(sdr);
    for (size_t i = 0; i < top.size(); ++i) {
        if (top[i].num != 1) {
            continue;
        }
        std::vector<F> svc = fields(top[i].body);
        uint64_t id = 999;
        for (size_t j = 0; j < svc.size(); ++j) {
            if (svc[j].num == 1 && svc[j].wt == 0) {
                id = svc[j].v;
            }
            if (svc[j].num == 2) {
                std::vector<F> ss = fields(svc[j].body);
                for (size_t k = 0; k < ss.size(); ++k) {
                    if (ss[k].num == 1) {
                        std::vector<F> s = fields(ss[k].body);
                        for (size_t q = 0; q < s.size(); ++q) {
                            if (s[q].num == 1) {
                                sensors.push_back(s[q].v);
                            }
                        }
                    }
                }
            }
        }
        m[id] = top[i].raw;
    }
    return m;
}

int main() {
    char gpsFile[96];
    snprintf(gpsFile, sizeof(gpsFile), "/tmp/entry-sdr-gps-test-%d", int(getpid()));
    evo::setCarGpsSourceForTest(gpsFile);
    unlink(gpsFile);
    std::vector<uint8_t> orig(kDhuSdpProtobuf, kDhuSdpProtobuf + kDhuSdpProtobufLen), sdr = orig;
    std::vector<uint64_t> s0, s1;
    std::map<uint64_t, std::vector<uint8_t>> c0 = channels(orig, s0);
    CHECK(s0.size() == 13 && c0.count(9) && c0.count(1));  // DHU: 13 sensor entries incl. LOCATION
    CHECK(aa::removeMediaChannel(sdr) == 1 && evo::trimDiscoverySensors(sdr) == 11);
    std::map<uint64_t, std::vector<uint8_t>> c1 = channels(sdr, s1);
    CHECK(!c1.count(9) && c1.size() == c0.size() - 1);
    CHECK(s1.size() == 2 && s1[0] == 10 && s1[1] == 13);  // NIGHT_MODE, DRIVING_STATUS only
    for (std::map<uint64_t, std::vector<uint8_t>>::iterator it = c1.begin(); it != c1.end(); ++it) {
        if (it->first != 1) {
            CHECK(it->second == c0[it->first]);  // other channels byte-identical
        }
    }
    CHECK(sdr.size() < orig.size());

    FILE* f = fopen(gpsFile, "wb");
    CHECK(f);
    CHECK(ftruncate(fileno(f), 0x4258) == 0);
    fclose(f);
    std::vector<uint8_t> withGps = orig;
    aa::removeMediaChannel(withGps);
    evo::trimDiscoverySensors(withGps);
    std::vector<uint64_t> carSensors;
    channels(withGps, carSensors);
    CHECK(carSensors.size() == 3 && carSensors[0] == 1);
    CHECK(evo::keptSensorTypes(true).size() == 3 && evo::keptSensorTypes(false).size() == 2);
    f = fopen(gpsFile, "wb");
    CHECK(f);
    fclose(f);  // unsupported layouts keep phone GPS
    CHECK(!evo::carGpsPresent());
    unlink(gpsFile);
    std::vector<uint8_t> junk(5, 0xff);  // unparsable input is left alone
    CHECK(evo::filterSdrSensors(junk, evo::keptSensorTypes(false)) == 0 && junk.size() == 5);
    // Vehicle file (written at installation for another car): model and year only, invalid lines ignored.
    char vehicle[96];
    snprintf(vehicle, sizeof(vehicle), "/tmp/entry-sdr-vehicle-%d", int(getpid()));
    f = fopen(vehicle, "w");
    CHECK(f);
    fputs("model=320d xDrive\r\nyear=2021\nmodel=bad;model\nyear=3021\nother=1\n", f);
    fclose(f);
    aa::SdrIdentity id;
    CHECK(id.carModel == "M340i" && id.carYear == "2019");
    CHECK(id.loadVehicleFile(vehicle) && id.carModel == "320d xDrive" && id.carYear == "2021");
    unlink(vehicle);
    aa::SdrIdentity none;
    CHECK(!none.loadVehicleFile(vehicle) && none.carModel == "M340i");
    CHECK(aa::validCarModel("X3 M40i") && !aa::validCarModel("") && !aa::validCarModel(std::string(25, 'a')) &&
          !aa::validCarModel("a'b") && aa::validCarYear("1999") && !aa::validCarYear("999") &&
          !aa::validCarYear("3000"));
    // The software version announced: the program's own when it gives one, "0.0.0" otherwise; nothing else moves.
    const std::vector<uint8_t> unversioned =
        aa::buildCarServiceDiscovery(kDhuSdpProtobuf, kDhuSdpProtobufLen, nullptr, nullptr);
    const std::vector<uint8_t> versioned =
        aa::buildCarServiceDiscovery(kDhuSdpProtobuf, kDhuSdpProtobufLen, nullptr, "12.34.56-rc.7");
    const std::string plain(unversioned.begin(), unversioned.end()), named(versioned.begin(), versioned.end());
    CHECK(plain.find("0.0.0") != std::string::npos && plain.find("12.34.56-rc.7") == std::string::npos);
    CHECK(named.find("12.34.56-rc.7") != std::string::npos && named.find("0.0.0") == std::string::npos);
    // Twice: at the top level and in the head unit info, 8 characters longer each time.
    CHECK(named.find("12.34.56-rc.7") != named.rfind("12.34.56-rc.7") && versioned.size() >= unversioned.size() + 16);
    puts(
        "PASS capteurs AA : modele DHU reduit a NIGHT_MODE + DRIVING_STATUS (le telephone garde son GPS), canal 9 "
        "toujours retire par le coeur, autres canaux identiques, fichier voiture (modele, annee), version annoncee");
    return 0;
}
