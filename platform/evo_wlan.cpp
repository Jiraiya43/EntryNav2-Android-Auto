// Copyright (c) 2026 Jiraiya43
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Switches the car's hotspot on like CarPlay does (WLAN 0xA0.01 SelectWLANState3 = 1), only when it is off.
#include "evo_wlan.hpp"

#include <poll.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include "evo_log.hpp"
#include "vmost_client.hpp"

namespace evo {

size_t buildSelectWlanState3(uint8_t out[3], uint16_t senderHandle, bool on) {
    out[0] = static_cast<uint8_t>(senderHandle >> 8);
    out[1] = static_cast<uint8_t>(senderHandle & 0xff);
    out[2] = on ? 1 : 0;
    return 3;
}

namespace {

const uint16_t kSenderHandle = 0x5741;  // 'WA': ours, any value is accepted (CarPlay uses its own counter)

struct Session {
    VmostClient vmost;
    WlanRequestResult* r;
    bool sent, gotState, final;
    explicit Session(WlanRequestResult* result) : r(result), sent(false), gotState(false), final(false) {}
};

uint32_t nowMs() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<uint32_t>(ts.tv_sec * 1000u + ts.tv_nsec / 1000000u);
}

void onMessage(void* msg, void* user) {
    Session* s = static_cast<Session*>(user);
    VmostMsgView v;
    if (!s || !parseVmostMsg(msg, v) || v.fblock != kWlanFblock || v.inst != kWlanApInstance) {
        return;
    }
    ++s->r->messages;
    char hex[3 * 16 + 1];
    size_t o = 0;
    for (size_t i = 0; i < v.len && i < 16; ++i) {
        o += snprintf(hex + o, sizeof(hex) - o, "%02x ", v.data[i]);
    }
    hex[o] = 0;
    ELOG("Hotspot", "wland reply WLAN.01 function 0x%03x op 0x%x: %s", v.fkt, (unsigned)v.op, hex);
    if (v.fkt == kWlanState3 && v.op == kMostOpStatus && v.len >= 1) {
        if (s->sent) {
            s->r->stateAfter = v.data[0];
        } else {
            s->r->stateBefore = v.data[0];
        }
        s->gotState = true;
    } else if (v.fkt == kSelectWlanState3) {
        s->r->replyOp = static_cast<int>(v.op);
        if (v.op == 0xD || v.op == 0x9 || v.op == 0xF) {
            s->final = true;  // ResultAck / ErrorAck / Error
        }
    }
}

// Process vmostd messages until done() or the deadline.
void pump(Session& s, int fd, unsigned ms, bool (*done)(const Session&)) {
    const uint32_t end = nowMs() + ms;
    while (!done(s) && static_cast<int32_t>(end - nowMs()) > 0) {
        struct pollfd p;
        p.fd = fd;
        p.events = POLLIN;
        p.revents = 0;
        int r = poll(&p, 1, 50);
        if (r < 0 && errno != EINTR) {
            break;
        }
        if (r > 0 && (p.revents & (POLLERR | POLLHUP | POLLNVAL))) {
            break;
        }
        if (r > 0 && (p.revents & POLLIN) && s.vmost.processMessages() != 0) {
            break;
        }
    }
}
bool haveState(const Session& s) {
    return s.gotState;
}
bool haveFinal(const Session& s) {
    return s.final;
}

}  // namespace

bool requestWlanAccessPoint(unsigned waitMs, WlanRequestResult* result) {
    WlanRequestResult local;
    WlanRequestResult& r = result ? *result : local;
    r.connected = false;
    r.stateBefore = -1;
    r.sent = false;
    r.stateAfter = -1;
    r.replyOp = -1;
    r.messages = 0;
    Session s(&r);
    VmostClient& v = s.vmost;
    switch (v.load()) {
        case VmostClient::kLoaded:
            break;
        case VmostClient::kNoDaemon:
            ELOG("Hotspot", "vmostd socket missing: hotspot not requested");
            return false;
        case VmostClient::kNoLibrary:
            ELOG("Hotspot", "libvmostclient.so.1: %s", v.error().c_str());
            return false;
        case VmostClient::kNoSymbols:
            ELOG("Hotspot", "vMOST functions missing: hotspot not requested");
            return false;
    }
    if (v.connect(onMessage, &s, 0) != 0) {
        ELOG("Hotspot", "vmostd refused the connection");
        v.unload();
        return false;
    }
    int rc = v.shadowRegister(kWlanFblock, kWlanApInstance);
    int fd = -1;
    if (rc != 0 || v.fileDescriptorGet(&fd) != 0 || fd < 0) {
        ELOG("Hotspot", "WLAN.01 registration refused (%d): hotspot not requested", rc);
        if (rc == 0) {
            v.shadowDeregister(kWlanFblock, kWlanApInstance);
        }
        v.disconnect();
        v.unload();
        return false;
    }
    r.connected = true;
    static const uint8_t dummy[4] = {0, 0, 0, 0};
    VmostMsgOut m;
    buildVmostGet(m, kWlanFblock, kWlanApInstance, kWlanState3, dummy);  // read-only: current hotspot state
    rc = v.send(&m);
    ELOG("Hotspot", "read WLANState3 (A0.01.C10 Get): %s (%d)", rc == 0 ? "sent" : "refused", rc);
    if (rc == 0) {
        pump(s, fd, 1000, haveState);
    }
    bool ok = false;
    if (r.stateBefore == 1) {
        ELOG("Hotspot", "wland already reports the hotspot on (state 1): nothing sent");
        ok = true;
    } else {
        uint8_t payload[3];
        buildSelectWlanState3(payload, kSenderHandle, true);
        memset(&m, 0, sizeof(m));
        m.addr = 0xFFFF;
        m.fblock = kWlanFblock;
        m.inst = kWlanApInstance;
        m.fkt = kSelectWlanState3;
        m.op = kMostOpStartResultAck;
        m.len = 3;
        m.data = payload;
        m.channel = 0;
        rc = v.send(&m);
        r.sent = rc == 0;
        s.sent = r.sent;
        s.gotState = false;
        ELOG("Hotspot",
             "state before %d: switch-on requested as the CarPlay switch does (A0.01.C11 StartResultAck "
             "%02x %02x 01): %s (%d)",
             r.stateBefore, payload[0], payload[1], r.sent ? "sent" : "refused", rc);
        if (r.sent) {
            pump(s, fd, waitMs, haveFinal);
        }
        if (r.sent && !s.gotState) {
            pump(s, fd, 500, haveState);
        }
        ok = r.sent;
        ELOG("Hotspot", "request done: reply op %d, state after %d, %u wland message(s)", r.replyOp, r.stateAfter,
             r.messages);
    }
    v.shadowDeregister(kWlanFblock, kWlanApInstance);
    v.disconnect();
    v.unload();
    return ok;
}

}  // namespace evo
