// Copyright (c) 2026 Jiraiya43
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// A fake KOMBI on 127.0.0.1 for the cluster guidance tests: records every request and answers it, as scripted.
#ifndef ENTRY_AAD_TESTS_KOMBI_FAKE_KOMBI_HPP
#define ENTRY_AAD_TESTS_KOMBI_FAKE_KOMBI_HPP

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <stdint.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <vector>

namespace kombitest {

inline double secondsNow() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<double>(ts.tv_sec) + static_cast<double>(ts.tv_nsec) / 1e9;
}

// One request as the fake received it.
struct Seen {
    int connection;  // 1 for the first connection accepted, and so on
    uint16_t method;
    uint16_t client;
    uint16_t session;
    std::vector<uint8_t> body;
};

class FakeKombi {
public:
    FakeKombi() : listen_(-1), port_(0), quit_(false), cookieMs_(0), refuseUntil_(0), connections_(0), active_(0) {
        pthread_mutex_init(&mu_, nullptr);
        listen_ = socket(AF_INET, SOCK_STREAM, 0);
        const int one = 1;
        setsockopt(listen_, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
        struct sockaddr_in a;
        memset(&a, 0, sizeof(a));
        a.sin_family = AF_INET;
        a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        socklen_t size = sizeof(a);
        if (bind(listen_, reinterpret_cast<struct sockaddr*>(&a), sizeof(a)) != 0 || listen(listen_, 16) != 0 ||
            getsockname(listen_, reinterpret_cast<struct sockaddr*>(&a), &size) != 0) {
            fprintf(stderr, "FAIL fake KOMBI: cannot listen\n");
            exit(1);
        }
        port_ = ntohs(a.sin_port);
        pthread_create(&acceptor_, nullptr, &FakeKombi::acceptMain, this);
    }

    ~FakeKombi() {
        lock();
        quit_ = true;
        unlock();
        pthread_join(acceptor_, nullptr);
        close(listen_);
        hangUpAll();
        for (int i = 0; i < 500 && active() > 0; ++i) {
            usleep(10000);
        }
        pthread_mutex_destroy(&mu_);
    }

    uint16_t port() const { return port_; }

    // The n-th request (from 1, over every connection) is answered with return code 1.
    void refuse(size_t n) {
        lock();
        refused_.insert(n);
        unlock();
    }
    // Every request is refused until this time (secondsNow()).
    void refuseUntil(double t) {
        lock();
        refuseUntil_ = t;
        unlock();
    }
    // The n-th request gets no answer: its connection is closed instead.
    void hangUpAt(size_t n) {
        lock();
        hangUps_.insert(n);
        unlock();
    }
    // A magic cookie every ms on each new connection.
    void sendCookies(int ms) {
        lock();
        cookieMs_ = ms;
        unlock();
    }
    // Closes every open connection, as the KOMBI does on a link blip.
    void hangUpAll() {
        lock();
        for (std::set<int>::iterator it = open_.begin(); it != open_.end(); ++it) {
            shutdown(*it, SHUT_RDWR);
        }
        unlock();
    }

    size_t count() {
        lock();
        const size_t n = seen_.size();
        unlock();
        return n;
    }
    std::vector<Seen> seen() {
        lock();
        const std::vector<Seen> s = seen_;
        unlock();
        return s;
    }
    bool waitFor(size_t n, double seconds = 10.0) {
        const double end = secondsNow() + seconds;
        while (count() < n && secondsNow() < end) {
            usleep(5000);
        }
        return count() >= n;
    }

private:
    struct Connection {
        FakeKombi* fake;
        int fd;
        int id;
        int cookieMs;
        int users;  // threads still using it, under mu_: the last one closes it
    };

    // The thread's last use of the connection.
    static void release(Connection* c) {
        FakeKombi* f = c->fake;
        f->lock();
        const bool last = --c->users == 0;
        if (last) {
            f->open_.erase(c->fd);
        }
        --f->active_;
        f->unlock();
        if (last) {
            close(c->fd);
            delete c;
        }
    }

    void lock() { pthread_mutex_lock(&mu_); }
    void unlock() { pthread_mutex_unlock(&mu_); }
    bool quitting() {
        lock();
        const bool q = quit_;
        unlock();
        return q;
    }
    int active() {
        lock();
        const int n = active_;
        unlock();
        return n;
    }

    static void* acceptMain(void* p) {
        FakeKombi* f = static_cast<FakeKombi*>(p);
        while (!f->quitting()) {
            struct pollfd pf;
            pf.fd = f->listen_;
            pf.events = POLLIN;
            pf.revents = 0;
            if (poll(&pf, 1, 20) <= 0) {
                continue;
            }
            const int fd = accept(f->listen_, nullptr, nullptr);
            if (fd < 0) {
                continue;
            }
            Connection* c = new Connection;
            c->fake = f;
            c->fd = fd;
            f->lock();
            c->id = ++f->connections_;
            c->cookieMs = f->cookieMs_;
            c->users = c->cookieMs ? 2 : 1;
            f->open_.insert(fd);
            f->active_ += c->users;
            f->unlock();
            pthread_t t;
            if (c->cookieMs) {
                pthread_create(&t, nullptr, &FakeKombi::cookieMain, c);
                pthread_detach(t);
            }
            pthread_create(&t, nullptr, &FakeKombi::connectionMain, c);
            pthread_detach(t);
        }
        return nullptr;
    }

    static bool readAll(int fd, uint8_t* p, size_t n) {
        while (n) {
            const ssize_t got = recv(fd, p, n, 0);
            if (got <= 0) {
                return false;
            }
            p += got;
            n -= static_cast<size_t>(got);
        }
        return true;
    }

    static void* connectionMain(void* p) {
        Connection* c = static_cast<Connection*>(p);
        FakeKombi* f = c->fake;
        for (;;) {
            uint8_t h[16];
            if (!readAll(c->fd, h, sizeof(h))) {
                break;
            }
            const uint32_t length = static_cast<uint32_t>(h[4]) << 24 | h[5] << 16 | h[6] << 8 | h[7];
            if (length < 8 || length > 65536) {
                break;
            }
            Seen s;
            s.connection = c->id;
            s.method = static_cast<uint16_t>(h[2] << 8 | h[3]);
            s.client = static_cast<uint16_t>(h[8] << 8 | h[9]);
            s.session = static_cast<uint16_t>(h[10] << 8 | h[11]);
            s.body.resize(length - 8);
            if (length > 8 && !readAll(c->fd, &s.body[0], s.body.size())) {
                break;
            }
            f->lock();
            f->seen_.push_back(s);
            const size_t n = f->seen_.size();
            const bool refused = f->refused_.count(n) || secondsNow() < f->refuseUntil_;
            const bool hangUp = f->hangUps_.count(n) != 0;
            f->unlock();
            if (hangUp) {
                break;
            }
            h[4] = h[5] = h[6] = 0;
            h[7] = 8;
            h[14] = 0x80;
            h[15] = refused ? 1 : 0;
            if (send(c->fd, h, sizeof(h), MSG_NOSIGNAL) != static_cast<ssize_t>(sizeof(h))) {
                break;
            }
        }
        shutdown(c->fd, SHUT_RDWR);
        release(c);
        return nullptr;
    }

    // Cookies until the connection or the fake goes; they may cross an answer, as on the car.
    static void* cookieMain(void* p) {
        Connection* c = static_cast<Connection*>(p);
        FakeKombi* f = c->fake;
        const uint8_t cookie[16] = {0xFF, 0xFF, 0x80, 0x00, 0, 0, 0, 8, 0xDE, 0xAD, 0xBE, 0xEF, 1, 1, 2, 0};
        while (!f->quitting()) {
            usleep(static_cast<useconds_t>(c->cookieMs) * 1000);
            if (send(c->fd, cookie, sizeof(cookie), MSG_NOSIGNAL) != static_cast<ssize_t>(sizeof(cookie))) {
                break;
            }
        }
        release(c);
        return nullptr;
    }

    int listen_;
    uint16_t port_;
    pthread_t acceptor_;
    pthread_mutex_t mu_;
    // Under mu_
    bool quit_;
    int cookieMs_;
    double refuseUntil_;
    int connections_;
    int active_;
    std::set<int> open_;
    std::set<size_t> refused_, hangUps_;
    std::vector<Seen> seen_;
};

}  // namespace kombitest

#endif
