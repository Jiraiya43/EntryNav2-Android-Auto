// Copyright (c) 2026 Jiraiya43
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Car position as the Android Auto LOCATION sensor: message encoding and send rate.
#ifndef ENTRY_AAD_CORE_CAR_LOCATION_HPP
#define ENTRY_AAD_CORE_CAR_LOCATION_HPP

#include <stdint.h>
#include <time.h>

#include <vector>

#include "pb.hpp"

namespace aa {

// The monotonic clock in milliseconds: sample times, and the session's silence limit.
inline uint64_t locationClockMs() {
    timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return static_cast<uint64_t>(now.tv_sec) * 1000 + static_cast<uint64_t>(now.tv_nsec / 1000000);
}

// One fix of the car's receiver, in microdegrees. A value whose has* flag is false is not sent; these are the values
// CarPlay gives the iPhone (NMEA RMC/GGA).
struct CarLocation {
    int32_t latitudeE6, longitudeE6;
    uint64_t sampledMs;  // locationClockMs() when read from the car
    bool valid;
    bool hasSpeed, hasBearing, hasAltitude, hasAccuracy;
    double speedMps, bearingDeg, altitudeM, accuracyM;
    uint64_t utcSeconds;  // 0: not sent
    CarLocation()
        : latitudeE6(0),
          longitudeE6(0),
          sampledMs(0),
          valid(false),
          hasSpeed(false),
          hasBearing(false),
          hasAltitude(false),
          hasAccuracy(false),
          speedMps(0),
          bearingDeg(0),
          altitudeM(0),
          accuracyM(0),
          utcSeconds(0) {}
    // Valid, in range, and at most 5 s old.
    bool fresh(uint64_t nowMs) const {
        const bool inRange =
            latitudeE6 >= -90000000 && latitudeE6 <= 90000000 && longitudeE6 >= -180000000 && longitudeE6 <= 180000000;
        return valid && sampledMs <= nowMs && nowMs - sampledMs <= 5000 && inRange;
    }
};

// Scaled to an integer, rounded half away from zero.
inline uint64_t locationScaled(double value, double scale) {
    const double scaled = value * scale;
    return static_cast<uint64_t>(static_cast<int64_t>(scaled + (scaled < 0 ? -0.5 : 0.5)));
}

// SensorEventIndication {1: LocationData}, fields in ascending order: 1 timestamp (epoch seconds, deprecated, sent as
// OpenAuto does), 2 latitude and 3 longitude in signed E7 (degrees * 10^7, widened before the scaling; within int32
// even at 180 degrees; tests/test_gps_wire_oracle.py), 4 accuracy in mm, 5 altitude in cm, 6 speed in mm/s, 7 bearing
// in microdegrees. The speed goes as read: with the knots factor 1.94384 of OpenAuto and headunit, Waze showed
// 75-100 km/h at 50 km/h on the car.
inline std::vector<uint8_t> locationEvent(const CarLocation& p) {
    std::vector<uint8_t> data;
    if (p.utcSeconds) {
        pb::appendUint(data, 1, p.utcSeconds);
    }
    pb::appendUint(data, 2, static_cast<uint64_t>(static_cast<int64_t>(p.latitudeE6) * 10));
    pb::appendUint(data, 3, static_cast<uint64_t>(static_cast<int64_t>(p.longitudeE6) * 10));
    if (p.hasAccuracy) {
        pb::appendUint(data, 4, locationScaled(p.accuracyM, 1000));
    }
    if (p.hasAltitude) {
        pb::appendUint(data, 5, locationScaled(p.altitudeM, 100));
    }
    if (p.hasSpeed) {
        pb::appendUint(data, 6, locationScaled(p.speedMps, 1000));
    }
    if (p.hasBearing) {
        pb::appendUint(data, 7, locationScaled(p.bearingDeg, 1000000));
    }
    std::vector<uint8_t> event;
    event.push_back(0x80);  // SensorEventIndication 0x8003
    event.push_back(0x03);
    pb::appendMessage(event, 1, data);
    return event;
}

// SensorError 0x8004 {1: LOCATION, 2: SENSOR_OK (1) or SENSOR_ERROR_TRANSIENT (2)}.
inline std::vector<uint8_t> locationStatus(bool available) {
    std::vector<uint8_t> status;
    status.push_back(0x80);
    status.push_back(0x04);
    pb::appendUint(status, 1, 1);
    pb::appendUint(status, 2, available ? 1 : 2);
    return status;
}

// What goes to the phone once it started the sensor: the latest fix only, at a bounded rate, never a stale one; the
// sensor's state when the fix comes or goes. Guarded by the session's car input lock.
struct LocationFeed {
    CarLocation latest;
    bool started;
    int availability;  // -1 not told yet, 0 unavailable, 1 available
    uint64_t lastSentMs, lastSampleMs;
    uint32_t periodMs;
    LocationFeed() { reset(); }
    // Stopped, the latest fix kept.
    void reset() {
        started = false;
        availability = -1;
        lastSentMs = 0;
        lastSampleMs = 0;
        periodMs = 1000;
    }
    // The phone's period, between 1 s and 1 min.
    void subscribe(uint32_t requestedMs) {
        reset();
        started = true;
        periodMs = requestedMs < 1000 ? 1000 : requestedMs > 60000 ? 60000 : requestedMs;
    }
    std::vector<std::vector<uint8_t>> take(uint64_t nowMs) {
        std::vector<std::vector<uint8_t>> out;
        if (!started) {
            return out;
        }
        if (!latest.fresh(nowMs)) {
            if (availability != 0) {
                out.push_back(locationStatus(false));
            }
            availability = 0;
            lastSentMs = 0;
            return out;
        }
        if (availability == 0) {
            out.push_back(locationStatus(true));
        }
        availability = 1;
        // A new fix goes as soon as it comes: a plain 1 s timer beats against the 1 Hz receiver, skipping or repeating
        // fixes, which the phone shows as jumps. An unchanged fix goes again only after a clear silence.
        const uint64_t sinceSent = nowMs - lastSentMs;
        const bool newFix = latest.sampledMs != lastSampleMs && sinceSent + 200 >= periodMs;
        if (!lastSentMs || newFix || sinceSent >= static_cast<uint64_t>(periodMs) + 500) {
            out.push_back(locationEvent(latest));
            lastSentMs = nowMs;
            lastSampleMs = latest.sampledMs;
        }
        return out;
    }
};

}  // namespace aa

#endif  // ENTRY_AAD_CORE_CAR_LOCATION_HPP
