// Copyright (c) 2026 Jiraiya43
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// The phone's maneuver in the cluster's terms: direction and lane tables, distance texts, roundabout exit.
#include "kombi_display.hpp"

#include <cstdio>
#include <cstdlib>

namespace evo {
namespace kombi {

namespace {

const double kMetresPerMile = 1609.344;
const double kMetresPerYard = 0.9144;
const double kMetresPerFoot = 0.3048;
const double kFeetPerMetre = 3.28084;
const double kMilesShownAsTenth = 0.095;  // the least distance written "0.1" mi
const double kMetresShownAsKm = 999.5;    // the least distance written "1.0" km
const int kHighestExit = 12;

enum UnitSystem { kMetricSystem, kMilesYardsSystem, kMilesFeetSystem };

UnitSystem systemOf(int displayUnit) {
    switch (displayUnit) {
        case kUnitMetres:
        case kUnitKilometres:
        case kUnitKilometresTenths:
            return kMetricSystem;
        case kUnitYards:
            return kMilesYardsSystem;
        default:
            return kMilesFeetSystem;
    }
}

std::string oneDecimal(double value) {
    char text[32];
    snprintf(text, sizeof(text), "%.1f", value);
    return text;
}

std::string whole(int64_t value) {
    char text[32];
    snprintf(text, sizeof(text), "%lld", static_cast<long long>(value));
    return text;
}

// value rounded down to a multiple of step, and at least `least`.
int64_t steppedDown(double value, int64_t step, int64_t least) {
    const int64_t rounded = static_cast<int64_t>(value / static_cast<double>(step)) * step;
    return rounded < least ? least : rounded;
}

// The phone's lane shape as the cluster's arrow; -1 when the cluster has none. Checked on the car: straight, slight
// left, left and right; slight right by symmetry; sharp and U-turns by the cluster's enum names only.
int laneArrow(int shape) {
    static const int kByShape[] = {-1,     kStraightOn, kHalfLeft,   kHalfRight, kLeft,
                                   kRight, kTightLeft,  kTightRight, kUTurnLeft, kUTurnRight};
    return shape >= 0 && shape < static_cast<int>(sizeof(kByShape) / sizeof(kByShape[0])) ? kByShape[shape] : -1;
}

}  // namespace

int clusterDirection(int maneuverType) {
    // Indexed by Maneuver TYPE_*; any other type is straight on.
    static const uint8_t kByType[] = {// 0-2: unknown, depart, name change
                                      kStraightOn, kStraightOn, kStraightOn,
                                      // 3-6: keep left, keep right, slight left, slight right
                                      kHalfLeft, kHalfRight, kHalfLeft, kHalfRight,
                                      // 7-12: left, right, sharp left, sharp right, U-turn left, U-turn right
                                      kLeft, kRight, kTightLeft, kTightRight, kUTurnLeft, kUTurnRight,
                                      // 13-20: on-ramps, left then right
                                      kHighwayInLeft, kHighwayInRight, kHighwayInLeft, kHighwayInRight, kHighwayInLeft,
                                      kHighwayInRight, kHighwayInLeft, kHighwayInRight,
                                      // 21-24: off-ramps, left then right
                                      kHighwayOutLeft, kHighwayOutRight, kHighwayOutLeft, kHighwayOutRight,
                                      // 25-29: fork left, fork right, merge left, merge right, merge on no side
                                      kForkLeft, kForkRight, kLaneChangeLeft, kLaneChangeRight, kStraightOn,
                                      // 30-35: roundabouts; 32 and 33 turn clockwise (left-hand traffic)
                                      kRoundaboutCounterClockwise, kRoundaboutCounterClockwise, kRoundaboutClockwise,
                                      kRoundaboutClockwise, kRoundaboutCounterClockwise, kRoundaboutCounterClockwise,
                                      // 36-38: straight, ferries
                                      kStraightOn, kStraightOn, kStraightOn,
                                      // 39-42: destination: the checkered flag
                                      kDestination, kDestination, kDestination, kDestination,
                                      // 43-46: roundabouts; 43 and 44 turn clockwise
                                      kRoundaboutClockwise, kRoundaboutClockwise, kRoundaboutCounterClockwise,
                                      kRoundaboutCounterClockwise};
    const int known = static_cast<int>(sizeof(kByType) / sizeof(kByType[0]));
    return maneuverType >= 0 && maneuverType < known ? kByType[maneuverType] : static_cast<uint8_t>(kStraightOn);
}

int clusterUnit(int displayUnit) {
    switch (displayUnit) {
        case kUnitMetres:
            return kClusterMetres;
        case kUnitKilometres:
        case kUnitKilometresTenths:
            return kClusterKilometres;
        case kUnitMiles:
        case kUnitMilesTenths:
            return kClusterMiles;
        case kUnitFeet:
            return kClusterFeet;
        case kUnitYards:
            return kClusterYards;
        default:
            return kNoClusterUnit;
    }
}

const char* clusterUnitName(int unit) {
    static const char* const kNames[] = {"km", "mi", "mi", "m", "yd", "ft"};
    if (unit < 0) {
        return "(unit kept)";
    }
    return unit < static_cast<int>(sizeof(kNames) / sizeof(kNames[0])) ? kNames[unit] : "ft";
}

bool clusterLanes(const std::vector<PhoneLane>& lanes, ClusterLanes& out) {
    out.clear();
    if (lanes.empty() || lanes.size() > kMaxLanes) {
        return false;
    }
    for (size_t i = 0; i < lanes.size(); ++i) {
        const PhoneLane& lane = lanes[i];
        if (lane.empty()) {
            out.clear();
            return false;
        }
        size_t shown = 0;
        while (shown < lane.size() && !lane[shown].highlighted) {
            ++shown;
        }
        const bool take = shown < lane.size();
        const int arrow = laneArrow(lane[take ? shown : 0].shape);
        if (arrow < 0) {
            out.clear();
            return false;
        }
        out.push_back(ClusterLane(take ? ClusterLane::kTake : ClusterLane::kOther, static_cast<uint8_t>(arrow)));
    }
    return true;
}

std::string describeLanes(const ClusterLanes& lanes) {
    static const char* const kArrows[] = {"?",           "STRAIGHT_ON", "HALF_LEFT", "HALF_RIGHT", "TIGHT_LEFT",
                                          "TIGHT_RIGHT", "LEFT",        "RIGHT",     "UTURN_LEFT", "UTURN_RIGHT"};
    if (lanes.empty()) {
        return "none";
    }
    std::string text;
    for (size_t i = 0; i < lanes.size(); ++i) {
        const uint8_t arrow = lanes[i].arrow;
        text += i ? " " : "";
        text += arrow < sizeof(kArrows) / sizeof(kArrows[0]) ? kArrows[arrow] : kArrows[0];
        text += lanes[i].kind == ClusterLane::kTake ? "*" : "";
    }
    return text;
}

bool isRoundabout(int maneuverType) {
    return (maneuverType >= 30 && maneuverType <= 35) || (maneuverType >= 43 && maneuverType <= 46);
}

int exitInCue(const std::string& cue) {
    std::string lower(cue);
    for (size_t i = 0; i < lower.size(); ++i) {
        if (lower[i] >= 'A' && lower[i] <= 'Z') {
            lower[i] = static_cast<char>(lower[i] - 'A' + 'a');
        }
    }
    // A digit 1-9 not after another digit, then an ordinal suffix: e, r, s, n, t, or the first byte of an accented e
    // (0xC3 in UTF-8, 0xE8 in Latin-1).
    const std::string suffixes = "erstn\xc3\xe8";
    for (size_t i = 0; i + 1 < lower.size(); ++i) {
        const char c = lower[i];
        const bool afterDigit = i > 0 && lower[i - 1] >= '0' && lower[i - 1] <= '9';
        if (c >= '1' && c <= '9' && !afterDigit && suffixes.find(lower[i + 1]) != std::string::npos) {
            return c - '0';
        }
    }
    // Else an ordinal word; the first of this list found anywhere wins.
    static const struct {
        const char* word;
        int exit;
    } kWords[] = {{"premi", 1},  {"deuxi", 2},  {"seconde", 2}, {"second ", 2}, {"troisi", 3},
                  {"quatri", 4}, {"cinqui", 5}, {"sixi", 6},    {"first", 1},   {"third", 3},
                  {"fourth", 4}, {"fifth", 5},  {"sixth", 6}};
    for (size_t k = 0; k < sizeof(kWords) / sizeof(kWords[0]); ++k) {
        if (lower.find(kWords[k].word) != std::string::npos) {
            return kWords[k].exit;
        }
    }
    return 0;
}

int roundaboutExit(const Maneuver& m) {
    if (!isRoundabout(m.type)) {
        return 0;
    }
    const int written = exitInCue(m.cue);
    if (written >= 1 && written <= kHighestExit) {
        return written;
    }
    return m.exitNumber >= 2 && m.exitNumber <= kHighestExit ? m.exitNumber : 0;
}

bool metresToManeuver(const Maneuver& m, double& metres) {
    if (m.hasMetres) {
        metres = static_cast<double>(m.metres);
        return true;
    }
    double perUnit;
    switch (m.distanceUnit) {
        case kUnitMetres:
            perUnit = 1.0;
            break;
        case kUnitKilometres:
        case kUnitKilometresTenths:
            perUnit = 1000.0;
            break;
        case kUnitMiles:
        case kUnitMilesTenths:
            perUnit = kMetresPerMile;
            break;
        case kUnitFeet:
            perUnit = kMetresPerFoot;
            break;
        case kUnitYards:
            perUnit = kMetresPerYard;
            break;
        default:
            return false;
    }
    const char* text = m.distanceText.c_str();
    char* end = nullptr;
    const double value = strtod(text, &end);
    if (end == text) {
        return false;
    }
    while (*end == ' ' || *end == '\t') {
        ++end;
    }
    if (*end != '\0') {
        return false;
    }
    metres = value * perUnit;
    return true;
}

void milesAndFeet(double metres, std::string& text, int& displayUnit) {
    const double miles = metres / kMetresPerMile;
    if (miles >= kMilesShownAsTenth) {
        text = oneDecimal(miles);
        displayUnit = kUnitMilesTenths;
        return;
    }
    text = whole(steppedDown(metres * kFeetPerMetre, 50, 50));
    displayUnit = kUnitFeet;
}

void metric(double metres, std::string& text, int& displayUnit) {
    if (metres >= kMetresShownAsKm) {
        text = oneDecimal(metres / 1000.0);
        displayUnit = kUnitKilometresTenths;
        return;
    }
    text = whole(steppedDown(metres, metres < 100.0 ? 10 : 50, 10));
    displayUnit = kUnitMetres;
}

void milesAndYards(double metres, std::string& text, int& displayUnit) {
    const double miles = metres / kMetresPerMile;
    if (miles >= kMilesShownAsTenth) {
        text = oneDecimal(miles);
        displayUnit = kUnitMilesTenths;
        return;
    }
    text = whole(steppedDown(metres / kMetresPerYard, 50, 50));
    displayUnit = kUnitYards;
}

void inPhoneUnits(double metres, int phoneUnit, std::string& text, int& displayUnit) {
    switch (systemOf(phoneUnit)) {
        case kMetricSystem:
            metric(metres, text, displayUnit);
            break;
        case kMilesYardsSystem:
            milesAndYards(metres, text, displayUnit);
            break;
        case kMilesFeetSystem:
            milesAndFeet(metres, text, displayUnit);
            break;
    }
}

void shortDistance(const Maneuver& m, size_t maxChars, std::string& text, int& displayUnit) {
    if (utf8Length(m.distanceText) <= maxChars && clusterUnit(m.distanceUnit) != kNoClusterUnit) {
        text = m.distanceText;
        displayUnit = m.distanceUnit;
        return;
    }
    double metres;
    if (!metresToManeuver(m, metres)) {
        text = "-";
        displayUnit = m.distanceUnit;
        return;
    }
    inPhoneUnits(metres, m.distanceUnit, text, displayUnit);
    if (text.size() > maxChars) {  // too far for tenths: whole kilometres or miles
        if (systemOf(m.distanceUnit) == kMetricSystem) {
            text = whole(static_cast<int64_t>(metres / 1000.0 + 0.5));
            displayUnit = kUnitKilometres;
        } else {
            text = whole(static_cast<int64_t>(metres / kMetresPerMile + 0.5));
            displayUnit = kUnitMilesTenths;
        }
    }
    text = utf8Prefix(text, maxChars);
}

std::string utf8Prefix(const std::string& s, size_t maxChars) {
    size_t end = 0;
    for (size_t chars = 0; end < s.size() && chars < maxChars; ++chars) {
        const unsigned char lead = static_cast<unsigned char>(s[end]);
        size_t width = 1;
        if ((lead & 0xE0) == 0xC0) {
            width = 2;
        } else if ((lead & 0xF0) == 0xE0) {
            width = 3;
        } else if ((lead & 0xF8) == 0xF0) {
            width = 4;
        }
        end += width < s.size() - end ? width : s.size() - end;
    }
    return s.substr(0, end);
}

size_t utf8Length(const std::string& s) {
    size_t chars = 0;
    for (size_t i = 0; i < s.size(); ++i) {
        chars += (static_cast<unsigned char>(s[i]) & 0xC0) != 0x80;
    }
    return chars;
}

}  // namespace kombi
}  // namespace evo
