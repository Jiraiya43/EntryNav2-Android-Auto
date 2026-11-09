// Copyright (c) 2026 Jiraiya43
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Tests platform/kombi_phone.cpp: the next maneuver read from Android Auto's navigation messages, hostile ones too.
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "kombi_phone.hpp"
#include "kombi_test_util.hpp"

namespace {

using evo::kombi::Maneuver;
using evo::kombi::NavInbox;
using kombitest::Bytes;
using kombitest::Step;

Maneuver next(const NavInbox& in) {
    Maneuver m;
    CHECK(in.nextManeuver(m));
    return m;
}

// Seen on the car with Google Maps, parked: the step has no metres, only "0" ft.
void parkedPhone() {
    const NavInbox in = kombitest::inbox(kombitest::state(Step(1, "toward Main St").cue("toward Main St")),
                                         kombitest::position(-1, "0", evo::kombi::kUnitFeet));
    CHECK(in.guiding());
    const Maneuver m = next(in);
    CHECK(m.type == 1 && m.road == "toward Main St" && m.cue == "toward Main St");
    CHECK(m.distanceText == "0" && m.distanceUnit == evo::kombi::kUnitFeet && !m.hasMetres);
    CHECK(m.exitNumber == -1 && !m.hasExitAngle && m.lanes.empty());
}

void guidanceFollowsTheStatus() {
    const Bytes s = kombitest::state(Step(7, "R"));
    CHECK(kombitest::inbox(s, Bytes(), 1).guiding());
    CHECK(!kombitest::inbox(s, Bytes(), 2).guiding());  // inactive
    CHECK(!kombitest::inbox(s, Bytes(), 3).guiding());  // rerouting: the caller's grace period covers it
    NavInbox none;
    CHECK(!none.guiding());
    NavInbox text;
    kombitest::put(text, evo::kombi::kNavStatus, Bytes{0x0a, 0x01, '1'});  // status as a string: not a status
    CHECK(!text.guiding());
    NavInbox twice;
    kombitest::put(twice, evo::kombi::kNavStatus, Bytes{0x08, 0x01, 0x08, 0x02});  // the first value counts
    CHECK(twice.guiding());
    CHECK(!twice.keep(0x8001, nullptr, 0));  // a message the arrows don't read
    twice.clear();
    CHECK(!twice.guiding());
}

void stepAndDistance() {
    const Step roundabout = Step(35, "CIRCLE RD").exit(2).angle(90).cue("Au rond-point, prenez la 2e sortie");
    Maneuver m = next(kombitest::inbox(kombitest::state(roundabout, {Step(5, "OAK AVE")}),
                                       kombitest::position(320, "0.2", evo::kombi::kUnitMilesTenths)));
    CHECK(m.type == 35 && m.road == "CIRCLE RD" && m.exitNumber == 2 && m.hasExitAngle && m.exitAngle == 90);
    CHECK(m.cue == "Au rond-point, prenez la 2e sortie");
    CHECK(m.hasMetres && m.metres == 320 && m.distanceText == "0.2" && m.distanceUnit == 5);

    m = next(kombitest::inbox(kombitest::state(Step(8, "MAIN ST"))));  // the state came before its position
    CHECK(m.type == 8 && m.distanceText.empty() && !m.hasMetres && m.distanceUnit == 0);

    // Same turn: type, road and exit angle; the distance is compared on its own.
    Maneuver a, b;
    a.type = b.type = 7;
    a.road = b.road = "R";
    a.distanceText = "0.3";
    CHECK(a.sameTurn(b) && !a.sameDistance(b));
    b.hasExitAngle = true;
    CHECK(!a.sameTurn(b));
    a.hasExitAngle = true;
    a.exitAngle = 1;
    CHECK(!a.sameTurn(b));
}

void lanesLeftToRight() {
    const Step s = Step(7, "R")
                       .with(kombitest::lane(4, 1))
                       .with(kombitest::lane(1))
                       .with(kombitest::lane(1, 0))
                       .with(kombitest::lane(1, -1, 5, 2));
    const Maneuver m = next(kombitest::inbox(kombitest::state(s)));
    CHECK(m.lanes.size() == 4);
    CHECK(m.lanes[0].size() == 1 && m.lanes[0][0].shape == 4 && m.lanes[0][0].highlighted);
    CHECK(!m.lanes[1][0].highlighted && !m.lanes[2][0].highlighted);  // absent, and an explicit 0
    CHECK(m.lanes[3].size() == 2 && !m.lanes[3][0].highlighted && m.lanes[3][1].shape == 5 &&
          m.lanes[3][1].highlighted);
}

// NextTurnEvent and NextTurnDistanceEvent, from phones that send neither NavigationState nor its position.
void olderMessages() {
    struct Case {
        int event, side, type;
    } const cases[] = {
        {4, 1, 7},   {4, 2, 8},   {4, 0, 0},    // a turn without its side is not guessed
        {3, 1, 5},   {11, 1, 34}, {11, 2, 32},  // slight turn, roundabout
        {14, 0, 36}, {14, 2, 36}, {19, 0, 39},  // straight and destination have no side
        {15, 1, 0},  {99, 1, 0},
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        Bytes turn;
        aa::pb::appendString(turn, 1, "ELM ST");
        if (cases[i].side) {
            aa::pb::appendUint(turn, 2, static_cast<uint64_t>(cases[i].side));
        }
        aa::pb::appendUint(turn, 3, static_cast<uint64_t>(cases[i].event));
        aa::pb::appendUint(turn, 6, 45);
        NavInbox in = kombitest::inbox(Bytes());
        kombitest::put(in, evo::kombi::kNavTurn, turn);
        const Maneuver m = next(in);
        CHECK(m.type == cases[i].type && m.road == "ELM ST" && m.hasExitAngle && m.exitAngle == 45);
    }
    // Display distance in thousandths: tenths for the "_P1" units, else whole and rounded half to even.
    struct Distance {
        int64_t thousandths;
        int unit;
        const char* text;
    } const distances[] = {{200000, 6, "200"}, {2500, 6, "2"},   {3500, 6, "4"},  {2501, 6, "3"},     {499, 1, "0"},
                           {-2500, 1, "-2"},   {-3500, 1, "-4"}, {300, 5, "0.3"}, {12345, 3, "12.3"}, {499, 3, "0.5"}};
    Bytes turn;
    aa::pb::appendUint(turn, 2, 1);
    aa::pb::appendUint(turn, 3, 4);
    for (size_t i = 0; i < sizeof(distances) / sizeof(distances[0]); ++i) {
        Bytes event;
        aa::pb::appendUint(event, 1, 300);
        aa::pb::appendUint(event, 3, static_cast<uint64_t>(distances[i].thousandths));
        aa::pb::appendUint(event, 4, static_cast<uint64_t>(distances[i].unit));
        NavInbox in = kombitest::inbox(Bytes());
        kombitest::put(in, evo::kombi::kNavTurn, turn);
        kombitest::put(in, evo::kombi::kNavTurnDistance, event);
        const Maneuver m = next(in);
        CHECK(m.distanceText == distances[i].text && m.distanceUnit == distances[i].unit && m.metres == 300);
    }
    // The newer messages win over the older ones; an empty state doesn't count.
    NavInbox in = kombitest::inbox(kombitest::state(Step(8, "MAIN ST")), kombitest::position(480, "0.3", 5));
    kombitest::put(in, evo::kombi::kNavTurn, turn);
    Bytes event;
    aa::pb::appendUint(event, 1, 9);
    kombitest::put(in, evo::kombi::kNavTurnDistance, event);
    CHECK(next(in).type == 8 && next(in).metres == 480);
    kombitest::put(in, evo::kombi::kNavState, Bytes());
    CHECK(next(in).type == 7);
}

// Nothing a phone sends may crash the reader or make up a maneuver.
void hostileMessages() {
    Maneuver m;
    Bytes text;
    aa::pb::appendString(text, 1, "a string where the step belongs");
    CHECK(!kombitest::inbox(text).nextManeuver(m));
    CHECK(!kombitest::inbox(Bytes{0xff, 0xff, 0x07}).nextManeuver(m));
    const Bytes full = kombitest::state(Step(7, "R"));
    CHECK(!kombitest::inbox(Bytes(full.begin(), full.begin() + 9)).nextManeuver(m));
    NavInbox empty;
    CHECK(!empty.nextManeuver(m));

    // A maneuver whose bytes all read as text is taken for one: no type.
    Bytes maneuver, step, s;
    aa::pb::appendString(maneuver, 5, std::string(40, 'a'));
    aa::pb::appendMessage(step, 1, maneuver);
    aa::pb::appendMessage(s, 1, step);
    CHECK(kombitest::inbox(s).nextManeuver(m) && m.type == 0);

    // Field numbers above 2^29 - 1 are not protobuf: the message stops there.
    NavInbox big;
    kombitest::put(big, evo::kombi::kNavStatus, Bytes{0x80, 0x80, 0x80, 0x80, 0x10, 0x00, 0x08, 0x01});
    CHECK(!big.guiding());
    kombitest::put(big, evo::kombi::kNavStatus, Bytes{0x88, 0x80, 0x80, 0x80, 0x80, 0x01, 0x01});
    CHECK(!big.guiding());  // not field 1 cut to 32 bits either

    // Random bytes in every slot.
    uint64_t seed = 1;
    for (int i = 0; i < 20000; ++i) {
        NavInbox in = kombitest::inbox(Bytes());
        const uint16_t ids[] = {evo::kombi::kNavState, evo::kombi::kNavPosition, evo::kombi::kNavTurn,
                                evo::kombi::kNavTurnDistance};
        for (size_t k = 0; k < 4; ++k) {
            seed = seed * 6364136223846793005ULL + 1442695040888963407ULL;
            Bytes junk(static_cast<size_t>(seed >> 59) * 3);
            for (size_t j = 0; j < junk.size(); ++j) {
                seed = seed * 6364136223846793005ULL + 1442695040888963407ULL;
                junk[j] = static_cast<uint8_t>(seed >> 56);
            }
            kombitest::put(in, ids[k], junk);
        }
        if (in.nextManeuver(m)) {
            CHECK(m.lanes.size() < 64 && m.road.size() < 96);
        }
    }
}

}  // namespace

int main() {
    parkedPhone();
    guidanceFollowsTheStatus();
    stepAndDistance();
    lanesLeftToRight();
    olderMessages();
    hostileMessages();
    puts("PASS kombi phone: status, step, distance, lanes, older messages, hostile input");
    return 0;
}
