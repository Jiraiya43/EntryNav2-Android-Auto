// Copyright (c) 2026 Jiraiya43
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Android Auto's navigation messages (channel 10) and the next maneuver read from them. No I/O.

// Field numbers are those of aasdk's navigationstatus protos; maneuver types are androidx.car.app.navigation.model
// Maneuver TYPE_*:
//   0x8003 NavigationStatus           {1 status: 1 active, 2 inactive, 3 rerouting}
//   0x8006 NavigationState            {1 steps[] {1 maneuver {1 type, 2 roundabout exit, 3 exit angle},
//                                      2 road {1 name}, 3 lanes[] {1 directions[] {1 shape, 2 highlighted}},
//                                      4 cue {1 texts[]}}, 2 destinations[]}
//   0x8007 NavigationCurrentPosition  {1 step distance {1 distance {1 metres, 2 display text, 3 display unit},
//                                      2 seconds}, 2 destination distances[], 3 current road}
//   0x8004 NextTurnEvent, 0x8005 NextTurnDistanceEvent: what phones without 0x8006 and 0x8007 send.
// Fields are read with core/pb.hpp, each from its first occurrence: when that one has another wire type, the field
// counts as absent. A length-delimited field reads as a string when it is UTF-8 text, otherwise as a message.
#ifndef ENTRY_AAD_PLATFORM_KOMBI_PHONE_HPP
#define ENTRY_AAD_PLATFORM_KOMBI_PHONE_HPP

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

namespace evo {
namespace kombi {

// Message ids of the navigation channel that the arrows read.
enum NavMessageId : uint16_t {
    kNavStatus = 0x8003,
    kNavTurn = 0x8004,
    kNavTurnDistance = 0x8005,
    kNavState = 0x8006,
    kNavPosition = 0x8007,
};

// The phone's display units for a distance.
enum DisplayUnit {
    kUnitUnknown = 0,
    kUnitMetres = 1,
    kUnitKilometres = 2,
    kUnitKilometresTenths = 3,
    kUnitMiles = 4,
    kUnitMilesTenths = 5,
    kUnitFeet = 6,
    kUnitYards = 7,
};

// One direction of a lane: LaneDirection.Shape (0 unknown, 1 straight, 2/3 slight left/right, 4/5 left/right, 6/7 sharp
// left/right, 8/9 U-turn left/right), and whether the phone highlights it as a lane to take.
struct LaneShape {
    int shape;
    bool highlighted;
    LaneShape(int s, bool h) : shape(s), highlighted(h) {}
};
using PhoneLane = std::vector<LaneShape>;

// The next maneuver, in the phone's terms.
struct Maneuver {
    int type;
    std::string road;
    std::string cue;  // the step's instruction, its first text
    int exitNumber;   // NavigationManeuver's roundabout exit number as sent; -1 when absent
    bool hasExitAngle;
    int64_t exitAngle;
    std::string distanceText;  // the phone's display value: "0.3", "600"
    int distanceUnit;          // DisplayUnit
    bool hasMetres;
    int64_t metres;                // to the maneuver
    std::vector<PhoneLane> lanes;  // left to right; phones send them near the junction only

    Maneuver();
    // Same type, road and exit angle: the same turn, whatever its distance.
    bool sameTurn(const Maneuver& other) const;
    bool sameDistance(const Maneuver& other) const;
};

// The newest body of each message the arrows read. A plain value: the service copies it from the session thread.
class NavInbox {
public:
    // Keeps the body when the arrows read this message; false for any other message.
    bool keep(uint16_t id, const uint8_t* body, size_t size);
    void clear();
    // Guidance is on only while the latest NavigationStatus says active: rerouting counts as off, and the caller's
    // grace period keeps the arrow through it.
    bool guiding() const;
    // The next maneuver, from NavigationState (else NextTurnEvent) and the distance from NavigationCurrentPosition
    // (else NextTurnDistanceEvent). False when there is no maneuver to show.
    bool nextManeuver(Maneuver& out) const;

private:
    std::vector<uint8_t> status_, turn_, turnDistance_, state_, position_;
};

}  // namespace kombi
}  // namespace evo

#endif
