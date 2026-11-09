// Copyright (c) 2026 Jiraiya43
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Tests platform/kombi_client.cpp against a fake KOMBI on 127.0.0.1: sessions, reconnection, cookies, cancels.
#include <sys/socket.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "kombi_client.hpp"
#include "kombi_fake_kombi.hpp"
#include "kombi_test_util.hpp"

namespace {

using namespace evo::kombi;
using kombitest::FakeKombi;
using kombitest::Seen;

std::vector<Request> someRequests(size_t n) {
    RouteGraph g;
    std::vector<Request> all;
    CHECK(g.routeStart(ClusterManeuver(kLeft, "TEST ST", "0.5", kClusterMiles), all));
    all.resize(n);
    return all;
}

void sharedConnection() {
    FakeKombi k;
    ClusterLink link("127.0.0.1", k.port());
    const std::vector<Request> requests = someRequests(5);
    CHECK(link.send(requests) == kSent && link.send(requests) == kSent);
    const std::vector<Seen> s = k.seen();
    CHECK(s.size() == 10);
    for (size_t i = 0; i < s.size(); ++i) {
        CHECK(s[i].connection == 1 && s[i].client == kOurClient && s[i].session == i + 1);  // one connection, 1, 2...
        CHECK(s[i].method == requests[i % 5].method && s[i].body == requests[i % 5].body);
    }
}

void reconnectOnce() {
    FakeKombi k;
    ClusterLink link("127.0.0.1", k.port());
    const std::vector<Request> requests = someRequests(3);
    k.hangUpAt(2);  // no answer to the second request: one new connection, the request again
    CHECK(link.send(requests) == kSent);
    std::vector<Seen> s = k.seen();
    CHECK(s.size() == 4 && s[1].connection == 1 && s[2].connection == 2 && s[2].body == s[1].body);
    CHECK(s[2].session == 3 && s[3].session == 4);
    k.hangUpAt(5);
    k.hangUpAt(6);  // twice in a row: the link is down
    std::string why;
    CHECK(link.send(requests, &why) == kLinkDown && !why.empty());
    // Closed while idle: the next send reconnects.
    CHECK(link.send(requests) == kSent);
    k.hangUpAll();
    usleep(100000);
    CHECK(link.send(requests) == kSent);
}

void refusedNotRetried() {
    FakeKombi k;
    ClusterLink link("127.0.0.1", k.port());
    k.refuse(2);
    std::string why;
    CHECK(link.send(someRequests(3), &why) == kRefused && why.find("return code 0x01") != std::string::npos);
    CHECK(k.count() == 2);
    // A method outside the list is refused before it leaves.
    std::vector<Request> create(1);
    create[0].method = 0x0001;
    CHECK(link.send(create, &why) == kRefused && k.count() == 2);
}

void cookies() {
    FakeKombi k;
    k.sendCookies(20);
    ClusterLink link("127.0.0.1", k.port());
    CHECK(link.send(someRequests(2)) == kSent);
    usleep(400000);  // cookies pile up while idle
    CHECK(link.send(someRequests(2)) == kSent);
    CHECK(k.count() == 4 && k.seen()[3].connection == 1);
}

void cancels() {
    const RouteGraph g;
    std::vector<Request> full, fallback;
    g.cancel(full);
    g.cancelGuidance(fallback);
    {
        FakeKombi k;
        ClusterLink link("127.0.0.1", k.port());
        CHECK(link.send(someRequests(1)) == kSent);
        CHECK(link.cancel(g));
        const std::vector<Seen> s = k.seen();
        CHECK(s.size() == 1 + full.size() && s[1].connection == 2 && s[1].session == 400);
        for (size_t i = 0; i < full.size(); ++i) {
            CHECK(s[1 + i].body == full[i].body && s[1 + i].session == 400 + i);
        }
    }
    {
        // The cancel refused: its guidance-state-0 requests alone, on another connection.
        FakeKombi k;
        ClusterLink link("127.0.0.1", k.port());
        k.refuse(3);
        CHECK(link.cancel(g));
        const std::vector<Seen> s = k.seen();
        CHECK(s.size() == 3 + fallback.size() && s[3].connection == 2 && s[3].session == 500);
        CHECK(s[3].body == fallback[0].body && s[4].body == fallback[1].body);
        k.refuse(6);
        k.refuse(7);
        CHECK(!link.cancel(g, 0.0, 0.2));
    }
    {
        // Through a blip: again until it goes.
        FakeKombi k;
        k.refuseUntil(kombitest::secondsNow() + 0.3);
        ClusterLink link("127.0.0.1", k.port());
        CHECK(link.cancel(g, 3.0, 0.2) && k.count() > full.size());
    }
    {
        // Nothing listening: the link is down, not a refusal.
        const int closed = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in a;
        memset(&a, 0, sizeof(a));
        a.sin_family = AF_INET;
        a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        socklen_t size = sizeof(a);
        CHECK(bind(closed, reinterpret_cast<struct sockaddr*>(&a), sizeof(a)) == 0 &&
              getsockname(closed, reinterpret_cast<struct sockaddr*>(&a), &size) == 0);
        ClusterLink link("127.0.0.1", ntohs(a.sin_port), 500);
        CHECK(link.send(someRequests(1)) == kLinkDown && !link.cancel(g));
        close(closed);
    }
}

void emergency() {
    const RouteGraph g;
    std::vector<Request> full;
    g.cancel(full);
    FakeKombi k;
    ClusterLink link("127.0.0.1", k.port());
    link.armEmergencyCancel(g);
    ClusterLink::emergencyCancel();
    const std::vector<Seen> s = k.seen();
    CHECK(s.size() == full.size());
    for (size_t i = 0; i < full.size(); ++i) {
        CHECK(s[i].body == full[i].body && s[i].session == 700 + i && s[i].connection == 1);
    }
    ClusterLink nowhere("not an address", 1);
    nowhere.armEmergencyCancel(g);  // disarms: nothing is sent
    ClusterLink::emergencyCancel();
    CHECK(k.count() == full.size());
}

}  // namespace

int main() {
    sharedConnection();
    reconnectOnce();
    refusedNotRetried();
    cookies();
    cancels();
    emergency();
    puts("PASS kombi client: sessions, one reconnection, refusals, cookies, cancel and fallback, emergency cancel");
    return 0;
}
