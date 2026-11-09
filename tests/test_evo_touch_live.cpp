// Copyright (c) 2026 Jiraiya43
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Touch integration test on Linux (root, loopback only): real AF_PACKET tap with BPF, real UDP 30567 sockets.

// A fake "HMI" socket listens on 0.0.0.0:30567 like HmiMain3D; touch PDUs are sent to 127.0.0.1:30567.
//   1. HMI with SO_REUSEADDR: our connected socket takes the controller's datagrams while owned, gives them back after.
//   2. HMI without reuse flags: bind refused, copy-only fallback, HMI keeps receiving everything.
//   3. Observe mode never sends.
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <vector>

#include "evo_touch.hpp"
#define CHECK(x)                                                 \
    do {                                                         \
        if (!(x)) {                                              \
            fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); \
            exit(1);                                             \
        }                                                        \
    } while (0)

static std::vector<aa::TouchEventOut> got;
static pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;
static void onTouch(const aa::TouchEventOut& ev, void*) {
    pthread_mutex_lock(&mu);
    got.push_back(ev);
    pthread_mutex_unlock(&mu);
}
static size_t events() {
    pthread_mutex_lock(&mu);
    size_t n = got.size();
    pthread_mutex_unlock(&mu);
    return n;
}

static int hmiSocket(bool reuse) {
    int s = socket(AF_INET, SOCK_DGRAM, 0);
    CHECK(s >= 0);
    int one = 1;
    if (reuse) {
        CHECK(setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) == 0);
    }
    sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_port = htons(30567);
    a.sin_addr.s_addr = htonl(INADDR_ANY);
    CHECK(bind(s, (sockaddr*)&a, sizeof(a)) == 0);
    return s;
}
static int controller() {  // the touch controller: fixed source port like 30501
    int s = socket(AF_INET, SOCK_DGRAM, 0);
    CHECK(s >= 0);
    sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_port = htons(30501);
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    CHECK(bind(s, (sockaddr*)&a, sizeof(a)) == 0);
    return s;
}
static void sendTouch(int ctl, int x, int y, int state, unsigned char seq) {
    unsigned char f[112];
    aa::TouchContact c;
    c.id = 0;
    c.state = state;
    c.x = x;
    c.y = y;
    c.size = 8;
    aa::buildTouchFrame(f, &c, 1, 0, seq);
    sockaddr_in to;
    memset(&to, 0, sizeof(to));
    to.sin_family = AF_INET;
    to.sin_port = htons(30567);
    to.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    CHECK(sendto(ctl, f, sizeof(f), 0, (sockaddr*)&to, sizeof(to)) == (ssize_t)sizeof(f));
    usleep(60000);
}
static int drain(int s) {
    int n = 0;
    unsigned char b[256];
    while (recv(s, b, sizeof(b), MSG_DONTWAIT) > 0) {
        ++n;
    }
    return n;
}

int main() {
    if (geteuid() != 0) {
        puts("SKIP: root required (AF_PACKET)");
        return 77;
    }
    // 1. HMI shares the port: touches go to AA only while owned.
    int hmi = hmiSocket(true), ctl = controller();
    evo::EvoTouch t;
    CHECK(t.start(evo::INPUT_FORWARD, onTouch, 0));
    usleep(100000);
    sendTouch(ctl, 50, 60, 1, 0);
    sendTouch(ctl, 50, 60, 3, 12);  // not owned yet: decoded, not sent
    CHECK(events() == 0 && drain(hmi) == 2);
    t.setOwned(true);
    usleep(250000);  // peer learned: connected socket opens
    sendTouch(ctl, 100, 200, 1, 24);
    sendTouch(ctl, 120, 210, 2, 36);
    sendTouch(ctl, 120, 210, 3, 48);
    usleep(100000);
    CHECK(events() == 3 && got[0].action == 0 && got[0].p[0].x == 100 && got[1].action == 2 && got[2].action == 1);
    CHECK(drain(hmi) == 0);  // HMI got none of them
    t.setOwned(false);
    usleep(250000);  // AA hidden: back to the HMI
    sendTouch(ctl, 10, 10, 1, 60);
    sendTouch(ctl, 10, 10, 3, 72);
    CHECK(events() == 3 && drain(hmi) == 2);
    t.setOwned(true);
    usleep(250000);
    sendTouch(ctl, 5, 5, 1, 84);
    CHECK(events() == 4 && drain(hmi) == 0);
    t.stop();  // stop always returns the port
    sendTouch(ctl, 5, 5, 3, 96);
    CHECK(drain(hmi) == 1);
    t.summary();
    close(hmi);
    // 2. HMI did not set SO_REUSEADDR: bind refused, copy only, HMI unaffected.
    hmi = hmiSocket(false);
    evo::EvoTouch c;
    got.clear();
    CHECK(c.start(evo::INPUT_FORWARD, onTouch, 0));
    usleep(100000);
    c.setOwned(true);
    sendTouch(ctl, 300, 400, 1, 108);
    usleep(250000);
    sendTouch(ctl, 300, 400, 3, 120);
    usleep(100000);
    CHECK(events() == 2 && drain(hmi) == 2);
    c.stop();
    c.summary();
    // 3. Observe mode never sends.
    evo::EvoTouch o;
    got.clear();
    CHECK(o.start(evo::INPUT_OBSERVE, onTouch, 0));
    usleep(100000);
    o.setOwned(true);
    sendTouch(ctl, 1, 2, 1, 132);
    sendTouch(ctl, 1, 2, 3, 144);
    CHECK(events() == 0 && drain(hmi) == 2);
    o.stop();
    o.summary();
    close(hmi);
    close(ctl);
    puts(
        "PASS tactile reel (loopback, root) : copie AF_PACKET/BPF, socket connectee prioritaire pendant AA, "
        "restitution a l'iDrive, repli copie seule si port non partageable, observation sans envoi");
    return 0;
}
