// Copyright (c) 2026 Jiraiya43
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Android Auto sensor list filter (see evo_sdr_sensors.cpp): keep only the sensors this port feeds.
#ifndef ENTRY_AAD_PLATFORM_EVO_SDR_SENSORS_HPP
#define ENTRY_AAD_PLATFORM_EVO_SDR_SENSORS_HPP

#include <stdint.h>

#include <vector>

namespace evo {
// Sensor types kept in the sensor channel: NIGHT_MODE (10) and DRIVING_STATUS (13), plus LOCATION (1) when the car's
// GNSS ring is there (carGps).
std::vector<int> keptSensorTypes(bool carGps);
// Removes from the ServiceDiscovery body every sensor whose type is not in keep. Returns the number removed;
// leaves the SDR untouched (returns 0) if it cannot be parsed or keep is empty.
int filterSdrSensors(std::vector<uint8_t>& sdr, const std::vector<int>& keep);
// The trim every program linking this file registers with the core (aa::setDiscoveryTrim): the sensors of this car,
// LOCATION only when its GNSS ring is there.
int trimDiscoverySensors(std::vector<uint8_t>& sdr);
}  // namespace evo

#endif
