// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// The car's sound route for music over Wi-Fi (see audio_route.hpp).
#include "audio_route.hpp"

#include <errno.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <string>
#include <vector>

#include "evo_log.hpp"
#include "evo_process.hpp"
#include "vmost_client.hpp"

namespace evo {

namespace {
const char kTag[] = "AudioRoute";

struct Exchange {
    VmostClient vmost;
    uint16_t fkt, handle;
    int answer;  // the op of the car's final answer, -1 before it
    unsigned processing;
    std::vector<RouteEntry> table;  // the last connection table received
    bool tableIn;
    Exchange() : fkt(0), handle(0), answer(-1), processing(0), tableIn(false) {}
};

void onMessage(void* msg, void* user) {
    Exchange* x = static_cast<Exchange*>(user);
    VmostMsgView v;
    if (!x || !parseVmostMsg(msg, v) || v.fblock != kConnectionMasterFblock || v.inst != kConnectionMasterInst) {
        return;
    }
    if (v.fkt == kHighSyncConnectionTableExt && v.op == 0xC) {
        x->table = parseConnectionTable(v.data, v.len);
        x->tableIn = true;
        return;
    }
    if (v.fkt != x->fkt || v.len < 2 || ((v.data[0] << 8) | v.data[1]) != x->handle) {
        return;
    }
    if (v.op == 0xA) {
        ++x->processing;  // ProcessingAck: still working on it
    } else if (v.op == 0xD || v.op == 0x9) {
        x->answer = static_cast<int>(v.op);
    }
}

std::string selfPath() {
    char path[256];
    const ssize_t n = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (n <= 0) {
        return std::string();
    }
    path[n] = 0;
    return path;
}

struct Request {
    bool build, restore;
    uint16_t route;
    RouteDone done;
    void* user;
    unsigned ticket;  // its place in line
};

RouteRequester g_requester = nullptr;

// One request at a time, in the order made: a ticket each, served in turn.
pthread_mutex_t g_lineMu = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t g_lineCv = PTHREAD_COND_INITIALIZER;
unsigned g_nextTicket = 0, g_serving = 0;

void* runRequest(void* arg) {
    Request* r = static_cast<Request*>(arg);
    pthread_mutex_lock(&g_lineMu);
    while (g_serving != r->ticket) {
        pthread_cond_wait(&g_lineCv, &g_lineMu);
    }
    pthread_mutex_unlock(&g_lineMu);
    char route[8];
    snprintf(route, sizeof(route), "%x", r->route);
    const std::string self = selfPath();
    std::vector<std::string> args;
    args.push_back("--audio-route");
    args.push_back(r->build ? (r->restore ? "take" : "build") : (r->restore ? "give" : "remove"));
    args.push_back(route);
    std::string output;
    CommandReport report;
    const bool ok = !self.empty() && runCommand(self, args, output, 20000, &report);
    // runCommand clears output on failure, but its diagnostic keeps the helper's refusal.
    if (!ok) {
        output = report.text;
    }
    // The helper's own lines, then the outcome.
    size_t from = 0;
    while (from < output.size()) {
        size_t end = output.find('\n', from);
        if (end == std::string::npos) {
            end = output.size();
        }
        if (end > from) {
            ELOG(kTag, "%s", output.substr(from, end - from).c_str());
        }
        from = end + 1;
    }
    if (!ok && (self.empty() || report.timedOut || report.spawnError || report.signal)) {
        ELOG(kTag, "route 0x%x %s: the helper did not finish (exit %d%s)", r->route, r->build ? "build" : "release",
             report.exitCode, report.timedOut ? ", timed out" : "");
    }
    if (r->done) {
        r->done(ok && report.exitCode == 0, r->user);
    }
    pthread_mutex_lock(&g_lineMu);
    ++g_serving;
    pthread_cond_broadcast(&g_lineCv);
    pthread_mutex_unlock(&g_lineMu);
    delete r;
    return nullptr;
}
}  // namespace

void buildRoutePayload(uint8_t out[4], uint16_t handle, uint16_t route) {
    out[0] = static_cast<uint8_t>(handle >> 8);
    out[1] = static_cast<uint8_t>(handle);
    out[2] = static_cast<uint8_t>(route >> 8);
    out[3] = static_cast<uint8_t>(route);
}

std::vector<RouteEntry> parseConnectionTable(const uint8_t* data, size_t len) {
    std::vector<RouteEntry> table;
    for (size_t i = 2; i + 3 <= len; i += 3) {  // after [PosX][PosY]: [connection id u16][status u8]
        RouteEntry e;
        e.id = static_cast<uint16_t>((data[i] << 8) | data[i + 1]);
        e.status = data[i + 2];
        table.push_back(e);
    }
    return table;
}

std::vector<uint16_t> replacedConnections(const std::vector<RouteEntry>& before, const std::vector<RouteEntry>& after,
                                          uint16_t route) {
    std::vector<uint16_t> gone;
    for (size_t i = 0; i < before.size(); ++i) {
        bool still = before[i].id == route || before[i].status != kConnectionPlaying;
        for (size_t j = 0; j < after.size() && !still; ++j) {
            still = after[j].id == before[i].id;
        }
        if (!still) {
            gone.push_back(before[i].id);
        }
    }
    return gone;
}

bool somethingPlays(const std::vector<RouteEntry>& table) {
    for (size_t i = 0; i < table.size(); ++i) {
        if (table[i].status == kConnectionPlaying) {
            return true;
        }
    }
    return false;
}

namespace {
const char kReplacedFile[] = "/tmp/entry-aad-replaced";  // RAM: a reboot forgets it, as the car forgets the route

void pump(Exchange& x, int fd, uint32_t begin, unsigned waitMs, bool (*done)(const Exchange&)) {
    while (!done(x) && monoMs() - begin < waitMs) {
        struct pollfd p = {fd, POLLIN, 0};
        const int r = poll(&p, 1, 50);
        if (r < 0 && errno != EINTR) {
            break;
        }
        if (r > 0 && (p.revents & POLLIN) && x.vmost.processMessages() != 0) {
            break;
        }
    }
}

bool tableDone(const Exchange& x) { return x.tableIn; }
bool answerDone(const Exchange& x) { return x.answer >= 0; }

// Get [PosX 0][PosY 0]: the whole table, answered as one Status.
bool readTable(Exchange& x, int fd) {
    uint8_t pos[2] = {0, 0};
    VmostMsgOut m;
    memset(&m, 0, sizeof(m));
    m.addr = 0xFFFF;
    m.fblock = kConnectionMasterFblock;
    m.inst = kConnectionMasterInst;
    m.fkt = kHighSyncConnectionTableExt;
    m.op = 0x1;  // Get
    m.len = sizeof(pos);
    m.data = pos;
    x.tableIn = false;
    if (x.vmost.send(&m) != 0) {
        return false;
    }
    pump(x, fd, monoMs(), 1000, tableDone);
    return x.tableIn;
}

// One build or remove of a connection; a refused build is sent again (see below). The car's final op, -1 for none.
int ask(Exchange& x, int fd, bool build, uint16_t route, uint32_t begin, unsigned waitMs, unsigned& tries) {
    x.fkt = build ? kBuildHighSyncConnection : kRemoveHighSyncConnection;
    x.handle = static_cast<uint16_t>(kRouteHandleBase | (monoMs() & 0xff));
    x.answer = -1;
    x.processing = 0;
    uint8_t payload[4];
    buildRoutePayload(payload, x.handle, route);
    VmostMsgOut m;
    memset(&m, 0, sizeof(m));
    m.addr = 0xFFFF;
    m.fblock = kConnectionMasterFblock;
    m.inst = kConnectionMasterInst;
    m.fkt = x.fkt;
    m.op = kMostOpStartResultAck;
    m.len = sizeof(payload);
    m.data = payload;
    tries = 0;
    // BMW's CARPLAY sends a refused build again under the same handle (onBuildHighSyncConnectionErrorAckCallback,
    // up to 100 times): a refusal can be momentary. Here a few, spaced, within the prompt's wait for the car.
    while (tries < (build ? kBuildTries : 1u) && monoMs() - begin < waitMs) {
        if (tries) {
            usleep(kBuildRetryPauseMs * 1000);
            x.answer = -1;
        }
        ++tries;
        if (x.vmost.send(&m) != 0) {
            break;
        }
        pump(x, fd, begin, waitMs, answerDone);
        if (x.answer != 0x9) {
            break;  // granted, or no answer at all: sending again would not help
        }
    }
    return x.answer;
}

void printTable(const std::vector<RouteEntry>& table) {
    printf("connections:");
    for (size_t i = 0; i < table.size(); ++i) {
        printf(" 0x%x/%u", table[i].id, table[i].status);
    }
    printf("\n");
}
}  // namespace

// build|remove <route>; take|give <route>: the same, and take remembers the connections the car removed to make room
// (the radio's), give builds them again once nothing else plays; table 0: prints the connection table.
int audioRouteMain(const char* verb, const char* routeText, unsigned waitMs) {
    const bool take = !strcmp(verb, "take"), give = !strcmp(verb, "give"), table = !strcmp(verb, "table");
    const bool build = take || !strcmp(verb, "build");
    if (!build && !give && !table && strcmp(verb, "remove")) {
        return 2;
    }
    if (!build) {
        waitMs = waitMs < kReleaseWaitMs ? waitMs : kReleaseWaitMs;  // the service waits for it as it stops
    }
    char* end = nullptr;
    const unsigned long route = strtoul(routeText, &end, 16);
    if (!end || *end || (!route && !table) || route > 0xffff) {
        return 2;
    }
    Exchange x;
    if (x.vmost.load() != VmostClient::kLoaded) {
        printf("route 0x%lx: vMOST unavailable (%s)\n", route, x.vmost.error().c_str());
        return 1;
    }
    if (x.vmost.connect(onMessage, &x, 0) != 0) {
        printf("route 0x%lx: vmostd refused the connection\n", route);
        return 1;
    }
    int fd = -1;
    const int rc = x.vmost.shadowRegister(kConnectionMasterFblock, kConnectionMasterInst);
    if (rc != 0 || x.vmost.fileDescriptorGet(&fd) != 0 || fd < 0) {
        printf("route 0x%lx: ConnectionMaster registration refused (%d)\n", route, rc);
        x.vmost.disconnect();
        return 1;
    }
    if (table) {
        const bool in = readTable(x, fd);
        if (in) {
            printTable(x.table);
        }
        x.vmost.shadowDeregister(kConnectionMasterFblock, kConnectionMasterInst);
        x.vmost.disconnect();
        return in ? 0 : 1;
    }
    std::vector<RouteEntry> before;
    if (take && readTable(x, fd)) {
        before = x.table;
    }
    const uint32_t begin = monoMs();
    unsigned tries = 0;
    const int answer = ask(x, fd, build, static_cast<uint16_t>(route), begin, waitMs, tries);
    const char* what = build ? "connected to the speakers" : "given back";
    if (answer == 0xD) {
        printf("route 0x%lx %s by the car's audio manager after %u ms (try %u)\n", route, what, monoMs() - begin, tries);
    } else {
        printf("route 0x%lx not %s: %s after %u ms (%u processing answers, %u tries)\n", route, what,
               answer == 0x9 ? "the car refused it" : "no answer", monoMs() - begin, x.processing, tries);
    }
    if (take && answer == 0xD && readTable(x, fd)) {
        const std::vector<uint16_t> gone = replacedConnections(before, x.table, static_cast<uint16_t>(route));
        // None gone (already ours, or nothing played): what an earlier take remembered stays.
        if (!gone.empty()) {
            FILE* f = fopen(kReplacedFile, "w");
            for (size_t i = 0; f && i < gone.size(); ++i) {
                fprintf(f, "%x\n", gone[i]);
                printf("route 0x%lx took the place of connection 0x%x\n", route, gone[i]);
            }
            if (f) {
                fclose(f);
            }
        }
    }
    if (give) {
        FILE* f = fopen(kReplacedFile, "r");
        if (f && readTable(x, fd) && somethingPlays(x.table)) {
            printf("connection(s) given back to iDrive's own source: something else plays\n");
        } else if (f) {
            unsigned id = 0;
            while (fscanf(f, "%x", &id) == 1 && id && id <= 0xffff) {
                const int back = ask(x, fd, true, static_cast<uint16_t>(id), monoMs(), kReleaseWaitMs, tries);
                printf("connection 0x%x %s\n", id, back == 0xD ? "built again: the source before ours plays" : "not built again");
            }
        }
        if (f) {
            fclose(f);
            remove(kReplacedFile);
        }
    }
    x.vmost.shadowDeregister(kConnectionMasterFblock, kConnectionMasterInst);
    x.vmost.disconnect();
    return answer == 0xD ? 0 : 1;
}

void setAudioRouteRequesterForTest(RouteRequester requester) {
    g_requester = requester;
}

void requestAudioRoute(bool build, uint16_t route, bool wait, RouteDone done, void* user, bool restore) {
    if (g_requester) {
        g_requester(build, route, done, user);
        return;
    }
    Request* r = new Request;
    r->build = build;
    r->restore = restore;
    r->route = route;
    r->done = done;
    r->user = user;
    pthread_mutex_lock(&g_lineMu);
    r->ticket = g_nextTicket++;
    pthread_mutex_unlock(&g_lineMu);
    if (wait) {
        runRequest(r);
        return;
    }
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_t t;
    if (pthread_create(&t, &attr, runRequest, r) != 0) {
        ELOG(kTag, "route 0x%x: no thread to ask for it", route);
        pthread_mutex_lock(&g_lineMu);  // its turn is given up, or the line would stop here
        while (g_serving != r->ticket) {
            pthread_cond_wait(&g_lineCv, &g_lineMu);
        }
        ++g_serving;
        pthread_cond_broadcast(&g_lineCv);
        pthread_mutex_unlock(&g_lineMu);
        if (done) {
            done(false, user);
        }
        delete r;
    }
    pthread_attr_destroy(&attr);
}

void waitForAudioRoutes() {
    pthread_mutex_lock(&g_lineMu);
    const unsigned end = g_nextTicket;
    while (static_cast<int>(g_serving - end) < 0) {
        pthread_cond_wait(&g_lineCv, &g_lineMu);
    }
    pthread_mutex_unlock(&g_lineMu);
}

}  // namespace evo
