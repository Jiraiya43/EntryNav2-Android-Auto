// Copyright (c) 2026 Jiraiya43
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// TCP client of the cluster's route-guidance service: the shared connection, the cancel, the emergency cancel.
#include "kombi_client.hpp"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstring>

#include "evo_log.hpp"

namespace evo {
namespace kombi {

namespace {

const int kMaxCookies = 4;               // skipped while waiting for one answer
const int kEmergencyTimeoutMs = 1000;    // to connect, and per answer
const uint16_t kCancelSession = 400;     // first session number of the cancel
const uint16_t kFallbackSession = 500;   // of its guidance-state-0 fallback
const uint16_t kEmergencySession = 700;  // of the cancel from a fatal signal
const useconds_t kPauseStepUs = 100000;  // the pause between cancels, checking giveUp at this pace
const int kClosed = -1;                  // readExactly(), readMessage(): the cluster closed the connection
const int kBadLength = -2;               // readMessage(): a length field out of bounds

void explain(std::string* why, const char* what, int err = 0) {
    if (!why) {
        return;
    }
    char text[160];
    if (err) {
        snprintf(text, sizeof(text), "%s (%s)", what, strerror(err));
    } else {
        snprintf(text, sizeof(text), "%s", what);
    }
    *why = text;
}

// Blocking, with the socket's timeouts: 0, kClosed, or an errno value. Async-signal-safe.
int readExactly(int fd, uint8_t* p, size_t n) {
    while (n) {
        const ssize_t got = recv(fd, p, n, 0);
        if (got == 0) {
            return kClosed;
        }
        if (got < 0) {
            if (errno == EINTR) {
                continue;
            }
            return errno ? errno : EIO;
        }
        p += got;
        n -= static_cast<size_t>(got);
    }
    return 0;
}

int writeAll(int fd, const uint8_t* p, size_t n) {
    while (n) {
        const ssize_t put = send(fd, p, n, 0);
        if (put < 0) {
            if (errno == EINTR) {
                continue;
            }
            return errno ? errno : EIO;
        }
        p += put;
        n -= static_cast<size_t>(put);
    }
    return 0;
}

void setTimeouts(int fd, int ms) {
    struct timeval limit;
    limit.tv_sec = ms / 1000;
    limit.tv_usec = (ms % 1000) * 1000;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &limit, sizeof(limit));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &limit, sizeof(limit));
}

// One message from the cluster, header and body: 0, kClosed, kBadLength or an errno value.
int readMessage(int fd, Bytes& message) {
    message.resize(kHeaderSize);
    const int e = readExactly(fd, &message[0], kHeaderSize);
    if (e) {
        return e;
    }
    const uint32_t length = readBE32(&message[4]);
    if (length < 8 || length > kMaxLength) {
        return kBadLength;
    }
    message.resize(8 + length);
    return length > 8 ? readExactly(fd, &message[kHeaderSize], length - 8) : 0;
}

// The answer to one emergency request, whatever it says: false when the connection failed. After four magic cookies
// the fifth counts as the answer. Async-signal-safe.
bool skipAnswer(int fd) {
    for (int cookies = 0;;) {
        uint8_t header[kHeaderSize];
        if (readExactly(fd, header, sizeof(header))) {
            return false;
        }
        const uint32_t length = readBE32(header + 4);
        if (length < 8 || length > kMaxLength) {
            return false;
        }
        uint8_t chunk[64];
        for (size_t left = length - 8; left;) {
            const size_t n = left < sizeof(chunk) ? left : sizeof(chunk);
            if (readExactly(fd, chunk, n)) {
                return false;
            }
            left -= n;
        }
        if (!isMagicCookie(decodeHeader(header)) || ++cookies > kMaxCookies) {
            return true;
        }
    }
}

// The cancel a fatal-signal handler sends: its requests, encoded beforehand, and the cluster's address.
struct Emergency {
    uint8_t bytes[2048];
    size_t sizes[8];
    size_t count;
    struct sockaddr_in to;
};
Emergency g_emergency;
volatile int g_emergencyArmed = 0;

}  // namespace

