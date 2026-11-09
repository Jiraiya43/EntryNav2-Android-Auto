// Copyright (c) 2026 Jiraiya43
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Reads the next maneuver out of Android Auto's navigation messages, with core/pb.hpp.
#include "kombi_phone.hpp"

#include <cstdio>

#include "pb.hpp"

namespace evo {
namespace kombi {

namespace {

// Valid UTF-8 without control characters (a tab is allowed): how a string is told from a message.
bool looksLikeText(const uint8_t* p, size_t n) {
    if (n == 0) {
        return false;
    }
    for (size_t i = 0; i < n;) {
        const uint8_t lead = p[i];
        if ((lead < 0x20 && lead != '\t') || lead == 0x7F) {
            return false;
        }
        size_t width = 1;
        if (lead >= 0x80) {
            width = (lead & 0xE0) == 0xC0 ? 2 : (lead & 0xF0) == 0xE0 ? 3 : (lead & 0xF8) == 0xF0 ? 4 : 0;
            if (width == 0 || width > n - i) {
                return false;
            }
            for (size_t k = 1; k < width; ++k) {
                if ((p[i + k] & 0xC0) != 0x80) {
                    return false;
                }
            }
        }
        i += width;
    }
    return true;
}

// A message, or the absence of one, read field by field from the first occurrence of each.
class Fields {
public:
    Fields() : data_(nullptr), size_(0), present_(false) {}
    Fields(const uint8_t* data, size_t size) : data_(data), size_(size), present_(true) {}
    static Fields of(const std::vector<uint8_t>& body) {
        return body.empty() ? Fields(nullptr, 0) : Fields(&body[0], body.size());
    }

    bool present() const { return present_; }

    Fields message(uint32_t number) const {
        aa::pb::Field f;
        if (!first(number, f) || f.type != aa::pb::kBytes || looksLikeText(f.data, f.size)) {
            return Fields();
        }
        return Fields(f.data, f.size);
    }

    std::string text(uint32_t number) const {
        aa::pb::Field f;
        if (!first(number, f) || f.type != aa::pb::kBytes || !looksLikeText(f.data, f.size)) {
            return std::string();
        }
        return std::string(reinterpret_cast<const char*>(f.data), f.size);
    }

    // Leaves `value` alone when the field is absent.
    bool varint(uint32_t number, int64_t& value) const {
        aa::pb::Field f;
        if (!first(number, f) || f.type != aa::pb::kVarint) {
            return false;
        }
        value = static_cast<int64_t>(f.value);
        return true;
    }

    // Every length-delimited occurrence of the field, up to the end of the message or a malformed field.
    std::vector<Fields> every(uint32_t number) const {
        std::vector<Fields> found;
        aa::pb::FieldReader reader(data_, size_);
        aa::pb::Field f;
        while (present_ && reader.next(f)) {
            if (f.number == number && f.type == aa::pb::kBytes) {
                found.push_back(Fields(f.data, f.size));
            }
        }
        return found;
    }

private:
    bool first(uint32_t number, aa::pb::Field& out) const {
        if (!present_) {
            return false;
        }
        aa::pb::FieldReader reader(data_, size_);
        while (reader.next(out)) {
            if (out.number == number) {
                return true;
            }
        }
        return false;
    }

