// Copyright (c) 2026 Jiraiya43
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// A phone that vanishes without closing the socket must end the session within seconds; a crash logs before its hook.

// The phone vanishes when its Wi-Fi is switched off. The car GPS wakes the session thread about 10 times a second, so
// a check that counted select() timeouts would never fire and the last frame would stay on screen.
#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include <string>

#include "aa_session.hpp"
#include "crash_handler.hpp"

static double nowS() {
    timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec / 1e9;
}
static volatile int g_ended = 0;
static void ended(void*) {
    g_ended = 1;
}

// Runs a silent phone, with or without wake-ups, and returns how long the session lasted (or -1 after max_s).
static double silentPhone(bool wakes, double max_s) {
    int peers[2];
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, peers) == 0);
    aa::AASession session;
    g_ended = 0;
    session.setDisconnectCallback(ended, 0);
    const double t0 = nowS();
    assert(session.start(peers[0]));
    double lasted = -1;
    while (nowS() - t0 < max_s) {
        if (wakes) {
            session.postVideoFocus(true);  // same wake pipe as postCarLocation, every main loop tick
        }
        if (!session.isActive()) {
            lasted = nowS() - t0;
            break;
        }
        usleep(100000);
    }
    const int ended_now = g_ended;
    session.stop();
    close(peers[1]);
    if (lasted >= 0) {
        assert(ended_now);  // the disconnect callback ran: the picture leaves, the menu comes back
    }
    return lasted;
}

// Written on fd 1 from the signal handler, as the service's own crash line is.
static void writeLine(const char* line) {
    ssize_t w = write(1, line, strlen(line));
    (void)w;
}
static void hookLine() {
    writeLine("hook\n");
}
static void previousLine(int) {
    writeLine("previous\n");
}

// A fatal signal in a child: the crash line first, then the hook (the cluster arrow cancel in the service), then the
// handler that was installed before, each once, and the child still dies of that signal.
static void crashOrder() {
    int out[2];
    assert(pipe(out) == 0);
    const pid_t child = fork();
    assert(child >= 0);
    if (child == 0) {
        const struct rlimit noCore = {0, 0};
        setrlimit(RLIMIT_CORE, &noCore);  // the test's own crash leaves no core file behind
        dup2(out[1], 1);
        close(out[0]);
        signal(SIGABRT, previousLine);
        aa::installProcessGuards();
        aa::installProcessGuards();  // a second call must not chain the handler to itself
        aa::setFatalHook(hookLine);
        raise(SIGABRT);
        _exit(0);
    }
    close(out[1]);
    std::string text;
    char buf[256];
    ssize_t n;
    while ((n = read(out[0], buf, sizeof(buf))) > 0) {
        text.append(buf, static_cast<size_t>(n));
    }
    close(out[0]);
    int status = 0;
    assert(waitpid(child, &status, 0) == child);
    assert(WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT);
    const size_t crash = text.find("[CRASH] fatal signal 6 (SIGABRT)");
    const size_t hook = text.find("hook\n");
    const size_t previous = text.find("previous\n");
    assert(crash != std::string::npos && hook != std::string::npos && previous != std::string::npos);
    assert(crash < hook && hook < previous);
    assert(text.find("hook\n", hook + 1) == std::string::npos);
    assert(text.find("previous\n", previous + 1) == std::string::npos);
}

int main() {
    setvbuf(stdout, 0, _IONBF, 0);
    crashOrder();
    puts("PASS crash : ligne de crash, puis le crochet, puis l'ancien gestionnaire");
    const double quiet = silentPhone(false, 10);
    printf("telephone muet, sans reveil : fin apres %.1f s\n", quiet);
    assert(quiet > 4 && quiet < 7);  // 5 s of silence (kRxSilenceMs, core/aa_session.cpp)
    const double woken = silentPhone(true, 10);
    printf("telephone muet, reveils 10 fois par seconde : fin apres %.1f s\n", woken);
    assert(woken > 4 && woken < 7);
    puts("PASS session : un telephone disparu met fin a la session, meme reveillee par le GPS");
    return 0;
}
