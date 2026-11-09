// Copyright (c) 2026 Jiraiya43
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// A fake phone answers the Bluetooth handshake that hands it the car's Wi-Fi; the head unit's bytes must match the
// golden.

// The link is a SOCK_SEQPACKET socketpair: like the car's RFCOMM device, it keeps each message whole. Recorded, in
// order: every message each side writes, the handshake's verdict, and the BMW-app check on chosen first messages.
//   test-wireless-phone <golden>            compare (tests/fixtures/wireless_phone.golden)
//   test-wireless-phone --write <golden>    write the golden from the current code
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <initializer_list>
#include <string>
#include <vector>

#include "wireless_setup.hpp"

namespace {

typedef std::vector<uint8_t> Bytes;

const uint16_t kPort = 50000;
const int kWaitMs = 4000;

Bytes bytes(std::initializer_list<int> list) {
    Bytes out;
    for (int v : list) {
        out.push_back(static_cast<uint8_t>(v));
    }
    return out;
}

Bytes operator+(Bytes a, const Bytes& b) {
    a.insert(a.end(), b.begin(), b.end());
    return a;
}

Bytes text(const char* s) {
    return Bytes(s, s + strlen(s));
}

// A phone message: [u16 length][u16 id][body].
Bytes message(int id, const Bytes& body) {
    return bytes({static_cast<int>(body.size() >> 8), static_cast<int>(body.size() & 255), id >> 8, id & 255}) + body;
}

std::string hex(const Bytes& b) {
    static const char digits[] = "0123456789abcdef";
    std::string s;
    for (size_t i = 0; i < b.size(); ++i) {
        s += digits[b[i] >> 4];
        s += digits[b[i] & 15];
    }
    return s.empty() ? "-" : s;
}

std::string format(const char* fmt, ...) {
    char line[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);
    return line;
}

aa::ApInfo carWifi() {
    aa::ApInfo ap;
    ap.ssid = "EntryNav2 test";
    ap.key = "not-a-real-key";
    ap.bssid = "02:00:00:00:00:01";
    ap.ip = "192.168.43.1";
    return ap;
}

// The head unit side, on its own thread: the handshake, then the service loop as the service runs it.
struct HeadUnit {
    int fd;
    int peekMs;
    int timeoutMs;
    bool serve;
    bool result;
    bool spoke;
    pthread_t thread;
    static void* run(void* self) {
        HeadUnit& hu = *static_cast<HeadUnit*>(self);
        const aa::ApInfo ap = carWifi();
        aa::WirelessSetup setup(hu.fd, ap, kPort, hu.peekMs);
        hu.result = setup.handshake(hu.timeoutMs);
        hu.spoke = setup.spokeWireless();
        while (hu.serve && hu.result && setup.serve(300)) {
        }
        shutdown(hu.fd, SHUT_RDWR);
        return nullptr;
    }
};

class Phone {
public:
    Phone(std::vector<std::string>& log, int peekMs, int timeoutMs, bool serve) : log_(log) {
        int fds[2];
        if (socketpair(AF_UNIX, SOCK_SEQPACKET, 0, fds) != 0) {
            fprintf(stderr, "FAIL socketpair\n");
            exit(1);
        }
        fd_ = fds[1];
        hu_.fd = fds[0];
        hu_.peekMs = peekMs;
        hu_.timeoutMs = timeoutMs;
        hu_.serve = serve;
        hu_.result = hu_.spoke = false;
    }
    // Before the head unit starts: the phone speaks first.
    void early(const Bytes& data) {
        log_.push_back("ph " + hex(data));
        write(data);
    }
    void start() {
        if (pthread_create(&hu_.thread, nullptr, HeadUnit::run, &hu_) != 0) {
            fprintf(stderr, "FAIL thread\n");
            exit(1);
        }
    }
    void send(const Bytes& data) {
        log_.push_back("ph " + hex(data));
        write(data);
    }
    // The next head unit message, recorded; false when it closed or stayed silent.
    bool expect() {
        pollfd pfd = {fd_, POLLIN, 0};
        if (poll(&pfd, 1, kWaitMs) <= 0) {
            log_.push_back("hu <silent>");
            return false;
        }
        uint8_t buf[4096];
        const ssize_t n = read(fd_, buf, sizeof(buf));
        if (n <= 0) {
            log_.push_back("hu <closed>");
            return false;
        }
        log_.push_back("hu " + hex(Bytes(buf, buf + n)));
        return true;
    }
    // The phone hangs up, or waits for the head unit to give up; then the verdict.
    void hangUp() {
        log_.push_back("ph close");
        shutdown(fd_, SHUT_WR);
        finish();
    }
    void finish() {
        pthread_join(hu_.thread, nullptr);
        Bytes rest;
        uint8_t buf[4096];
        ssize_t n;
        while ((n = recv(fd_, buf, sizeof(buf), MSG_DONTWAIT)) > 0) {
            log_.push_back("hu " + hex(Bytes(buf, buf + n)));
        }
        log_.push_back(format("handshake=%d spoke=%d", hu_.result ? 1 : 0, hu_.spoke ? 1 : 0));
        close(fd_);
        close(hu_.fd);
    }

private:
    void write(const Bytes& data) {
        if (::write(fd_, &data[0], data.size()) != static_cast<ssize_t>(data.size())) {
            log_.push_back("ph <write failed>");
        }
    }
    std::vector<std::string>& log_;
    int fd_;
    HeadUnit hu_;
};

const Bytes kInfoRequest = message(2, Bytes());
const Bytes kStartAccepted = message(7, bytes({0x18, 0x00}));
const Bytes kConnected = message(6, bytes({0x08, 0x00}));

void standard(std::vector<std::string>& log) {
    log.push_back("-- start, info, connected; then the link stays served: ping, info again, hang up");
    Phone p(log, 0, 10000, true);
    p.start();
    p.expect();
    p.send(kStartAccepted);
    p.send(kInfoRequest);
    p.expect();
    p.send(kConnected);
    p.send(message(8, bytes({0x08, 0x05})));
    p.expect();
    p.send(kInfoRequest);
    p.expect();
    p.send(message(8, Bytes()));
    p.expect();
    p.hangUp();
}

void closings(std::vector<std::string>& log) {
    log.push_back("-- closed after accepting the start");
    {
        Phone p(log, 0, 10000, false);
        p.start();
        p.expect();
        p.send(kStartAccepted);
        p.send(kInfoRequest);
        p.expect();
        p.hangUp();
    }
    log.push_back("-- closed before accepting the start");
    {
        Phone p(log, 0, 10000, false);
        p.start();
        p.expect();
        p.send(kInfoRequest);
        p.expect();
        p.hangUp();
    }
    log.push_back("-- closed before asking for the info");
    {
        Phone p(log, 0, 10000, false);
        p.start();
        p.expect();
        p.send(kStartAccepted);
        p.hangUp();
    }
}

void statuses(std::vector<std::string>& log) {
    log.push_back("-- start response with ip, port and status 0 (fields 2, 3, 4); connect status 12");
    {
        Phone p(log, 0, 10000, false);
        p.start();
        p.expect();
        p.send(message(7, bytes({0x12, 0x0c}) + text("192.168.43.2") + bytes({0x18, 0xd0, 0x86, 0x03, 0x20, 0x00})));
        p.send(kInfoRequest);
        p.expect();
        p.send(message(6, bytes({0x08, 0x0c})));
        p.finish();
    }
    log.push_back("-- start refused (status 4 = 1), then closed");
    {
        Phone p(log, 0, 10000, false);
        p.start();
        p.expect();
        p.send(message(7, bytes({0x18, 0x00, 0x20, 0x01})));
        p.send(kInfoRequest);
        p.expect();
        p.hangUp();
    }
    log.push_back("-- start with field 3 only, non-zero: not accepted");
    {
        Phone p(log, 0, 10000, false);
        p.start();
        p.expect();
        p.send(message(7, bytes({0x18, 0x05})));
        p.send(message(7, Bytes()));
        p.send(kInfoRequest);
        p.expect();
        p.hangUp();
    }
    log.push_back("-- connect status 3, then status in field 2, then empty");
    {
        Phone p(log, 0, 10000, false);
        p.start();
        p.expect();
        p.send(kStartAccepted);
        p.send(kInfoRequest);
        p.expect();
        p.send(message(6, bytes({0x08, 0x03})));
        p.send(message(6, bytes({0x10, 0x03})));
        p.send(message(6, Bytes()));
        p.finish();
    }
    log.push_back("-- connect status 3, then closed after accepting");
    {
        Phone p(log, 0, 10000, false);
        p.start();
        p.expect();
        p.send(kStartAccepted);
        p.send(kInfoRequest);
        p.expect();
        p.send(message(6, bytes({0x08, 0x03})));
        p.hangUp();
    }
    log.push_back("-- connect status in field 2 = 0");
    {
        Phone p(log, 0, 10000, false);
        p.start();
        p.expect();
        p.send(kInfoRequest);
        p.expect();
        p.send(message(6, bytes({0x10, 0x00})));
        p.finish();
    }
}

void phoneFirst(std::vector<std::string>& log) {
    log.push_back("-- the phone sends its own start request");
    Phone p(log, 0, 10000, false);
    p.start();
    p.expect();
    p.send(message(1, bytes({0x0a, 0x0c}) + text("192.168.43.9") + bytes({0x10, 0xd0, 0x86, 0x03})));
    p.expect();
    p.send(message(5, bytes({0x08, 0x01})));
    p.send(message(20, bytes({0x0a, 0x02, 0xff, 0xfe})));
    p.send(message(32, Bytes()));
    p.send(message(8, Bytes()));
    p.expect();
    p.send(message(9, bytes({0x15, 1, 2, 3, 4, 0x19, 1, 2, 3, 4, 5, 6, 7, 8})));
    p.send(message(9, bytes({0x0b, 0x0c})));
    p.send(kInfoRequest);
    p.expect();
    p.send(kConnected);
    p.finish();
}

void framing(std::vector<std::string>& log) {
    log.push_back("-- garbage before a message: dropped a byte at a time");
    {
        Phone p(log, 0, 10000, false);
        p.start();
        p.expect();
        p.send(bytes({0xff, 0xff, 0x00, 0x00, 0x00, 0x21}) + kInfoRequest);
        p.expect();
        p.send(kConnected);
        p.finish();
    }
    log.push_back("-- a message in two writes, two messages in one");
    {
        Phone p(log, 0, 10000, false);
        p.start();
        p.expect();
        p.send(bytes({0x00, 0x02}));
        p.send(bytes({0x00, 0x07, 0x18, 0x00}) + kInfoRequest);
        p.expect();
        p.send(bytes({0x12, 0x34, 0x56, 0x78}));
        p.send(kConnected);
        p.finish();
    }
}

void bmwApp(std::vector<std::string>& log) {
    log.push_back("-- BMW app startup magic before we write (peek 300 ms)");
    {
        Phone p(log, 300, 10000, false);
        p.early(bytes({0x12, 0x34, 0x56, 0x78}));
        p.start();
        p.finish();
    }
    log.push_back("-- BMW app watchdog before we write");
    {
        Phone p(log, 300, 10000, false);
        p.early(bytes({0x00, 0x02, 0x13, 0x89, 0x13, 0x89, 0x00, 0x04, 0x13, 0x37, 0x13, 0x37}));
        p.start();
        p.finish();
    }
    log.push_back("-- link closed during the peek");
    {
        Phone p(log, 300, 10000, false);
        p.start();
        p.hangUp();
    }
    log.push_back("-- silent during the peek, then Android Auto");
    {
        Phone p(log, 300, 10000, false);
        p.start();
        p.expect();
        p.send(kInfoRequest);
        p.expect();
        p.send(kConnected);
        p.finish();
    }
    log.push_back("-- BMW app answering our start request (Etch)");
    {
        Phone p(log, 0, 10000, false);
        p.start();
        p.expect();
        p.send(bytes({0x00, 0x02, 0x00, 0x11, 0x0f, 0xa4, 0x00, 0x3c, 0xde, 0xad, 0xbe, 0xef, 0x01}));
        p.finish();
    }
}

void timeouts(std::vector<std::string>& log) {
    log.push_back("-- start accepted, no info request: two resends, then give up");
    {
        Phone p(log, 0, 200, false);
        p.start();
        p.expect();
        p.send(kStartAccepted);
        p.expect();
        p.expect();
        p.finish();
    }
    log.push_back("-- nothing at all: give up");
    {
        Phone p(log, 0, 200, false);
        p.start();
        p.expect();
        p.finish();
    }
}

void recognition(std::vector<std::string>& log) {
    log.push_back("-- first messages taken for the BMW app");
    const Bytes samples[] = {
        bytes({0x12, 0x34, 0x56, 0x78}),
        bytes({0x12, 0x34, 0x56}),
        bytes({0x12, 0x34, 0x56, 0x78, 0x00}),
        bytes({0x00, 0x02, 0x00, 0x11, 0x0f, 0xa4, 0x00, 0x3c, 0xde, 0xad, 0xbe, 0xef}),
        bytes({0x00, 0x08, 0x00, 0x07, 0x00, 0x00, 0x00, 0x00, 0xde, 0xad, 0xbe, 0xef}),
        bytes({0x00, 0x02, 0x13, 0x89, 0x13, 0x89, 0x00, 0x04, 0x13, 0x37, 0x13, 0x37}),
        bytes({0x00, 0x02, 0x13, 0x89, 0x13, 0x89, 0x00, 0x02, 0xaa, 0xbb}),
        bytes({0x00, 0x21, 0x13, 0x89, 0x13, 0x89, 0x00, 0x02, 0xaa, 0xbb}),
        bytes({0x00, 0x00, 0x13, 0x89, 0x13, 0x89, 0x00, 0x02, 0xaa, 0xbb}),
        bytes({0x00, 0x02, 0x00, 0x07, 0x18, 0x00}),
        bytes({0x00, 0x00, 0x00, 0x02}),
        bytes({0x00, 0x00, 0x00, 0x21}),
        bytes({0x00, 0x05, 0x00, 0x07, 0x18, 0x00}),
        bytes({0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06}),
    };
    for (size_t i = 0; i < sizeof(samples) / sizeof(samples[0]); ++i) {
        log.push_back(format("bmw=%d ", aa::looksLikeBmwBcl(&samples[i][0], samples[i].size()) ? 1 : 0) +
                      hex(samples[i]));
    }
    log.push_back(format("bmw=%d null", aa::looksLikeBmwBcl(nullptr, 4) ? 1 : 0));
}

struct Scenario {
    const char* name;
    void (*run)(std::vector<std::string>&);
};

const Scenario kScenarios[] = {{"standard", standard},      {"closings", closings},      {"statuses", statuses},
                               {"phone first", phoneFirst}, {"framing", framing},        {"bmw app", bmwApp},
                               {"timeouts", timeouts},      {"recognition", recognition}};

void timedOut(int) {
    static const char msg[] = "FAIL test-wireless-phone: no progress\n";
    ssize_t w = write(2, msg, sizeof(msg) - 1);
    (void)w;
    _exit(1);
}

bool readLines(const char* path, std::vector<std::string>& lines) {
    FILE* f = fopen(path, "r");
    if (!f) {
        return false;
    }
    std::string line;
    int c;
    while ((c = fgetc(f)) != EOF) {
        if (c == '\n') {
            lines.push_back(line);
            line.clear();
        } else {
            line += static_cast<char>(c);
        }
    }
    fclose(f);
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    const bool writing = argc == 3 && !strcmp(argv[1], "--write");
    const char* golden = writing ? argv[2] : (argc == 2 ? argv[1] : "tests/fixtures/wireless_phone.golden");
    signal(SIGPIPE, SIG_IGN);
    signal(SIGALRM, timedOut);
    alarm(120);
    fflush(stdout);
    const int console = dup(1);
    const int sink = open("/dev/null", O_WRONLY);
    if (console < 0 || sink < 0 || dup2(sink, 1) < 0) {
        return 1;
    }
    std::vector<std::string> out;
    for (size_t i = 0; i < sizeof(kScenarios) / sizeof(kScenarios[0]); ++i) {
        out.push_back(std::string("== ") + kScenarios[i].name);
        kScenarios[i].run(out);
    }
    fflush(stdout);
    dup2(console, 1);
    close(sink);
    if (writing) {
        FILE* f = fopen(golden, "w");
        for (size_t i = 0; f && i < out.size(); ++i) {
            fprintf(f, "%s\n", out[i].c_str());
        }
        if (!f || fclose(f) != 0) {
            printf("FAIL cannot write %s\n", golden);
            return 1;
        }
        printf("WROTE %s (%u lines)\n", golden, static_cast<unsigned>(out.size()));
        return 0;
    }
    std::vector<std::string> want;
    if (!readLines(golden, want)) {
        printf("FAIL cannot read %s\n", golden);
        return 1;
    }
    for (size_t i = 0; i < out.size() || i < want.size(); ++i) {
        const std::string got = i < out.size() ? out[i] : "<missing>";
        const std::string expected = i < want.size() ? want[i] : "<missing>";
        if (got != expected) {
            printf("FAIL wireless, golden line %u\n  got:  %s\n  want: %s\n", static_cast<unsigned>(i + 1), got.c_str(),
                   expected.c_str());
            return 1;
        }
    }
    printf(
        "PASS Bluetooth handshake with a fake phone: %u lines as the golden (start, info, statuses, phone-initiated "
        "start, ping, framing, BMW app, resends, timeouts)\n",
        static_cast<unsigned>(out.size()));
    return 0;
}
