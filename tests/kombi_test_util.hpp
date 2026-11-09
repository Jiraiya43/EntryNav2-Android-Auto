// Copyright (c) 2026 Jiraiya43
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Test helpers for the cluster guidance: the navigation messages a phone sends, written with core/pb.hpp.
#ifndef ENTRY_AAD_TESTS_KOMBI_TEST_UTIL_HPP
#define ENTRY_AAD_TESTS_KOMBI_TEST_UTIL_HPP

#include <stdint.h>

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "kombi_phone.hpp"
#include "pb.hpp"

#define CHECK(x)                                                         \
    do {                                                                 \
        if (!(x)) {                                                      \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); \
            exit(1);                                                     \
        }                                                                \
    } while (0)

namespace kombitest {

using Bytes = std::vector<uint8_t>;

// One direction of a lane; highlight -1 leaves the field out, as phones do for "no".
struct Arrow {
    int shape;
    int highlight;
};
using Lane = std::vector<Arrow>;

inline Lane lane(int shape, int highlight = -1) {
    const Arrow a = {shape, highlight};
    return Lane(1, a);
}

inline Lane lane(int shape1, int highlight1, int shape2, int highlight2) {
    Lane l = lane(shape1, highlight1);
    l.push_back(lane(shape2, highlight2)[0]);
    return l;
}

// One NavigationStep; the setters return it, so a step is written in one expression.
class Step {
public:
    Step(int type, const std::string& road) : type_(type), road_(road), cue_("cue text"), exit_(-1), angle_(-1) {}
    Step& cue(const std::string& text) {
        cue_ = text;
        return *this;
    }
    Step& exit(int number) {
        exit_ = number;
        return *this;
    }
    Step& angle(int degrees) {
        angle_ = degrees;
        return *this;
    }
    Step& with(const Lane& l) {
        lanes_.push_back(l);
        return *this;
    }
    Step& with(const std::vector<Lane>& lanes) {
        lanes_.insert(lanes_.end(), lanes.begin(), lanes.end());
        return *this;
    }

