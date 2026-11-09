// Copyright (c) 2026 Jiraiya43
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Tests platform/kombi_display.cpp: directions, units, lanes, distance texts and roundabout exits for the cluster.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "kombi_display.hpp"
#include "kombi_test_util.hpp"

namespace {

using namespace evo::kombi;

void directions() {
    // Checked on the car: left 6, right 7, the destination flag 10.
    CHECK(clusterDirection(7) == 6 && clusterDirection(8) == 7);
    CHECK(clusterDirection(39) == 10 && clusterDirection(42) == 10);
    const struct {
        int type, direction;
    } table[] = {{0, 1},   {1, 1},   {2, 1},   {3, 2},   {4, 3},   {5, 2},   {6, 3},   {9, 4},   {10, 5},
                 {11, 8},  {12, 9},  {13, 25}, {20, 26}, {21, 28}, {24, 29}, {25, 37}, {26, 38}, {27, 32},
                 {28, 33}, {29, 1},  {30, 15}, {32, 14}, {33, 14}, {34, 15}, {36, 1},  {37, 1},  {43, 14},
                 {44, 14}, {45, 15}, {46, 15}, {47, 1},  {-1, 1},  {999, 1}};
    for (size_t i = 0; i < sizeof(table) / sizeof(table[0]); ++i) {
        CHECK(clusterDirection(table[i].type) == table[i].direction);
    }
}

void units() {
    // The unit bytes read off the car's HUD: 0 km, 1 mi, 3 m, 4 yd, 5 ft.
    const int byDisplayUnit[] = {-1, 3, 0, 0, 1, 1, 5, 4, -1};
    for (int unit = 0; unit < 9; ++unit) {
        CHECK(clusterUnit(unit) == byDisplayUnit[unit]);
    }
    CHECK(!strcmp(clusterUnitName(kClusterKilometres), "km") && !strcmp(clusterUnitName(kClusterMetres), "m"));
    CHECK(!strcmp(clusterUnitName(2), "mi") && !strcmp(clusterUnitName(9), "ft"));
    CHECK(!strcmp(clusterUnitName(kNoClusterUnit), "(unit kept)"));
}

std::vector<PhoneLane> phoneLanes(const std::vector<kombitest::Lane>& in) {
    std::vector<PhoneLane> out;
    for (size_t i = 0; i < in.size(); ++i) {
        PhoneLane l;
        for (size_t k = 0; k < in[i].size(); ++k) {
            l.push_back(LaneShape(in[i][k].shape, in[i][k].highlight > 0));
        }
        out.push_back(l);
    }
    return out;
}

void lanes() {
    using kombitest::lane;
    ClusterLanes out;
    // The four junctions of a drive whose rows the car's cluster showed.
    const struct {
        std::vector<kombitest::Lane> phone;
        const char* shown;
    } junctions[] = {
        {{lane(4, 1), lane(1), lane(1), lane(1, 0, 5, 0)}, "LEFT* STRAIGHT_ON STRAIGHT_ON STRAIGHT_ON"},
        {{lane(4), lane(1), lane(1), lane(5, 1, 1, 0)}, "LEFT STRAIGHT_ON STRAIGHT_ON RIGHT*"},
        {{lane(4), lane(1), lane(5, 1)}, "LEFT STRAIGHT_ON RIGHT*"},
        {{lane(2, 1), lane(2, 1), lane(1), lane(1)}, "HALF_LEFT* HALF_LEFT* STRAIGHT_ON STRAIGHT_ON"},
    };
    for (size_t i = 0; i < sizeof(junctions) / sizeof(junctions[0]); ++i) {
        CHECK(clusterLanes(phoneLanes(junctions[i].phone), out) && describeLanes(out) == junctions[i].shown);
    }
    CHECK(out[0] == ClusterLane(ClusterLane::kTake, kHalfLeft) && out[2] != out[0]);
    // Every shape, the highlighted direction of a lane first.
    CHECK(clusterLanes(phoneLanes({lane(1, 0, 5, 1), lane(6, 0, 1, 0), lane(7), lane(8), lane(9), lane(3)}), out));
    CHECK(describeLanes(out) == "RIGHT* TIGHT_LEFT TIGHT_RIGHT UTURN_LEFT UTURN_RIGHT HALF_RIGHT");
    // No row rather than a wrong one: unknown shape, a lane without a direction, nine lanes; none at all.
    CHECK(!clusterLanes(phoneLanes({lane(4), lane(0)}), out) && out.empty());
    CHECK(!clusterLanes(phoneLanes({lane(4), kombitest::Lane()}), out) && out.empty());
    CHECK(!clusterLanes(phoneLanes({lane(10)}), out));
    CHECK(!clusterLanes(phoneLanes(std::vector<kombitest::Lane>(9, lane(1))), out));
    CHECK(clusterLanes(phoneLanes(std::vector<kombitest::Lane>(8, lane(1))), out) && out.size() == 8);
    CHECK(!clusterLanes(std::vector<PhoneLane>(), out) && describeLanes(out) == "none");
}

void roundaboutExits() {
    const struct {
        const char* cue;
        int exit;
    } cues[] = {
        {"Au rond-point, prenez la 2e sortie", 2},
        {"Prenez la 3\xc3\xa8me sortie", 3},
        {"Prenez la 3\xe8me sortie", 3},  // Latin-1
        {"Prenez la deuxi\xc3\xa8me sortie", 2},
        {"PREMIERE sortie", 1},
        {"Take the 2nd exit", 2},
        {"take the third exit", 3},
        {"Rue du 8 mai", 0},
        {"A7 sortie", 0},
        {"Take the 12th exit", 0},  // two digits: not read
        {"", 0},
    };
    for (size_t i = 0; i < sizeof(cues) / sizeof(cues[0]); ++i) {
        CHECK(exitInCue(cues[i].cue) == cues[i].exit);
    }
    Maneuver m;
    m.type = 34;
    m.exitNumber = 1;
    CHECK(roundaboutExit(m) == 0);  // Waze sends 1 on every roundabout
    m.exitNumber = 3;
    CHECK(roundaboutExit(m) == 3);
    m.exitNumber = 13;
    CHECK(roundaboutExit(m) == 0);
    m.cue = "prenez la 2e sortie";
    CHECK(roundaboutExit(m) == 2);  // the written instruction wins
    m.type = 8;
    m.exitNumber = 4;
    CHECK(roundaboutExit(m) == 0);  // a plain turn shows no exit (Waze sent 4 on one)
    CHECK(isRoundabout(30) && isRoundabout(35) && isRoundabout(43) && isRoundabout(46));
    CHECK(!isRoundabout(29) && !isRoundabout(36) && !isRoundabout(42) && !isRoundabout(47));
}

struct Shown {
    double metres;
    const char* text;
    int unit;
};

void distanceTexts() {
    const Shown feet[] = {{250, "0.2", kUnitMilesTenths},
                          {153, "0.1", kUnitMilesTenths},
                          {150, "450", kUnitFeet},
                          {137, "400", kUnitFeet},
                          {5, "50", kUnitFeet}};
    const Shown metres[] = {{1500, "1.5", kUnitKilometresTenths},
                            {999.6, "1.0", kUnitKilometresTenths},
                            {999, "950", kUnitMetres},
                            {330, "300", kUnitMetres},
                            {87, "80", kUnitMetres},
                            {3, "10", kUnitMetres}};
    const Shown yards[] = {{250, "0.2", kUnitMilesTenths}, {120, "100", kUnitYards}, {5, "50", kUnitYards}};
    std::string text;
    int unit;
    for (size_t i = 0; i < sizeof(feet) / sizeof(feet[0]); ++i) {
        milesAndFeet(feet[i].metres, text, unit);
        CHECK(text == feet[i].text && unit == feet[i].unit);
    }
    for (size_t i = 0; i < sizeof(metres) / sizeof(metres[0]); ++i) {
        metric(metres[i].metres, text, unit);
        CHECK(text == metres[i].text && unit == metres[i].unit);
    }
    for (size_t i = 0; i < sizeof(yards) / sizeof(yards[0]); ++i) {
        milesAndYards(yards[i].metres, text, unit);
        CHECK(text == yards[i].text && unit == yards[i].unit);
    }
    inPhoneUnits(1500, kUnitMetres, text, unit);
    CHECK(text == "1.5" && unit == kUnitKilometresTenths);  // never metric to miles
    inPhoneUnits(120, kUnitYards, text, unit);
    CHECK(text == "100" && unit == kUnitYards);
    inPhoneUnits(120, kUnitUnknown, text, unit);
    CHECK(text == "350" && unit == kUnitFeet);
}

void metresFromText() {
    Maneuver m;
    double metres = -1;
    m.distanceText = "0.5";
    m.distanceUnit = kUnitMilesTenths;
    CHECK(metresToManeuver(m, metres) && metres == 0.5 * 1609.344);
    m.distanceText = "300 \t";
    m.distanceUnit = kUnitFeet;
    CHECK(metresToManeuver(m, metres) && metres == 300 * 0.3048);
    m.distanceUnit = kUnitUnknown;
    CHECK(!metresToManeuver(m, metres));
    m.distanceUnit = kUnitYards;
    m.distanceText = "1,000";
    CHECK(!metresToManeuver(m, metres));
    m.distanceText = "";
    CHECK(!metresToManeuver(m, metres));
    m.hasMetres = true;
    m.metres = 42;
    CHECK(metresToManeuver(m, metres) && metres == 42);  // the phone's metres first
}

// At most three characters with a cluster unit: the phone's text when it fits, else from the metres in its system.
void shortTexts() {
    const struct {
        const char* phoneText;
        int phoneUnit;
        int metres;  // -1: none sent
        const char* text;
        int unit;
    } cases[] = {
        {"0.5", kUnitMilesTenths, 800, "0.5", kUnitMilesTenths},
        {"450", kUnitFeet, 137, "450", kUnitFeet},
        {"1,000", kUnitFeet, 305, "0.2", kUnitMilesTenths},
        {"12.5", kUnitMilesTenths, -1, "13", kUnitMilesTenths},
        {"1.2", kUnitKilometresTenths, 1200, "1.2", kUnitKilometresTenths},
        {"300", kUnitMetres, 300, "300", kUnitMetres},
        {"12.5", kUnitKilometresTenths, 12500, "13", kUnitKilometres},
        {"1,000", kUnitMetres, 1000, "1.0", kUnitKilometresTenths},
        {"xxxx", kUnitMetres, 5432, "5.4", kUnitKilometresTenths},
        {"350", kUnitYards, 320, "350", kUnitYards},
        {"xxxx", kUnitYards, 100, "100", kUnitYards},
        {"abc", kUnitUnknown, -1, "-", kUnitUnknown},
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        Maneuver m;
        m.distanceText = cases[i].phoneText;
        m.distanceUnit = cases[i].phoneUnit;
        m.hasMetres = cases[i].metres >= 0;
        m.metres = cases[i].metres;
        std::string text;
        int unit;
        shortDistance(m, 3, text, unit);
        CHECK(text == cases[i].text && unit == cases[i].unit);
    }
    // Far and near, any unit system: never more than three characters, never another system.
    const int far[] = {3, 10, 150, 999, 1000, 3000, 15000, 300000};
    const int units[] = {kUnitUnknown, kUnitKilometres, kUnitYards};
    for (size_t u = 0; u < 3; ++u) {
        for (size_t i = 0; i < sizeof(far) / sizeof(far[0]); ++i) {
            Maneuver m;
            m.distanceText = "xxxxxx";
            m.distanceUnit = units[u];
            m.hasMetres = true;
            m.metres = far[i];
            std::string text;
            int unit;
            shortDistance(m, 3, text, unit);
            CHECK(text.size() <= 3 && clusterUnit(unit) != kNoClusterUnit);
            CHECK((units[u] == kUnitKilometres) ==
                  (unit == kUnitMetres || unit == kUnitKilometres || unit == kUnitKilometresTenths));
        }
    }
}

void utf8() {
    const std::string strasse =
        "Stra\xc3\x9f"
        "e";
    CHECK(utf8Length(strasse) == 6 && utf8Prefix(strasse, 5) == "Stra\xc3\x9f" && utf8Prefix(strasse, 4) == "Stra");
    CHECK(utf8Prefix(std::string(300, 'X'), 40) == std::string(40, 'X'));
    CHECK(utf8Prefix("\xf0\x9f\x9a\x97x", 1) == "\xf0\x9f\x9a\x97" && utf8Prefix("ab\xe2\x82", 3) == "ab\xe2\x82");
}

}  // namespace

int main() {
    directions();
    units();
    lanes();
    roundaboutExits();
    distanceTexts();
    metresFromText();
    shortTexts();
    utf8();
    puts("PASS kombi display: directions, units, lanes, roundabout exits, distance texts");
    return 0;
}
