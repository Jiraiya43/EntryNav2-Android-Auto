// Copyright (c) 2026 Jiraiya43
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// What the cluster shows for the phone's next maneuver: direction, distance and unit, lanes, roundabout exit.

// The cluster's values come from BMW.IC.Guidance. Checked on the car: the directions LEFT 6, RIGHT 7 and the flag 10,
// the lane arrows STRAIGHT 1, HALF_LEFT 2, LEFT 6 and RIGHT 7, the unit bytes 0 km, 1 mi, 3 m, 4 yd and 5 ft (a sweep
// of 0..15 on the HUD: 2 is mi too, 6 and above ft). The other values follow the names of the cluster's enums.
#ifndef ENTRY_AAD_PLATFORM_KOMBI_DISPLAY_HPP
#define ENTRY_AAD_PLATFORM_KOMBI_DISPLAY_HPP

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "kombi_phone.hpp"

namespace evo {
namespace kombi {

// The cluster's maneuver directions, which its lane arrows share.
enum Direction {
    kStraightOn = 1,
    kHalfLeft = 2,
    kHalfRight = 3,
    kTightLeft = 4,
    kTightRight = 5,
    kLeft = 6,
    kRight = 7,
    kUTurnLeft = 8,
    kUTurnRight = 9,
    kDestination = 10,
    kRoundaboutClockwise = 14,
    kRoundaboutCounterClockwise = 15,
    kHighwayInLeft = 25,
    kHighwayInRight = 26,
    kHighwayOutLeft = 28,
    kHighwayOutRight = 29,
    kLaneChangeLeft = 32,
    kLaneChangeRight = 33,
    kForkLeft = 37,
    kForkRight = 38,
};

// The unit byte after the cluster's distance text.
enum ClusterUnit {
    kNoClusterUnit = -1,  // keep the template's byte
    kClusterKilometres = 0,
    kClusterMiles = 1,
    kClusterMetres = 3,
    kClusterYards = 4,
    kClusterFeet = 5,
};

int clusterDirection(int maneuverType);
int clusterUnit(int displayUnit);
const char* clusterUnitName(int unit);  // for the log

// One lane as the cluster draws it, with one arrow, as in every factory lane model.
struct ClusterLane {
    enum Kind : uint8_t { kTake = 1, kOther = 2 };
    uint8_t kind;
    uint8_t arrow;  // Direction
    ClusterLane(uint8_t k, uint8_t a) : kind(k), arrow(a) {}
    bool operator==(const ClusterLane& o) const { return kind == o.kind && arrow == o.arrow; }
    bool operator!=(const ClusterLane& o) const { return !(*this == o); }
};
using ClusterLanes = std::vector<ClusterLane>;
const size_t kMaxLanes = 8;  // factory lane models have 2 to 7

// The phone's lanes as the cluster's, left to right. A lane shows its highlighted direction, else its first (as the
// factory navigation draws a straight-and-right lane straight). False, and nothing out, when there are no lanes or any
// can't be drawn (no direction, an unknown shape, more than kMaxLanes): no lane row rather than a wrong one.
bool clusterLanes(const std::vector<PhoneLane>& lanes, ClusterLanes& out);
std::string describeLanes(const ClusterLanes& lanes);  // for the log: "LEFT* STRAIGHT_ON RIGHT", * = to take

// The roundabout exit to show, 0 for none: the number written in the instruction first, else the exit field from 2
// up. On the car the field was 1 on every roundabout and 4 on plain turns (Waze), so a 1 proves nothing; better no
// exit than a wrong one.
int roundaboutExit(const Maneuver& m);
bool isRoundabout(int maneuverType);
// The exit ordinal in a French or English instruction ("2e sortie", "3rd exit", "deuxieme", "second exit"); 0 = none.
int exitInCue(const std::string& cue);

// Metres to the maneuver: the phone's value, else its display text read in its unit.
bool metresToManeuver(const Maneuver& m, double& metres);

// Distance text in one unit system. Miles + feet, as Google Maps shows it in the US: from 0.1 mi one-decimal miles,
// below that feet rounded down to 50.
void milesAndFeet(double metres, std::string& text, int& displayUnit);
// Metric: from 1 km one-decimal kilometres, below that metres (10 m steps under 100 m, then 50 m steps).
void metric(double metres, std::string& text, int& displayUnit);
// Miles + yards (UK): from 0.1 mi one-decimal miles, below that yards rounded down to 50.
void milesAndYards(double metres, std::string& text, int& displayUnit);
// In the system of the phone's unit, never metric to miles.
void inPhoneUnits(double metres, int phoneUnit, std::string& text, int& displayUnit);

// A distance text of at most maxChars characters with a cluster unit ("0.5 mi", "450 ft", "1.2 km", "300 m"): the
// phone's text when it fits, else rewritten from the metres in the phone's unit system; "-" when the distance is
// unknown.
void shortDistance(const Maneuver& m, size_t maxChars, std::string& text, int& displayUnit);

// UTF-8 helpers: the first maxChars code points (never half a character), and the number of code points.
std::string utf8Prefix(const std::string& s, size_t maxChars);
size_t utf8Length(const std::string& s);

}  // namespace kombi
}  // namespace evo

#endif
