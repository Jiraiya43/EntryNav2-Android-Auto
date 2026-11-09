// Copyright (c) 2026 Jiraiya43
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Control protocol between the iDrive app and the Android Auto service (real UDP loopback, real state file).
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <fstream>
#include <sstream>
#include <vector>

#include "evo_control.hpp"
#define CHECK(x)                                                 \
    do {                                                         \
        if (!(x)) {                                              \
            fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); \
            exit(1);                                             \
        }                                                        \
    } while (0)
static std::vector<std::string> seen;
static void onCmd(const evo::ControlCommand& c, void*) {
    char b[48];
    snprintf(b, sizeof(b), "%s%s%d", c.verb.c_str(), c.hasArg ? ":" : "", c.hasArg ? c.arg : 0);
    seen.push_back(c.hasArg ? std::string(b) : c.verb);
}
static bool send(uint16_t port, const char* m, char* reply = 0) {
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_port = htons(port);
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    bool ok = sendto(fd, m, strlen(m), 0, (sockaddr*)&a, sizeof(a)) == (ssize_t)strlen(m);
    if (reply) {
        timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        ssize_t n = recv(fd, reply, 15, 0);
        reply[n > 0 ? n : 0] = 0;
    }
    close(fd);
    return ok;
}
static std::string text(const char* p) {
    std::ifstream f(p);
    std::ostringstream o;
    o << f.rdbuf();
    return o.str();
}
int main() {
    evo::ControlCommand c;
    CHECK(evo::parseControlCommand("gear 2", 6, c) && c.verb == "gear" && c.hasArg && c.arg == 2);
    CHECK(evo::parseControlCommand("show\n", 5, c) && c.verb == "show" && !c.hasArg);
    CHECK(evo::parseControlCommand("drivemode 0 3 1", 15, c) == false);                 // 3 args: refused
    CHECK(evo::parseControlCommand("call 1", 6, c) && c.verb == "call" && c.arg == 1);  // phone keys during a call
    CHECK(evo::parseControlCommand("call", 4, c) == false);
    CHECK(evo::parseControlCommand("popup 1", 7, c) && c.verb == "popup" && c.arg == 1);  // iDrive popup
    CHECK(evo::parseControlCommand("popup", 5, c) == false);
    CHECK(!evo::parseControlCommand("gear", 4, c) && !evo::parseControlCommand("show 1", 6, c));
    CHECK(!evo::parseControlCommand("reboot", 6, c) && !evo::parseControlCommand("", 0, c));
    CHECK(!evo::parseControlCommand("gear 2 x", 8, c));
    const uint16_t port = 52790 + (getpid() % 1000) + 1;
    char state[] = "/tmp/entry-aad-state-test-XXXXXX";
    int sfd = mkstemp(state);
    CHECK(sfd >= 0);
    close(sfd);
    unlink(state);
    evo::EvoControl ctl;
    CHECK(ctl.start(port, state, onCmd, 0));
    evo::EvoControl second;
    CHECK(!second.start(port, state, onCmd, 0));  // one service only
    char reply[16];
    CHECK(send(port, "ping", reply) && !strcmp(reply, "pong"));
    CHECK(send(port, "show") && send(port, "gear 2") && send(port, "bogus") && send(port, "hide") &&
          send(port, "quit"));
    usleep(400000);
    CHECK(seen.size() == 4 && seen[0] == "show" && seen[1] == "gear:2" && seen[2] == "hide" && seen[3] == "quit");
    ctl.writeState("connecting", 0);
    char expect[64];
    snprintf(expect, sizeof(expect), "connecting %d 0\n", (int)getpid());
    CHECK(text(state) == expect);
    ctl.writeState("connected", 1);
    snprintf(expect, sizeof(expect), "connected %d 1\n", (int)getpid());
    CHECK(text(state) == expect);
    ctl.stop();
    CHECK(access(state, F_OK) != 0);  // removed on stop: the app shows "Android Auto" again
    puts(
        "PASS controle appli iDrive : ping/pong, show/hide/quit/gear, commandes inconnues refusees, port unique, "
        "fichier d'etat atomique et supprime a l'arret");
    return 0;
}