    Bytes encode() const {
        Bytes maneuver, road, step;
        aa::pb::appendUint(maneuver, 1, static_cast<uint64_t>(static_cast<int64_t>(type_)));
        if (exit_ >= 0) {
            aa::pb::appendUint(maneuver, 2, static_cast<uint64_t>(exit_));
        }
        if (angle_ >= 0) {
            aa::pb::appendUint(maneuver, 3, static_cast<uint64_t>(angle_));
        }
        aa::pb::appendMessage(step, 1, maneuver);
        aa::pb::appendString(road, 1, road_);
        aa::pb::appendMessage(step, 2, road);
        for (size_t i = 0; i < lanes_.size(); ++i) {
            Bytes encoded;
            for (size_t k = 0; k < lanes_[i].size(); ++k) {
                Bytes direction;
                aa::pb::appendUint(direction, 1, static_cast<uint64_t>(lanes_[i][k].shape));
                if (lanes_[i][k].highlight >= 0) {
                    aa::pb::appendUint(direction, 2, static_cast<uint64_t>(lanes_[i][k].highlight));
                }
                aa::pb::appendMessage(encoded, 1, direction);
            }
            aa::pb::appendMessage(step, 3, encoded);
        }
        if (!cue_.empty()) {
            Bytes cue;
            aa::pb::appendString(cue, 1, cue_);
            aa::pb::appendMessage(step, 4, cue);
        }
        return step;
    }

private:
    int type_;
    std::string road_, cue_;
    int exit_, angle_;
    std::vector<Lane> lanes_;
};

inline Bytes status(uint64_t value) {
    Bytes o;
    aa::pb::appendUint(o, 1, value);
    return o;
}

// NavigationState: the step, the ones after it, a destination.
inline Bytes state(const Step& first, const std::vector<Step>& next = std::vector<Step>()) {
    Bytes o, destination;
    aa::pb::appendMessage(o, 1, first.encode());
    for (size_t i = 0; i < next.size(); ++i) {
        aa::pb::appendMessage(o, 1, next[i].encode());
    }
    aa::pb::appendString(destination, 1, "1 DEMO WAY");
    aa::pb::appendMessage(o, 2, destination);
    return o;
}

// NavigationCurrentPosition, with the metres to the maneuver unless metres < 0 (a parked phone sends none).
inline Bytes position(int64_t metres, const std::string& text, int unit) {
    Bytes distance, step, toDestination, destination, o;
    if (metres >= 0) {
        aa::pb::appendUint(distance, 1, static_cast<uint64_t>(metres));
    }
    aa::pb::appendString(distance, 2, text);
    aa::pb::appendUint(distance, 3, static_cast<uint64_t>(unit));
    aa::pb::appendMessage(step, 1, distance);
    aa::pb::appendUint(step, 2, 30);
    aa::pb::appendMessage(o, 1, step);
    aa::pb::appendUint(toDestination, 1, 5000);
    aa::pb::appendString(toDestination, 2, "3.1");
    aa::pb::appendUint(toDestination, 3, 5);
    aa::pb::appendMessage(destination, 1, toDestination);
    aa::pb::appendString(destination, 2, "12:17 AM");
    aa::pb::appendMessage(o, 2, destination);
    return o;
}

inline void put(evo::kombi::NavInbox& in, uint16_t id, const Bytes& body) {
    in.keep(id, body.empty() ? reinterpret_cast<const uint8_t*>("") : &body[0], body.size());
}

// What the service holds at one moment: a status, a state and, when given, a position.
inline evo::kombi::NavInbox inbox(const Bytes& stateBody, const Bytes& positionBody = Bytes(), uint64_t s = 1) {
    evo::kombi::NavInbox in;
    put(in, evo::kombi::kNavStatus, status(s));
    if (!stateBody.empty()) {
        put(in, evo::kombi::kNavState, stateBody);
    }
    if (!positionBody.empty()) {
        put(in, evo::kombi::kNavPosition, positionBody);
    }
    return in;
}

// The text a phone shows in miles and feet, as Google Maps does in the US.
inline void imperialText(double metres, std::string& text, int& unit) {
    char b[32];
    const double miles = metres / 1609.344;
    if (miles >= 0.095) {
        snprintf(b, sizeof(b), "%.1f", miles);
        unit = evo::kombi::kUnitMilesTenths;
    } else {
        snprintf(b, sizeof(b), "%d", static_cast<int>(metres * 3.28084) / 50 * 50);
        unit = evo::kombi::kUnitFeet;
    }
    text = b;
}

// One leg of a test route; angle -1: none.
struct Leg {
    int type;
    const char* road;
    double metres;
    int angle;
};

// What the service holds at one moment of a drive.
struct Moment {
    double t;
    evo::kombi::NavInbox inbox;
};

// The route at `speed` m/s, a moment a second, distances in miles and feet. The first moment has the guidance on and
// no step yet; the last turns it off.
inline std::vector<Moment> drive(const std::vector<Leg>& legs, double speed = 15.0) {
    std::vector<Moment> out;
    Moment m;
    m.t = 0;
    put(m.inbox, evo::kombi::kNavStatus, status(1));
    out.push_back(m);
    for (size_t k = 0; k < legs.size(); ++k) {
        Step step(legs[k].type, legs[k].road);
        if (legs[k].angle >= 0) {
            step.angle(legs[k].angle).exit(1);
        }
        std::vector<Step> next;
        for (size_t j = k + 1; j < legs.size() && j < k + 3; ++j) {
            next.push_back(Step(legs[j].type, legs[j].road));
        }
        const Bytes s = state(step, next);
        for (double left = legs[k].metres; left > 0; left -= speed) {
            std::string text;
            int unit;
            imperialText(left, text, unit);
            m.t += 1;
            m.inbox = inbox(s, position(static_cast<int64_t>(left), text, unit));
            out.push_back(m);
        }
    }
    m.t += 1;
    m.inbox = evo::kombi::NavInbox();
    put(m.inbox, evo::kombi::kNavStatus, status(2));
    out.push_back(m);
    return out;
}

// Left, right, a roundabout at 90 degrees, a slight left, the destination on the right.
inline std::vector<Leg> demoRoute() {
    const Leg legs[] = {{7, "TEST ST", 800, -1},
                        {8, "MAIN ST", 480, -1},
                        {35, "CIRCLE RD", 320, 90},
                        {5, "OAK AVE", 400, -1},
                        {42, "DESTINATION", 160, -1}};
    return std::vector<Leg>(legs, legs + 5);
}

// The ASCII text at a model's first byte-order mark, and the byte after its terminator (-1: none).
inline std::string textIn(const Bytes& b, int* after = nullptr) {
    for (size_t mark = 18; mark + 1 < b.size(); ++mark) {
        if (b[mark] == 0xFE && b[mark + 1] == 0xFF) {
            const size_t end = mark + b[mark - 1];
            std::string s;
            for (size_t k = mark + 2; k + 2 < end && k + 1 < b.size(); k += 2) {
                s += static_cast<char>(b[k + 1]);
            }
            if (after) {
                *after = end < b.size() ? b[end] : -1;
            }
            return s;
        }
    }
    return "<none>";
}

// A model's type and index (body[8], body[4:8] & 0xFFFFFF).
inline int modelType(const Bytes& b) {
    return b.size() > 8 ? b[8] : 0;
}

inline uint32_t modelIndex(const Bytes& b) {
    return b.size() >= 8 ? (static_cast<uint32_t>(b[5]) << 16 | static_cast<uint32_t>(b[6]) << 8 | b[7]) : 0;
}

}  // namespace kombitest

#endif
