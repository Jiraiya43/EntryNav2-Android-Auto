// Copyright (c) 2026 Jiraiya43
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Trims the Android Auto sensor list to the sensors this port feeds.

// The core sends Google's DHU ServiceDiscovery template, whose sensor channel (channel 1) declares 13 sensors
// (LOCATION, COMPASS, SPEED, ODOMETER, FUEL, PARKING_BRAKE, GEAR, NIGHT_MODE, DRIVING_STATUS and others). Only the
// implemented ones are announced: day/night, driving status, and LOCATION when the BMW GNSS ring exists. LOCATION sends
// measured fixes and transient error/recovery events; without the ring the phone keeps its own GPS.
#include "evo_sdr_sensors.hpp"

#include <cstdio>

#include "evo_gps.hpp"
#include "pb.hpp"
#include "sdr_builder.hpp"

namespace evo {
namespace {

namespace pb = aa::pb;
typedef std::vector<uint8_t> Bytes;

// Service {1 id, 2 SensorSourceService {1 repeated Sensor {1 type}}}.
const uint32_t kServiceField = 1;
const uint32_t kSensorSourceField = 2;
const uint32_t kSensorField = 1;
const uint32_t kSensorTypeField = 1;

bool hasSensorSource(const pb::Field& service) {
    pb::FieldReader reader(service.data, service.size);
    pb::Field field;
    while (reader.next(field)) {
        if (pb::isMessage(field, kSensorSourceField)) {
            return true;
        }
    }
    return false;
}

bool kept(const pb::Field& sensor, const std::vector<int>& keep) {
    uint64_t type = 0;
    pb::findVarint(sensor.data, sensor.size, kSensorTypeField, type);
    for (size_t i = 0; i < keep.size(); ++i) {
        if (static_cast<uint64_t>(keep[i]) == type) {
            return true;
        }
    }
    return false;
}

// Every program linking this file announces the trimmed list: the core calls the trim once its own steps are done.
struct Registration {
    Registration() { aa::setDiscoveryTrim(trimDiscoverySensors); }
};
const Registration kRegistration;

}  // namespace

std::vector<int> keptSensorTypes(bool carGps) {
    // A supported native GNSS ring can acquire a fix after discovery; transient errors cover that interval.
    std::vector<int> keep;
    if (carGps) {
        keep.push_back(1);
    }
    keep.push_back(10);
    keep.push_back(13);
    return keep;
}

int filterSdrSensors(std::vector<uint8_t>& sdr, const std::vector<int>& keep) {
    if (keep.empty() || sdr.empty()) {
        return 0;
    }
    int removed = 0;
    bool innerParsed = true;
    Bytes out;
    // The sensor service of the sensor channel, its sensors not kept dropped; everything else copied.
    const auto dropSensors = [&](const pb::Field& sensor, Bytes&) -> bool {
        const bool drop = pb::isMessage(sensor, kSensorField) && !kept(sensor, keep);
        removed += drop ? 1 : 0;
        return drop;
    };
    const auto filterSource = [&](const pb::Field& part, Bytes& into) -> bool {
        if (!pb::isMessage(part, kSensorSourceField)) {
            return false;
        }
        Bytes source;
        innerParsed = pb::rewriteMessage(part.data, part.size, source, dropSensors) && innerParsed;
        pb::appendMessage(into, kSensorSourceField, source);
        return true;
    };
    const bool parsed = pb::rewriteMessage(&sdr[0], sdr.size(), out, [&](const pb::Field& service, Bytes& to) -> bool {
        if (!pb::isMessage(service, kServiceField) || !hasSensorSource(service)) {
            return false;
        }
        Bytes rebuilt;
        innerParsed = pb::rewriteMessage(service.data, service.size, rebuilt, filterSource) && innerParsed;
        pb::appendMessage(to, kServiceField, rebuilt);
        return true;
    });
    if (!parsed || !innerParsed) {
        return 0;  // unparsable: left as it was
    }
    if (removed) {
        sdr.swap(out);
    }
    return removed;
}

int trimDiscoverySensors(std::vector<uint8_t>& sdr) {
    const bool carGps = carGpsPresent();
    const int removed = filterSdrSensors(sdr, keptSensorTypes(carGps));
    if (removed) {
        printf("[SDR] sensors: %d removed, day/night + driving status; car GPS %s\n", removed,
               carGps ? "available" : "missing (phone GPS)");
    }
    return removed;
}

}  // namespace evo
