// Copyright (c) 2026 Jiraiya43
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// The ServiceDiscoveryResponse that describes this car to the phone, built from Google's DHU template.
#ifndef ENTRY_AAD_CORE_SDR_BUILDER_HPP
#define ENTRY_AAD_CORE_SDR_BUILDER_HPP

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

namespace aa {

// What the phone is told about the car and the head unit.
struct SdrIdentity {
    std::string carMake, carModel, carYear, vehicleId;
    std::string huMake, huModel, swBuild, swVersion;
    SdrIdentity();  // this car and head unit; swVersion "0.0.0" until the program gives its own
    // Model and year of a car other than the reference one, written at installation ("model=320d xDrive",
    // "year=2021", one per line). A missing file or an invalid line keeps the reference values. False if unreadable.
    bool loadVehicleFile(const char* path);
};
// Accepted model and year: 1 to 24 of A-Z a-z 0-9 space . _ -, and 1xxx or 2xxx.
bool validCarModel(const std::string& s);
bool validCarYear(const std::string& s);

// The steps of buildCarServiceDiscovery, in its order; each prints what it changed.
// The identity strings replaced, at the top level and in the head unit info (field 17); everything else untouched.
std::vector<uint8_t> buildServiceDiscovery(const uint8_t* base, size_t baseLen, const SdrIdentity& id);
// Main display (channel 6): density 140 (seen on the car: the rail icons no longer overlap) and a bottom inset of
// 240, so that the phone keeps its UI in the top 480 rows of its 1280x720 canvas, which the panel shows 1:1.
bool tuneMainDisplay(std::vector<uint8_t>& sdr);
// The touchscreen (channel 7) as 1280 x 720, the phone's canvas: touch coordinates go through unscaled.
bool declareTouchscreen(std::vector<uint8_t>& sdr);
// No media sink (channel 9): media plays over the car's Bluetooth. The phone refuses the session without an audio
// sink or without the microphone, so 8 (speech and guidance) and 14 stay. Returns the descriptors removed.
int removeMediaChannel(std::vector<uint8_t>& sdr);
// The channel 8 sink as SYSTEM_AUDIO instead of GUIDANCE: with neither a guidance nor a media sink the phone mixes
// the prompts into its own audio, which plays over Bluetooth. Returns the fields changed (0 or 1).
int retypeGuidanceSink(std::vector<uint8_t>& sdr);

// The platform's last word on the description, after removeMediaChannel (platform/evo_sdr_sensors.cpp registers
// one: the sensor list cut to what the car feeds). Returns what it removed.
using DiscoveryTrim = int (*)(std::vector<uint8_t>& sdr);
void setDiscoveryTrim(DiscoveryTrim trim);

// What the phone is told about this car: the template adapted by every step above. vehicleFile: the model
// and year (SdrIdentity::loadVehicleFile), given by the platform; null keeps the reference car. softwareVersion: the
// version of the program, announced as the head unit's software version; null keeps "0.0.0".
std::vector<uint8_t> buildCarServiceDiscovery(const uint8_t* base, size_t baseLen, const char* vehicleFile,
                                              const char* softwareVersion);

}  // namespace aa

#endif  // ENTRY_AAD_CORE_SDR_BUILDER_HPP