ClusterLink::ClusterLink(const std::string& address, uint16_t port, int timeoutMs)
    : address_(address), port_(port), timeoutMs_(timeoutMs), fd_(-1), session_(0) {}

ClusterLink::~ClusterLink() {
    disconnect();
}

void ClusterLink::disconnect() {
    if (fd_ >= 0) {
        ::close(fd_);
    }
    fd_ = -1;
}

// A connection to the cluster, connected within timeoutMs_, or -1 with errno set.
int ClusterLink::open() const {
    struct sockaddr_in to;
    memset(&to, 0, sizeof(to));
    to.sin_family = AF_INET;
    to.sin_port = htons(port_);
    if (inet_pton(AF_INET, address_.c_str(), &to.sin_addr) != 1) {
        errno = EINVAL;
        return -1;
    }
    const int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return -1;
    }
    const int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    int err = 0;
    if (connect(fd, reinterpret_cast<const struct sockaddr*>(&to), sizeof(to)) < 0) {
        err = errno;
        if (err == EINPROGRESS) {
            struct pollfd p;
            p.fd = fd;
            p.events = POLLOUT;
            p.revents = 0;
            int ready;
            do {
                ready = poll(&p, 1, timeoutMs_);
            } while (ready < 0 && errno == EINTR);
            socklen_t size = sizeof(err);
            err = 0;
            if (ready <= 0) {
                err = ready == 0 ? ETIMEDOUT : errno;
            } else if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &size) < 0) {
                err = errno;
            }
        }
    }
    if (err) {
        ::close(fd);
        errno = err;
        return -1;
    }
    fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
    setTimeouts(fd, timeoutMs_);
    const int one = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
    return fd;
}

// One request and its answer, magic cookies skipped. False with *refused: the cluster answered something other than
// an acknowledgement; false alone: the connection failed.
bool ClusterLink::exchange(int fd, const Request& request, uint16_t session, std::string* why, bool* refused) const {
    *refused = false;
    if (!isAllowedMethod(request.method)) {
        *refused = true;
        explain(why, "method not allowed");
        return false;
    }
    const Bytes bytes = encodeRequest(request.method, request.body, session);
    int e = writeAll(fd, &bytes[0], bytes.size());
    if (e) {
        explain(why, "send failed", e);
        return false;
    }
    Bytes answer;
    Header reply = Header();
    for (int cookies = 0;; ++cookies) {
        e = readMessage(fd, answer);
        if (e == kClosed) {
            explain(why, "connection closed by the KOMBI");
            return false;
        }
        if (e == kBadLength) {
            explain(why, "invalid SOME/IP length");
            return false;
        }
        if (e) {
            explain(why, "no reply", e);
            return false;
        }
        reply = decodeHeader(&answer[0]);
        if (!isMagicCookie(reply)) {
            break;
        }
        if (cookies >= kMaxCookies) {
            explain(why, "only magic cookies received");
            return false;
        }
    }
    if (!acknowledges(reply, request.method, session)) {
        *refused = true;
        if (why) {
            char text[120];
            snprintf(text, sizeof(text), "method 0x%04X: reply type 0x%02X return code 0x%02X (not a matching E_OK)",
                     request.method, reply.type, reply.code);
            *why = text;
        }
        return false;
    }
    return true;
}

// Reads away what the cluster sent while we were idle. False: the connection is lost.
bool ClusterLink::readQueued(std::string* why) {
    for (;;) {
        struct pollfd p;
        p.fd = fd_;
        p.events = POLLIN;
        p.revents = 0;
        if (poll(&p, 1, 0) <= 0) {
            return true;
        }
        Bytes message;
        const int e = readMessage(fd_, message);
        if (e) {
            explain(why, e == kClosed ? "connection closed by the KOMBI" : "drain failed");
            return false;
        }
    }
}

SendOutcome ClusterLink::send(const std::vector<Request>& requests, std::string* why) {
    for (size_t i = 0; i < requests.size(); ++i) {
        for (int attempt = 1;; ++attempt) {
            if (fd_ < 0) {
                fd_ = open();
                if (fd_ < 0) {
                    explain(why, "connect failed", errno);
                    if (attempt == 2) {
                        return kLinkDown;
                    }
                    continue;
                }
            }
            bool refused = false;
            if (readQueued(why)) {
                session_ = static_cast<uint16_t>(session_ % 0xFFFE + 1);
                if (exchange(fd_, requests[i], session_, why, &refused)) {
                    break;
                }
            }
            if (refused) {
                return kRefused;
            }
            disconnect();
            if (attempt == 2) {
                return kLinkDown;
            }
        }
    }
    return kSent;
}

