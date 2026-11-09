// Copyright (c) 2026 Jiraiya43
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// What the cluster shows, tick by tick: the guidance state machine between the phone's maneuver and the requests.

//   first maneuver           -> the whole route start, maneuver 0 patched (direction, distance and unit, road)
//   distance text changes    -> one distance update, at most every distanceEveryS
//   next maneuver            -> maneuver 0 updated in place (the factory's update block, the route stays); the whole
//                               route start after a gap in the guidance (a reroute) or once the cluster refused an
//                               update in place, until the next clear
//   lanes added or changed   -> the same, at most every laneGraphEveryS; then a route position the cluster checks the
//                               lane row against, at most every lanePositionEveryS and only when it moved by 1 m
//   no guidance for graceS   -> the factory cancel (the arrow is cleared)
// A request the cluster refuses, or a link that fails, stops the machine until resumeAfterStop(); it clears first.
// Single-threaded; now is in seconds of a monotonic clock.
#ifndef ENTRY_AAD_PLATFORM_KOMBI_GUIDE_HPP
#define ENTRY_AAD_PLATFORM_KOMBI_GUIDE_HPP

#include <stddef.h>

#include <string>
#include <vector>

#include "kombi_client.hpp"
#include "kombi_display.hpp"
#include "kombi_graph.hpp"
#include "kombi_phone.hpp"

namespace evo {
namespace kombi {

struct GuideSettings {
    enum InPlace { kFullGraphs = 0, kInPlace = 1, kInPlaceWithoutGuiding = 2 };

    double approachMiles;       // a maneuver shows only this close (0: at once), as a production navigation's HUD
    double hysteresis;          // once shown, the same maneuver hides again only past approachMiles * hysteresis
    size_t distanceChars;       // with the approach on: the distance text has at most this many characters
    bool hideBar;               // the red distance bar hidden: without a factory route its percentage means nothing
    double graceS;              // no guidance this long before the clear: rerouting blips keep the arrow
    double firstDistanceS;      // a first maneuver without a distance waits this long, then shows "-"
    double distanceEveryS;      // distance updates at most this often
    double staleS;              // guidance on but no message this long: guidance ended (read by the service)
    bool lanes;                 // the phone's lanes as the cluster's lane row
    double laneGraphEveryS;     // a lane change on the same maneuver sends a new graph at most this often
    double lanePositionEveryS;  // the route position for the lane row at most this often
    int inPlace;                // InPlace: how a new maneuver reaches a route already shown

    GuideSettings();
};

// Where the guide's requests go: the cluster link in the service, a recorder in tests.
class GuideOutput {
public:
    virtual ~GuideOutput() {}
    virtual SendOutcome send(const std::vector<Request>& requests, std::string* why) = 0;
    virtual bool cancel(std::string* why) = 0;  // the factory cancel; true: acknowledged
};

class Guide {
public:
    enum Stop { kRunning = 0, kStoppedLink, kStoppedRefused };

    Guide(const RouteGraph& graph, GuideOutput& output, const GuideSettings& settings);

    // One tick. next: the phone's next maneuver; null when there is no guidance (whyNone says why, for the log).
    void step(const Maneuver* next, double now, const char* whyNone = "guidance ended");
    // Clears the arrow when anything was sent.
    void clear(const char* why);
    // The BMW navigation guides: forget the arrow without a cancel, its graph replaces ours.
    void yieldToFactory();

    bool showing() const { return showing_; }
    bool sentAny() const { return sentAny_; }
    bool lastCancelOk() const { return cancelOk_; }
    Stop stopped() const { return stop_; }
    void resumeAfterStop() { stop_ = kRunning; }

private:
    // A condition and when it began.
    struct Since {
        bool on;
        double at;
        Since() : on(false), at(0) {}
        void begin(double now) {
            if (!on) {
                on = true;
                at = now;
            }
        }
        void end() { on = false; }
    };

    void noManeuver(double now, const char* why);
    bool holdForApproach(const Maneuver& m);
    bool settleDistance(Maneuver& m, double now);
    void fitForCluster(Maneuver& m) const;
    void show(const Maneuver& m, const ClusterLanes& lanes, bool newTurn, bool lanesChanged, double now);
    bool updateDistance(const Maneuver& m, double now);
    void reportPosition(const Maneuver& m, double now, bool forced);
    SendOutcome deliver(const char* what, const std::vector<Request>& requests, bool quiet);
    bool sendOrStop(const char* what, const std::vector<Request>& requests, bool quiet = false);
    void keepSection(const ClusterLanes& lanes, const LaneSection& section);
    void forget();

    const RouteGraph& graph_;
    GuideOutput& output_;
    const GuideSettings settings_;

    // On the cluster
    bool showing_;
    Maneuver shown_;           // as shown: distance, road and caps applied
    ClusterLanes shownLanes_;  // empty: no lane row
    double graphAt_;           // when the last graph went
    bool rebuildNext_;         // the next maneuver gets the whole route start: there was a gap in the guidance
    bool inPlaceOk_;           // false once the cluster refused an update in place, until the next clear
    Maneuver lastDistance_;
    double distanceAt_;
    Since gap_;         // no guidance
    Since noDistance_;  // a maneuver without a distance yet

    // The lane row's route position
    bool hasSection_;  // laneSection_ is the section of the last graph built with lanes
    LaneSection laneSection_;
    bool hasPosition_;
    double position_;
    double positionAt_;

    // The link
    bool sentAny_;
    bool cancelOk_;  // the last clear's cancel was acknowledged
    Stop stop_;
};

}  // namespace kombi
}  // namespace evo

#endif