    const uint8_t* data_;
    size_t size_;
    bool present_;
};

// NavigationState: its first step is the next maneuver. False when that step is missing.
bool readState(const Fields& state, Maneuver& m) {
    const Fields step = state.message(1);
    if (!step.present()) {
        return false;
    }
    const Fields maneuver = step.message(1);
    int64_t value;
    if (maneuver.varint(1, value)) {
        m.type = static_cast<int>(value);
    }
    if (maneuver.varint(2, value)) {
        m.exitNumber = static_cast<int>(value);
    }
    if (maneuver.varint(3, value)) {
        m.hasExitAngle = true;
        m.exitAngle = value;
    }
    m.road = step.message(2).text(1);
    m.cue = step.message(4).text(1);
    const std::vector<Fields> lanes = step.every(3);
    for (size_t i = 0; i < lanes.size(); ++i) {
        const std::vector<Fields> directions = lanes[i].every(1);
        PhoneLane lane;
        for (size_t k = 0; k < directions.size(); ++k) {
            int64_t shape = 0, highlighted = 0;
            directions[k].varint(1, shape);
            directions[k].varint(2, highlighted);
            lane.push_back(LaneShape(static_cast<int>(shape), highlighted != 0));
        }
        m.lanes.push_back(lane);
    }
    return true;
}

// NextTurnEvent {1 road, 2 side: 1 left, 2 right, 3 event, 5 exit number, 6 exit angle}. Its event becomes a maneuver
// type by side; without a side, only events that have no side give one.
void readTurn(const Fields& turn, Maneuver& m) {
    struct Turn {
        int event, left, right;
    };
    static const Turn kTurns[] = {{1, 1, 1},    {2, 2, 2},    {3, 5, 6},    {4, 7, 8},    {5, 9, 10},   {6, 11, 12},
                                  {7, 15, 16},  {8, 23, 24},  {9, 25, 26},  {10, 27, 28}, {11, 34, 32}, {12, 34, 32},
                                  {13, 34, 32}, {14, 36, 36}, {16, 37, 37}, {17, 38, 38}, {19, 39, 39}};
    int64_t side = 0, event = 0, angle = 0;
    const bool sided = turn.varint(2, side);
    int left = 0, right = 0;
    if (turn.varint(3, event)) {
        for (size_t i = 0; i < sizeof(kTurns) / sizeof(kTurns[0]); ++i) {
            if (kTurns[i].event == event) {
                left = kTurns[i].left;
                right = kTurns[i].right;
            }
        }
    }
    if (sided && side == 1) {
        m.type = left;
    } else if (sided && side == 2) {
        m.type = right;
    } else {
        m.type = left == right ? left : 0;
    }
    m.road = turn.text(1);
    if (turn.varint(6, angle)) {
        m.hasExitAngle = true;
        m.exitAngle = angle;
    }
}

// NavigationCurrentPosition: the distance to the next maneuver.
void readPosition(const Fields& position, Maneuver& m) {
    const Fields distance = position.message(1).message(1);
    int64_t value;
    if (distance.varint(1, value)) {
        m.hasMetres = true;
        m.metres = value;
    }
    m.distanceText = distance.text(2);
    if (distance.varint(3, value)) {
        m.distanceUnit = static_cast<int>(value);
    }
}

// Thousandths to a whole number, half to even (2500 -> 2, 3500 -> 4), in integers: the service doesn't link libm.
int64_t wholeFromThousandths(int64_t thousandths) {
    const bool negative = thousandths < 0;
    const uint64_t magnitude =
        negative ? static_cast<uint64_t>(-(thousandths + 1)) + 1 : static_cast<uint64_t>(thousandths);
    uint64_t whole = magnitude / 1000;
    const uint64_t rest = magnitude % 1000;
    if (rest > 500 || (rest == 500 && whole % 2 == 1)) {
        ++whole;
    }
    return negative ? -static_cast<int64_t>(whole) : static_cast<int64_t>(whole);
}

// NextTurnDistanceEvent {1 metres, 2 seconds, 3 display distance in thousandths, 4 display unit}. The display text has
// one decimal for the units shown with tenths, none otherwise.
void readTurnDistance(const Fields& event, Maneuver& m) {
    int64_t value;
    if (event.varint(1, value)) {
        m.hasMetres = true;
        m.metres = value;
    }
    if (event.varint(4, value)) {
        m.distanceUnit = static_cast<int>(value);
    }
    if (event.varint(3, value)) {
        char text[32];
        if (m.distanceUnit == kUnitKilometresTenths || m.distanceUnit == kUnitMilesTenths) {
            snprintf(text, sizeof(text), "%.1f", static_cast<double>(value) / 1000.0);
        } else {
            snprintf(text, sizeof(text), "%lld", static_cast<long long>(wholeFromThousandths(value)));
        }
        m.distanceText = text;
    }
}

}  // namespace

Maneuver::Maneuver()
    : type(0),
      exitNumber(-1),
      hasExitAngle(false),
      exitAngle(0),
      distanceUnit(kUnitUnknown),
      hasMetres(false),
      metres(0) {}

bool Maneuver::sameTurn(const Maneuver& other) const {
    return type == other.type && road == other.road && hasExitAngle == other.hasExitAngle &&
           (!hasExitAngle || exitAngle == other.exitAngle);
}

bool Maneuver::sameDistance(const Maneuver& other) const {
    return distanceText == other.distanceText && distanceUnit == other.distanceUnit;
}

bool NavInbox::keep(uint16_t id, const uint8_t* body, size_t size) {
    std::vector<uint8_t>* slot;
    switch (id) {
        case kNavStatus:
            slot = &status_;
            break;
        case kNavTurn:
            slot = &turn_;
            break;
        case kNavTurnDistance:
            slot = &turnDistance_;
            break;
        case kNavState:
            slot = &state_;
            break;
        case kNavPosition:
            slot = &position_;
            break;
        default:
            return false;
    }
    slot->assign(body, body + size);
    return true;
}

void NavInbox::clear() {
    *this = NavInbox();
}

bool NavInbox::guiding() const {
    int64_t status = 0;
    return Fields::of(status_).varint(1, status) && status == 1;
}

bool NavInbox::nextManeuver(Maneuver& out) const {
    out = Maneuver();
    if (!state_.empty()) {
        if (!readState(Fields::of(state_), out)) {
            return false;
        }
    } else if (!turn_.empty()) {
        readTurn(Fields::of(turn_), out);
    } else {
        return false;
    }
    if (!position_.empty()) {
        readPosition(Fields::of(position_), out);
    } else if (!turnDistance_.empty()) {
        readTurnDistance(Fields::of(turnDistance_), out);
    }
    return true;
}

}  // namespace kombi
}  // namespace evo