// The requests on a connection of their own, numbered from firstSession.
bool ClusterLink::sendAll(const std::vector<Request>& requests, uint16_t firstSession, std::string* why) const {
    const int fd = open();
    if (fd < 0) {
        explain(why, "connect failed", errno);
        return false;
    }
    bool ok = true;
    for (size_t i = 0; i < requests.size() && ok; ++i) {
        bool refused = false;
        ok = exchange(fd, requests[i], static_cast<uint16_t>(firstSession + i), why, &refused);
    }
    ::close(fd);
    return ok;
}

bool ClusterLink::cancel(const RouteGraph& graph, double retryForS, double pauseS, GiveUp giveUp, void* user,
                         std::string* why) {
    std::vector<Request> full, fallback;
    graph.cancel(full);
    graph.cancelGuidance(fallback);
    const double until = upSec() + retryForS;
    for (;;) {
        disconnect();
        if (sendAll(full, kCancelSession, why) || sendAll(fallback, kFallbackSession, why)) {
            return true;
        }
        if (upSec() + pauseS > until || (giveUp && giveUp(user))) {
            return false;
        }
        for (double waited = 0; waited < pauseS && !(giveUp && giveUp(user)); waited += kPauseStepUs / 1e6) {
            usleep(kPauseStepUs);
        }
        if (giveUp && giveUp(user)) {
            return false;
        }
    }
}

void ClusterLink::armEmergencyCancel(const RouteGraph& graph) {
    g_emergencyArmed = 0;
    std::vector<Request> requests;
    graph.cancel(requests);
    Emergency& e = g_emergency;
    const size_t slots = sizeof(e.sizes) / sizeof(e.sizes[0]);
    size_t used = 0, n = 0;
    for (size_t i = 0; i < requests.size() && n < slots; ++i) {
        const Bytes bytes =
            encodeRequest(requests[i].method, requests[i].body, static_cast<uint16_t>(kEmergencySession + i));
        if (used + bytes.size() > sizeof(e.bytes)) {
            return;
        }
        memcpy(e.bytes + used, &bytes[0], bytes.size());
        used += bytes.size();
        e.sizes[n++] = bytes.size();
    }
    memset(&e.to, 0, sizeof(e.to));
    e.to.sin_family = AF_INET;
    e.to.sin_port = htons(port_);
    if (inet_pton(AF_INET, address_.c_str(), &e.to.sin_addr) != 1) {
        return;
    }
    e.count = n;
    g_emergencyArmed = 1;
}

// System calls only (socket, fcntl, connect, poll, getsockopt, setsockopt, send, recv, close), no allocation.
void ClusterLink::emergencyCancel() {
    if (!g_emergencyArmed) {
        return;
    }
    const Emergency& e = g_emergency;
    const int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return;
    }
    const int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    if (connect(fd, reinterpret_cast<const struct sockaddr*>(&e.to), sizeof(e.to)) < 0) {
        if (errno != EINPROGRESS) {
            ::close(fd);
            return;
        }
        struct pollfd p;
        p.fd = fd;
        p.events = POLLOUT;
        p.revents = 0;
        int err = 0;
        socklen_t size = sizeof(err);
        if (poll(&p, 1, kEmergencyTimeoutMs) <= 0 || getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &size) < 0 || err) {
            ::close(fd);
            return;
        }
    }
    fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
    setTimeouts(fd, kEmergencyTimeoutMs);
    size_t at = 0;
    for (size_t i = 0; i < e.count; ++i) {
        if (writeAll(fd, e.bytes + at, e.sizes[i])) {
            break;
        }
        at += e.sizes[i];
        if (!skipAnswer(fd)) {
            break;
        }
    }
    ::close(fd);
}

}  // namespace kombi
}  // namespace evo
