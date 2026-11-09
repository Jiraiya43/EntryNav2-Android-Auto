// Copyright (c) 2026 Jiraiya43
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// The guidance state machine: see kombi_guide.hpp.
#include "kombi_guide.hpp"

#include <cstdio>

#include "evo_log.hpp"

namespace evo {
namespace kombi {

namespace {
const char* const kTag = "KombiNav";
const size_t kMaxRoadChars = 40;  // the cluster shows far fewer
const size_t kMaxDistanceChars = 8;
const double kMetresPerMile = 1609.344;
const double kPositionStepM = 1.0;  // a smaller move sends no new route position
}  // namespace

GuideSettings::GuideSettings()
    : approachMiles(1.0),
      hysteresis(1.15),
      distanceChars(3),
      hideBar(true),
      graceS(8.0),
      firstDistanceS(3.0),
      distanceEveryS(0.5),
      staleS(300.0),
      lanes(true),
      laneGraphEveryS(3.0),
      lanePositionEveryS(1.0),
      inPlace(kInPlace) {}

Guide::Guide(const RouteGraph& graph, GuideOutput& output, const GuideSettings& settings)
    : graph_(graph),
      output_(output),
      settings_(settings),
      showing_(false),
      graphAt_(0),
      rebuildNext_(false),
      inPlaceOk_(true),
      distanceAt_(0),
      hasSection_(false),
      hasPosition_(false),
      position_(0),
      positionAt_(0),
      sentAny_(false),
      cancelOk_(true),
      stop_(kRunning) {
    laneSection_.maneuverAt = 0;
    laneSection_.span = 0;
}

void Guide::step(const Maneuver* next, double now, const char* whyNone) {
    if (stop_ != kRunning) {
        return;
    }
    if (!next) {
        noManeuver(now, whyNone);
        return;
    }
    gap_.end();
    Maneuver m = *next;
    if (holdForApproach(m) || !settleDistance(m, now)) {
        return;
    }
    fitForCluster(m);
    ClusterLanes lanes;
    if (settings_.lanes) {
        clusterLanes(m.lanes, lanes);  // empty when there are none, or none the cluster can draw
    }
    const bool newTurn = !showing_ || !m.sameTurn(shown_);
    if (!newTurn) {
        rebuildNext_ = false;  // the guidance came back on the same maneuver: nothing to rebuild
    }
    // Phones add, highlight again or drop lanes as the junction nears: the same maneuver's graph goes again with the
    // new row (what was proven on the car).
    const bool lanesChanged = !newTurn && lanes != shownLanes_ && now - graphAt_ >= settings_.laneGraphEveryS;
    if (newTurn || lanesChanged) {
        show(m, lanes, newTurn, lanesChanged, now);
        return;
    }
    if (!updateDistance(m, now)) {
        return;
    }
    if (!shownLanes_.empty()) {
        reportPosition(m, now, false);
        if (stop_ != kRunning) {
            return;
        }
    }
    shown_ = m;
}

void Guide::noManeuver(double now, const char* why) {
    noDistance_.end();
    if (!showing_) {
        return;
    }
    rebuildNext_ = true;  // a gap is likely a reroute: the next maneuver gets the whole route start
    gap_.begin(now);
    if (now - gap_.at >= settings_.graceS) {
        clear(why);
    }
}

// True when the maneuver is still too far to show (the arrow is cleared if one is up). An unknown distance shows.
// Hysteresis keeps the same maneuver; it is compared as the phone sent it with the one shown.
bool Guide::holdForApproach(const Maneuver& m) {
    if (settings_.approachMiles <= 0) {
        return false;
    }
    double metres;
    if (!metresToManeuver(m, metres)) {
        return false;
    }
    const bool same = showing_ && m.sameTurn(shown_);
    const double limit = settings_.approachMiles * kMetresPerMile * (same ? settings_.hysteresis : 1.0);
    if (metres <= limit) {
        return false;
    }
    if (showing_) {
        ELOG(kTag, "APPROACH: next maneuver %.1f mi away (> %g mi): hidden", metres / kMetresPerMile,
             settings_.approachMiles);
        clear("approach");
    }
    noDistance_.end();
    return true;
}

// Fills a missing distance: the one shown when the phone sent the step before its distance, else "-" after
// firstDistanceS (never the template's own "600"). False while still waiting.
bool Guide::settleDistance(Maneuver& m, double now) {
    if (m.distanceText.empty() && showing_ && !shown_.distanceText.empty() && shown_.distanceText != "-") {
        m.distanceText = shown_.distanceText;
        m.distanceUnit = shown_.distanceUnit;
        m.hasMetres = shown_.hasMetres;
        m.metres = shown_.metres;
    }
    if (!m.distanceText.empty()) {
        noDistance_.end();
        return true;
    }
    noDistance_.begin(now);
    if (!showing_ && now - noDistance_.at < settings_.firstDistanceS) {
        return false;
    }
    m.distanceText = "-";
    return true;
}

// The distance in a unit the cluster has, short enough; the roundabout exit and the caps on the road line.
void Guide::fitForCluster(Maneuver& m) const {
    std::string text;
    int unit;
    if (clusterUnit(m.distanceUnit) == kNoClusterUnit && m.hasMetres && m.distanceText != "-") {
        milesAndFeet(static_cast<double>(m.metres), text, unit);
        m.distanceText = text;
        m.distanceUnit = unit;
    }
    if (settings_.approachMiles > 0 && m.distanceText != "-") {
        shortDistance(m, settings_.distanceChars, text, unit);
        m.distanceText = text;
        m.distanceUnit = unit;
    }
    const int exit = roundaboutExit(m);
    if (exit > 0) {  // on the road line: the template has no roundabout model
        char label[24];
        snprintf(label, sizeof(label), "Sortie %d", exit);
        m.road = m.road.empty() ? std::string(label) : std::string(label) + " - " + m.road;
    }
    m.road = utf8Prefix(m.road.empty() ? std::string(" ") : m.road, kMaxRoadChars);
    m.distanceText = utf8Prefix(m.distanceText, kMaxDistanceChars);
}

void Guide::show(const Maneuver& m, const ClusterLanes& lanes, bool newTurn, bool lanesChanged, double now) {
    const int direction = clusterDirection(m.type), unit = clusterUnit(m.distanceUnit);
    const std::string laneNote = !lanes.empty() || lanesChanged ? " | lanes " + describeLanes(lanes) : std::string();
    if (newTurn && isRoundabout(m.type)) {
        ELOG(kTag, "roundabout: exit field %d, angle %s%d, cue '%s' -> exit shown %d", m.exitNumber,
             m.hasExitAngle ? "" : "missing ", m.hasExitAngle ? static_cast<int>(m.exitAngle) : 0, m.cue.c_str(),
             roundaboutExit(m));
    }
    ELOG(kTag, "%s type %d -> direction %d | %s %s | road '%s'%s", newTurn ? "SHOW" : "LANES", m.type, direction,
         m.distanceText.c_str(), clusterUnitName(unit), m.road.c_str(), laneNote.c_str());
    const ClusterManeuver shown(direction, m.road, m.distanceText, unit, lanes);
    std::vector<Request> requests;
    LaneSection section;
    bool sent = false;
    if (showing_ && !rebuildNext_ && inPlaceOk_ && settings_.inPlace > GuideSettings::kFullGraphs &&
        graph_.maneuverUpdate(shown, settings_.inPlace != GuideSettings::kInPlaceWithoutGuiding, requests, &section)) {
        keepSection(lanes, section);
        const SendOutcome outcome = deliver("maneuver", requests, false);
        if (outcome == kSent) {
            sent = true;
        } else if (outcome == kRefused) {
            ELOG(kTag, "the KOMBI refused the update in place: whole graphs until the next clear");
            inPlaceOk_ = false;
        } else {
            stop_ = kStoppedLink;
            clear("KOMBI link failed");
            return;
        }
    }
    if (!sent) {
        if (!graph_.routeStart(shown, requests, &section)) {
            ELOG(kTag, "internal error: the graph template has an unexpected layout");
            stop_ = kStoppedRefused;
            clear("template layout");
            return;
        }
        keepSection(lanes, section);
        if (!sendOrStop("graph", requests)) {
            return;
        }
    }
    rebuildNext_ = false;
    showing_ = true;
    shown_ = m;
    lastDistance_ = m;
    distanceAt_ = now;
    shownLanes_ = lanes;
    graphAt_ = now;
    hasPosition_ = false;
    if (!lanes.empty()) {
        reportPosition(m, now, true);  // the cluster checks the lane row on the next position
    }
}

// False when the machine stopped.
bool Guide::updateDistance(const Maneuver& m, double now) {
    if (m.sameDistance(lastDistance_) || now - distanceAt_ < settings_.distanceEveryS) {
        return true;
    }
    const int unit = clusterUnit(m.distanceUnit);
    std::vector<Request> requests;
    if (!graph_.distanceUpdate(m.distanceText, unit, requests)) {
        ELOG(kTag, "internal error: the distance template has an unexpected layout");
        stop_ = kStoppedRefused;
        clear("template layout");
        return false;
    }
    ELOG(kTag, "DIST %s %s", m.distanceText.c_str(), unit == kNoClusterUnit ? "" : clusterUnitName(unit));
    if (!sendOrStop("distance", requests)) {
        return false;
    }
    lastDistance_ = m;
    distanceAt_ = now;
    return true;
}

// The car's route position from the metres to the maneuver, so the cluster's lane check finds it in the section.
void Guide::reportPosition(const Maneuver& m, double now, bool forced) {
    if (!forced && now - positionAt_ < settings_.lanePositionEveryS) {
        return;
    }
    double metres = 0;
    const bool known = metresToManeuver(m, metres);
    if (!hasSection_) {
        return;
    }
    const double at = lanePosition(laneSection_, known, metres);
    if (!forced && hasPosition_ && at - position_ < kPositionStepM && position_ - at < kPositionStepM) {
        return;
    }
    std::vector<Request> requests;
    if (!graph_.positionUpdate(at, requests)) {
        ELOG(kTag, "internal error: the graph template has no destination info: lane row without position updates");
        return;
    }
    if (!sendOrStop("position", requests, true)) {
        return;
    }
    positionAt_ = now;
    hasPosition_ = true;
    position_ = at;
}

void Guide::keepSection(const ClusterLanes& lanes, const LaneSection& section) {
    if (!lanes.empty()) {
        laneSection_ = section;
        hasSection_ = true;
    }
}

SendOutcome Guide::deliver(const char* what, const std::vector<Request>& requests, bool quiet) {
    std::string why;
    const SendOutcome outcome = output_.send(requests, &why);
    sentAny_ = true;
    if (outcome == kSent && !quiet) {
        ELOG(kTag, "%s: %u request(s) sent", what, static_cast<unsigned>(requests.size()));
    } else if (outcome != kSent) {
        ELOG(kTag, "%s: %u request(s) FAILED%s%s", what, static_cast<unsigned>(requests.size()),
             why.empty() ? "" : ": ", why.c_str());
    }
    return outcome;
}

bool Guide::sendOrStop(const char* what, const std::vector<Request>& requests, bool quiet) {
    const SendOutcome outcome = deliver(what, requests, quiet);
    if (outcome == kSent) {
        return true;
    }
    stop_ = outcome == kRefused ? kStoppedRefused : kStoppedLink;
    clear(outcome == kRefused ? "KOMBI rejected a frame" : "KOMBI link failed");
    return false;
}

void Guide::clear(const char* why) {
    ELOG(kTag, "CLEAR (%s)", why);
    if (sentAny_ || showing_) {
        std::string detail;
        cancelOk_ = output_.cancel(&detail);
        ELOG(kTag, "cancel: %s%s%s", cancelOk_ ? "OK" : "FAILED", cancelOk_ ? "" : ": ",
             cancelOk_ ? "" : detail.c_str());
    }
    forget();
}

void Guide::yieldToFactory() {
    if (showing_ || sentAny_) {
        ELOG(kTag, "YIELD (iDrive route active): stop writing, no cancel");
    }
    forget();
}

void Guide::forget() {
    showing_ = false;
    shown_ = Maneuver();
    lastDistance_ = Maneuver();
    gap_.end();
    noDistance_.end();
    sentAny_ = false;
    shownLanes_.clear();
    hasPosition_ = false;
    rebuildNext_ = false;
    inPlaceOk_ = true;
}

}  // namespace kombi
}  // namespace evo
