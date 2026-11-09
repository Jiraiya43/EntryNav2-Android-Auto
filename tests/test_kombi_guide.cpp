// Copyright (c) 2026 Jiraiya43
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Tests platform/kombi_guide.cpp: what the guidance state machine sends, tick by tick, to a recorder.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "kombi_guide.hpp"
#include "kombi_test_util.hpp"

namespace {

using namespace evo::kombi;
using kombitest::lane;
using kombitest::modelIndex;
using kombitest::modelType;
using kombitest::Step;
using kombitest::textIn;

// Records what the guide sends; the outcomes can be scripted, one per send.
class Recorder : public GuideOutput {
public:
    Recorder() : cancels(0), cancelAnswer(true) {}
    SendOutcome send(const std::vector<Request>& requests, std::string*) override {
        SendOutcome outcome = kSent;
        if (!script.empty()) {
            outcome = script.front();
            script.erase(script.begin());
        }
        if (outcome == kSent) {
            sent.insert(sent.end(), requests.begin(), requests.end());
            batches.push_back(requests.size());
        }
        return outcome;
    }
    bool cancel(std::string*) override {
        ++cancels;
        return cancelAnswer;
    }

    std::vector<Request> sent;
    std::vector<size_t> batches;
    std::vector<SendOutcome> script;
    int cancels;
    bool cancelAnswer;
};

GuideSettings fullGraphs() {
    GuideSettings s;
    s.approachMiles = 0;
    s.distanceEveryS = 0;
    s.inPlace = GuideSettings::kFullGraphs;
    return s;
}

GuideSettings inPlace(int mode = GuideSettings::kInPlace) {
    GuideSettings s = fullGraphs();
    s.inPlace = mode;
    return s;
}

// A guide with its graph and recorder.
struct Rig {
    RouteGraph graph;
    Recorder out;
    Guide guide;
    explicit Rig(const GuideSettings& s = fullGraphs()) : guide(graph, out, s) {}
    void tick(double t, const NavInbox& in) {
        Maneuver m;
        const bool shown = in.guiding() && in.nextManeuver(m);
        guide.step(shown ? &m : nullptr, t);
    }
    void tick(double t, const Step& s, int64_t metres, const char* text, int unit) {
        tick(t, kombitest::inbox(kombitest::state(s), kombitest::position(metres, text, unit)));
    }
    size_t count() const { return out.sent.size(); }
    // method/model type of requests [from, to)
    std::string shape(size_t from, size_t to) const {
        std::string s;
        char b[16];
        for (size_t i = from; i < to && i < out.sent.size(); ++i) {
            const Request& r = out.sent[i];
            snprintf(b, sizeof(b), "%x/%x ", r.method, r.method == 0x0004 ? 0 : modelType(r.body));
            s += b;
        }
        return s;
    }
    size_t countOf(uint16_t method, size_t from = 0) const {
        size_t n = 0;
        for (size_t i = from; i < out.sent.size(); ++i) {
            n += out.sent[i].method == method;
        }
        return n;
    }
    // The route position of the last destination info sent, -1 if none.
    double lastPosition() const {
        for (size_t i = out.sent.size(); i-- > 0;) {
            const Request& r = out.sent[i];
            if (r.method == 0x0009 && modelType(r.body) == 17 && r.body.size() >= 30) {
                uint64_t bits = 0;
                for (int k = 0; k < 8; ++k) {
                    bits = bits << 8 | r.body[22 + k];
                }
                double at;
                memcpy(&at, &bits, sizeof(at));
                return at;
            }
        }
        return -1;
    }
    // The text and unit byte of the last maneuver-0 distance sent.
    std::string lastDistance(int* unit = nullptr) const {
        for (size_t i = out.sent.size(); i-- > 0;) {
            const Request& r = out.sent[i];
            if (r.method == 0x000A && modelType(r.body) == 29 && modelIndex(r.body) == 0) {
                return textIn(r.body, unit);
            }
        }
        return "<none>";
    }
    std::string lastRoad() const {
        for (size_t i = out.sent.size(); i-- > 0;) {
            const Request& r = out.sent[i];
            if (r.method == 0x0003 && modelType(r.body) == 3 && modelIndex(r.body) == 0) {
                return textIn(r.body);
            }
        }
        return "<none>";
    }
};

const char* const kUpdateBlock = "3/3 e/28 a/22 b/7 a/1d a/f c/20 4/0 9/21 ";

void demoDriveInPlace() {
    Rig r(inPlace());
    const std::vector<kombitest::Moment> drive = kombitest::drive(kombitest::demoRoute());
    for (size_t i = 0; i < drive.size(); ++i) {
        r.tick(drive[i].t, drive[i].inbox);
    }
    r.guide.step(nullptr, drive.back().t + 100);
    CHECK(r.out.batches[0] == 97 && r.out.cancels == 1 && !r.guide.showing());
    size_t blocks = 0;
    for (size_t i = 97; i < r.count(); ++i) {
        const Request& q = r.out.sent[i];
        CHECK(isAllowedMethod(q.method));
        CHECK(!(q.method == 0x0009 && modelType(q.body) == 33 && q.body[30] == 1));  // never a route start again
        if (q.method == 0x0003 && modelType(q.body) == 3) {
            CHECK(r.shape(i, i + 9) == kUpdateBlock && r.out.sent[i + 8].body[30] == 2);
            ++blocks;
        }
    }
    CHECK(blocks == 4);
}

void gapsAndRefusals() {
    {
        Rig r(inPlace());
        r.tick(0, Step(7, "TEST ST"), 400, "0.3", 5);
        r.tick(1, Step(8, "MAIN ST"), 400, "0.3", 5);
        CHECK(r.count() == 97 + 9);
        r.guide.step(nullptr, 2);                      // a gap: likely a reroute
        r.tick(3, Step(8, "MAIN ST"), 380, "0.2", 5);  // back on the same maneuver: its distance only
        CHECK(r.count() == 97 + 9 + 1);
        r.tick(4, Step(7, "OAK AVE"), 500, "0.3", 5);
        CHECK(r.count() == 97 + 9 + 1 + 9);
        r.guide.step(nullptr, 5);
        r.tick(6, Step(8, "ELM WAY"), 500, "0.3", 5);  // a new maneuver after a gap: the whole route start
        CHECK(r.count() == 97 + 9 + 1 + 9 + 97);
        r.tick(7, Step(7, "PINE RD"), 500, "0.3", 5);
        CHECK(r.count() == 97 + 9 + 1 + 9 + 97 + 9);
    }
    {
        Rig r(inPlace(GuideSettings::kInPlaceWithoutGuiding));
        r.tick(0, Step(7, "TEST ST"), 400, "0.3", 5);
        r.tick(1, Step(8, "MAIN ST"), 400, "0.3", 5);
        CHECK(r.count() == 97 + 8 && r.countOf(0x000B, 97) == 0);
    }
    {
        // The update in place refused: the whole graph at once, and until the next clear; then in place again.
        Rig r(inPlace());
        r.tick(0, Step(7, "TEST ST"), 400, "0.3", 5);
        r.out.script.push_back(kRefused);
        r.tick(1, Step(8, "MAIN ST"), 400, "0.3", 5);
        CHECK(r.count() == 97 + 97 && r.guide.showing() && r.guide.stopped() == Guide::kRunning);
        r.tick(2, Step(7, "OAK AVE"), 400, "0.3", 5);
        CHECK(r.count() == 97 * 3);
        r.guide.clear("test");
        r.tick(3, Step(7, "TEST ST"), 400, "0.3", 5);
        r.tick(4, Step(8, "MAIN ST"), 400, "0.3", 5);
        CHECK(r.count() == 97 * 4 + 9);
    }
    {
        // A refused route start: stopped, cleared, and nothing more until resumed.
        Rig r;
        r.out.script.push_back(kRefused);
        r.tick(0, Step(7, "TEST ST"), 800, "0.5", 5);
        CHECK(r.guide.stopped() == Guide::kStoppedRefused && !r.guide.showing() && r.out.cancels == 1);
        r.tick(1, Step(7, "TEST ST"), 700, "0.4", 5);
        CHECK(r.count() == 0);
        r.guide.resumeAfterStop();
        r.tick(2, Step(7, "TEST ST"), 700, "0.4", 5);
        CHECK(r.count() == 97);
        // A distance refused, then the link lost.
        r.out.script.push_back(kRefused);
        r.tick(3, Step(7, "TEST ST"), 600, "0.3", 5);
        CHECK(r.guide.stopped() == Guide::kStoppedRefused && r.out.cancels == 2);
        r.guide.resumeAfterStop();
        r.out.script.push_back(kLinkDown);
        r.tick(4, Step(7, "TEST ST"), 600, "0.3", 5);
        CHECK(r.guide.stopped() == Guide::kStoppedLink && r.out.cancels == 3 && !r.guide.sentAny());
    }
}

void distancesAndNames() {
    {
        // Up to 3 s for a distance, then "-", never the template's "600"; the road capped at 40 characters.
        Rig r;
        const NavInbox noDistance = kombitest::inbox(kombitest::state(Step(7, std::string(300, 'X'))));
        r.tick(0, noDistance);
        r.tick(2.9, noDistance);
        CHECK(r.count() == 0);
        r.tick(3.0, noDistance);
        CHECK(r.count() == 97 && r.lastDistance() == "-" && r.lastRoad() == std::string(40, 'X'));
    }
    {
        // A step without its distance keeps the one shown: nothing sent.
        Rig r;
        r.tick(0, Step(7, "TEST ST"), 800, "0.5", 5);
        r.tick(2, kombitest::inbox(kombitest::state(Step(7, "TEST ST"))));
        CHECK(r.count() == 97 && r.guide.showing());
    }
    {
        // Metric stays metric: the unit bytes read on the car, 3 for m and 0 for km.
        int unit = -1;
        Rig m;
        m.tick(0, Step(8, "RUE"), 250, "250", kUnitMetres);
        CHECK(m.lastDistance(&unit) == "250" && unit == kClusterMetres);
        Rig k;
        k.tick(0, Step(8, "RUE"), 1400, "1.4", kUnitKilometresTenths);
        CHECK(k.lastDistance(&unit) == "1.4" && unit == kClusterKilometres);
    }
    {
        // Updates at most every distanceEveryS; the red bar always hidden.
        const GuideSettings car;
        Rig r(car);
        r.tick(0, Step(7, "TEST ST"), 800, "0.5", 5);
        r.tick(0.3, Step(7, "TEST ST"), 700, "0.4", 5);
        CHECK(r.count() == 97);
        r.tick(0.5, Step(7, "TEST ST"), 700, "0.4", 5);
        CHECK(r.count() == 98 && r.lastDistance() == "0.4" && r.out.sent.back().body[22] == 255);
    }
    {
        // The roundabout exit on the road line.
        Rig r;
        r.tick(0, Step(34, "RUE A").cue("Au rond-point, prenez la 2e sortie").angle(90), 300, "0.2", 5);
        CHECK(r.lastRoad() == "Sortie 2 - RUE A");
        r.tick(1, Step(34, "").cue("prenez la 3e sortie").angle(90), 300, "0.2", 5);
        CHECK(r.lastRoad() == "Sortie 3");
    }
}

void graceAndYield() {
    {
        // Rerouting for less than 8 s keeps the arrow; a real end clears it.
        Rig r;
        NavInbox rerouting;
        kombitest::put(rerouting, kNavStatus, kombitest::status(3));
        r.tick(0, Step(7, "TEST ST"), 800, "0.5", 5);
        r.tick(1, rerouting);
        r.tick(5, rerouting);
        CHECK(r.guide.showing() && r.out.cancels == 0);
        r.tick(6, Step(7, "TEST ST"), 800, "0.5", 5);
        r.tick(7, rerouting);
        r.tick(15, rerouting);
        CHECK(!r.guide.showing() && r.out.cancels == 1);
        r.guide.clear("again");
        CHECK(r.out.cancels == 1);  // nothing sent since: no second cancel
    }
    {
        // The BMW navigation guides: no cancel, nothing at exit, then a fresh route start.
        Rig r;
        r.tick(0, Step(7, "TEST ST"), 800, "0.5", 5);
        r.guide.yieldToFactory();
        CHECK(!r.guide.showing() && r.count() == 97);
        r.guide.clear("exit");
        CHECK(r.out.cancels == 0);
        r.tick(3, Step(7, "TEST ST"), 700, "0.4", 5);
        CHECK(r.count() == 97 * 2);
    }
}

void approach() {
    const double mile = 1609.344;
    {
        GuideSettings s;
        s.approachMiles = 2.0;
        s.distanceEveryS = 0;
        Rig r(s);
        r.tick(0, Step(7, "X"), static_cast<int64_t>(2.1 * mile), "2.1", 5);
        CHECK(!r.guide.showing() && r.count() == 0);
        r.tick(1, Step(7, "X"), static_cast<int64_t>(1.9 * mile), "1.9", 5);
        CHECK(r.guide.showing());
        r.tick(2, Step(7, "X"), static_cast<int64_t>(2.1 * mile), "2.1", 5);  // the same maneuver, under 2.3: kept
        CHECK(r.guide.showing());
        r.tick(3, Step(8, "Y"), static_cast<int64_t>(2.1 * mile), "2.1", 5);  // a new one at 2.1: hidden
        CHECK(!r.guide.showing() && r.out.cancels == 1);
        for (size_t i = 0; i < r.count(); ++i) {
            if (r.out.sent[i].method == 0x000A && modelType(r.out.sent[i].body) == 29 &&
                modelIndex(r.out.sent[i].body) == 0) {
                CHECK(textIn(r.out.sent[i].body).size() <= 3);
            }
        }
    }
    {
        // An unknown distance shows anyway, with "-" after the wait.
        GuideSettings s;
        s.approachMiles = 2.0;
        Rig r(s);
        const NavInbox in = kombitest::inbox(kombitest::state(Step(7, "X")));
        r.tick(0, in);
        r.tick(3.5, in);
        CHECK(r.guide.showing() && r.count() == 97);
    }
}

std::vector<kombitest::Lane> junction(int k) {
    std::vector<kombitest::Lane> l;
    if (k == 0) {
        l.push_back(lane(4, 1));
        l.push_back(lane(1));
        l.push_back(lane(1));
        l.push_back(lane(1, -1, 5, -1));
    } else if (k == 1) {
        l.push_back(lane(4));
        l.push_back(lane(1));
        l.push_back(lane(1));
        l.push_back(lane(5, 1, 1, -1));
    } else {
        l.push_back(lane(4));
        l.push_back(lane(1));
        l.push_back(lane(5, 1));
    }
    return l;
}

void lanesWithGraphs() {
    {
        // From the start: 93 requests, then a position the cluster checks the row on; positions at most once a
        // second and only when the car moved, kept inside the section.
        Rig r;
        r.tick(0, Step(7, "TEST ST").with(junction(0)), 320, "0.2", 5);
        CHECK(r.count() == 93 + 1 && r.countOf(0x0010) == 2 && r.lastPosition() == 4132.0);  // 3952 + 180 at most
        r.tick(0.5, Step(7, "TEST ST").with(junction(0)), 150, "500", 6);
        CHECK(r.countOf(0x0009, 94) == 0 && r.countOf(0x000A, 94) == 1);
        r.tick(1.2, Step(7, "TEST ST").with(junction(0)), 150, "500", 6);
        CHECK(r.countOf(0x0009, 94) == 1 && r.lastPosition() == 3952.0 + 150);
        const size_t n = r.count();
        r.tick(2.5, Step(7, "TEST ST").with(junction(0)), 150, "500", 6);
        CHECK(r.count() == n);  // not moved
        r.tick(3.6, Step(7, "TEST ST").with(junction(0)), 10, "50", 6);
        CHECK(r.lastPosition() == 3962.0);
    }
    {
        // Lanes appear on the same maneuver: a new graph, but not within 3 s of the last; they go: the graph
        // without them, and no more positions.
        Rig r;
        r.tick(0, Step(8, "MAIN ST"), 600, "0.4", 5);
        r.tick(1, Step(8, "MAIN ST").with(junction(1)), 400, "0.2", 5);
        CHECK(r.count() == 98);
        r.tick(3.5, Step(8, "MAIN ST").with(junction(1)), 380, "0.2", 5);
        CHECK(r.count() == 98 + 93 + 1 && r.countOf(0x0010, 98) == 2);
        r.tick(5, Step(8, "MAIN ST").with(junction(1)), 100, "0.1", 5);
        CHECK(r.count() == 98 + 94 + 2 && r.out.sent.back().method == 0x0009);
        r.tick(7, Step(8, "MAIN ST"), 150, "500", 6);
        CHECK(r.count() == 98 + 96 + 97);
        r.tick(9, Step(8, "MAIN ST"), 50, "150", 6);
        CHECK(r.count() == 98 + 96 + 98 && r.out.sent.back().method == 0x000A);
    }
    {
        // A new maneuver with lanes builds its own; lanes the cluster can't draw send none; lanes off: no row.
        Rig r;
        r.tick(0, Step(7, "TEST ST").with(junction(0)), 320, "0.2", 5);
        r.tick(0.5, Step(7, "OAK AVE").with(junction(2)), 300, "0.2", 5);
        CHECK(r.count() == 2 * 94);
        r.tick(1, Step(5, "ELM WAY").with(lane(0, 1)).with(lane(2, 1)), 300, "0.2", 5);
        CHECK(r.count() == 2 * 94 + 97);
        GuideSettings off = fullGraphs();
        off.lanes = false;
        Rig without(off), plain;
        without.tick(0, Step(7, "TEST ST").with(junction(0)), 320, "0.2", 5);
        plain.tick(0, Step(7, "TEST ST"), 320, "0.2", 5);
        CHECK(without.count() == 97 && plain.count() == 97);
        for (size_t i = 0; i < 97; ++i) {
            CHECK(without.out.sent[i].body == plain.out.sent[i].body);
        }
    }
    {
        // A clear or a yield forgets the row: no positions afterwards.
        Rig r;
        r.tick(0, Step(7, "TEST ST").with(junction(0)), 320, "0.2", 5);
        r.guide.yieldToFactory();
        const size_t n = r.count();
        r.guide.step(nullptr, 5);
        CHECK(r.count() == n);
        r.tick(6, Step(7, "TEST ST").with(junction(0)), 200, "0.1", 5);
        CHECK(r.count() == n + 94);
        r.guide.clear("test");
        r.tick(20, kombitest::inbox(kombitest::state(Step(7, "OTHER"))));
        CHECK(r.count() == n + 94);
    }
}

void lanesInPlace() {
    Rig r(inPlace());
    r.tick(0, Step(8, "MAIN ST"), 600, "0.4", 5);
    r.tick(3.5, Step(8, "MAIN ST").with(junction(1)), 380, "0.2", 5);
    CHECK(r.count() == 97 + 10 + 1 && r.shape(97, 107) == "3/3 e/28 a/22 b/7 a/1d 10/1b a/f c/20 4/0 9/21 ");
    CHECK(readBE32(&r.out.sent[103].body[54]) == 4 && readBE32(&r.out.sent[103].body[58]) == 0x1B000000);
    CHECK(r.lastPosition() == 4132.0);
    r.tick(4, Step(7, "OAK AVE").with(junction(2)), 300, "0.2", 5);
    CHECK(r.count() == 108 + 11 && r.countOf(0x0010, 108) == 1);
    r.tick(8, Step(7, "OAK AVE"), 150, "500", 6);
    CHECK(r.count() == 119 + 9 && r.countOf(0x0010, 119) == 0 && readBE32(&r.out.sent[119 + 5].body[54]) == 0);
    r.tick(10, Step(7, "OAK AVE"), 50, "150", 6);
    CHECK(r.count() == 128 + 1 && r.out.sent.back().method == 0x000A);
}

}  // namespace

int main() {
    demoDriveInPlace();
    gapsAndRefusals();
    distancesAndNames();
    graceAndYield();
    approach();
    lanesWithGraphs();
    lanesInPlace();
    puts("PASS kombi guide: in place and whole graphs, distances, lanes and positions, grace, approach, refusals");
    return 0;
}
