// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// audioRouteMain against tests/fake_vmostclient.c: a refused build is sent again, up to kBuildTries; a remove is not.
// usage: test-audio-route <path of fake-vmostclient.so>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <string>

#include "audio_route.hpp"
#include "vmost_client.hpp"

namespace {
const char kLog[] = "/tmp/test-audio-route.log";

int sends() {
    FILE* f = fopen(kLog, "r");
    int n = 0;
    char line[256];
    while (f && fgets(line, sizeof(line), f)) {
        n += std::string(line).compare(0, 5, "send ") == 0;
    }
    if (f) {
        fclose(f);
    }
    return n;
}

int run(const char* verb, const char* refusals) {
    remove(kLog);
    setenv("FAKE_ROUTE_REFUSALS", refusals, 1);
    return evo::audioRouteMain(verb, "31a", 5000);
}
}  // namespace

int main(int argc, char** argv) {
    assert(argc == 2);
    setenv("FAKE_VMOST_LOG", kLog, 1);
    // A socket where vmostd's would be: VmostClient only loads the library when it finds one.
    char dir[] = "/tmp/entry-route-test-XXXXXX";
    assert(mkdtemp(dir));
    const std::string sock = std::string(dir) + "/vmost_sockstream";
    struct sockaddr_un a;
    memset(&a, 0, sizeof(a));
    a.sun_family = AF_UNIX;
    strncpy(a.sun_path, sock.c_str(), sizeof(a.sun_path) - 1);
    const int s = socket(AF_UNIX, SOCK_STREAM, 0);
    assert(s >= 0 && bind(s, reinterpret_cast<sockaddr*>(&a), sizeof(a)) == 0);
    evo::setVmostForTest(sock.c_str(), argv[1]);
    assert(run("build", "0") == 0 && sends() == 1);  // granted at once
    assert(run("build", "2") == 0 && sends() == 3);  // two refusals, then granted
    assert(run("build", "99") == 1 && sends() == static_cast<int>(evo::kBuildTries));  // gives up
    assert(run("remove", "1") == 1 && sends() == 1);  // a release is not sent again
    close(s);
    unlink(sock.c_str());
    rmdir(dir);
    // The car's tables of 21 November 2026: FM, then our music, then our music released.
    const uint8_t fm[] = {0, 0, 0x00, 0x01, 0, 0x00, 0x84, 2, 0x00, 0x85, 2};
    const uint8_t ours[] = {0, 0, 0x00, 0x84, 2, 0x00, 0x85, 2, 0x03, 0x15, 0};
    const uint8_t none[] = {0, 0, 0x00, 0x84, 2, 0x00, 0x85, 2};
    const std::vector<evo::RouteEntry> tFm = evo::parseConnectionTable(fm, sizeof(fm));
    const std::vector<evo::RouteEntry> tOurs = evo::parseConnectionTable(ours, sizeof(ours));
    const std::vector<evo::RouteEntry> tNone = evo::parseConnectionTable(none, sizeof(none));
    assert(tFm.size() == 3 && tFm[0].id == 1 && tFm[0].status == 0 && tFm[2].id == 0x85 && tFm[2].status == 2);
    const std::vector<uint16_t> gone = evo::replacedConnections(tFm, tOurs, 0x315);
    assert(gone.size() == 1 && gone[0] == 1);                            // the radio made room for our music
    assert(evo::replacedConnections(tNone, tOurs, 0x315).empty());       // nothing played: nothing to give back
    assert(evo::replacedConnections(tOurs, tOurs, 0x315).empty());       // already ours
    assert(evo::somethingPlays(tFm) && evo::somethingPlays(tOurs) && !evo::somethingPlays(tNone));
    assert(evo::parseConnectionTable(fm, 1).empty());
    puts("PASS audio route: a refused build sent again up to the limit, a release once; connection table and the source "
         "our route replaced");
    return 0;
}
