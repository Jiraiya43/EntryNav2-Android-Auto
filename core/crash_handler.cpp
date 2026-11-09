// Copyright (c) 2026 Jiraiya43
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Process guards: line-buffered stdout, SIGPIPE ignored, and a crash line that says where the session was.
#include "crash_handler.hpp"

#include <unistd.h>

#include <csignal>
#include <cstdio>

namespace aa {

SessionTrail g_trail = {kTrailIdle, -1, -1};

namespace {

struct FatalSignal {
    int number;
    const char* name;
};
const FatalSignal kFatal[] = {
    {SIGSEGV, "SIGSEGV"}, {SIGBUS, "SIGBUS"}, {SIGILL, "SIGILL"}, {SIGFPE, "SIGFPE"}, {SIGABRT, "SIGABRT"}};
const size_t kFatalCount = sizeof(kFatal) / sizeof(kFatal[0]);

void (*g_chained[kFatalCount])(int);  // each signal's handler before ours (SIG_DFL when none)
FatalHook volatile g_hook = nullptr;
volatile sig_atomic_t g_hookDone = 0;

// A line built without the C library: write(2) is all a signal handler may use.
class CrashLine {
public:
    CrashLine() : size_(0) {}
    CrashLine& text(const char* s) {
        while (*s && size_ < kTextRoom) {
            buffer_[size_++] = *s++;
        }
        return *this;
    }
    CrashLine& decimal(long value) {
        char digits[24];
        int count = 0;
        unsigned long rest = value < 0 ? 0ul - static_cast<unsigned long>(value) : static_cast<unsigned long>(value);
        do {
            digits[count++] = static_cast<char>('0' + rest % 10);
            rest /= 10;
        } while (rest && count < 22);
        if (value < 0) {
            buffer_[size_++] = '-';
        }
        while (count) {
            buffer_[size_++] = digits[--count];
        }
        return *this;
    }
    // Upper case, no leading zero; "-1" for any negative value (no message yet).
    CrashLine& hex(int value) {
        if (value < 0) {
            return text("-1");
        }
        static const char kDigits[] = "0123456789ABCDEF";
        bool started = false;
        for (int shift = 28; shift >= 0; shift -= 4) {
            const int digit = (value >> shift) & 0xF;
            if (digit || started || shift == 0) {
                buffer_[size_++] = kDigits[digit];
                started = true;
            }
        }
        return *this;
    }
    void writeTo(int fd) {
        buffer_[size_++] = '\n';
        const ssize_t written = write(fd, buffer_, size_);
        (void)written;
    }

private:
    static const size_t kTextRoom = 200;  // the numbers fit in what is left
    char buffer_[256];
    size_t size_;
};

void onFatalSignal(int number) {
    const char* name = "";
    for (size_t i = 0; i < kFatalCount; ++i) {
        if (kFatal[i].number == number) {
            name = kFatal[i].name;
        }
    }
    CrashLine line;
    line.text("\n[CRASH] fatal signal ").decimal(number).text(" (").text(name).text(")");
    line.text(" stage=").decimal(g_trail.stage).text(" last_channel=").decimal(g_trail.channel);
    line.text(" last_msg=0x").hex(g_trail.message).writeTo(1);
    // The line is out first: the hook may wait on the network. A fatal signal raised by the hook runs it no more.
    const FatalHook hook = g_hook;
    if (hook && !g_hookDone) {
        g_hookDone = 1;
        hook();
    }
    for (size_t i = 0; i < kFatalCount; ++i) {
        void (*const chained)(int) = g_chained[i];
        if (kFatal[i].number == number && chained != SIG_DFL && chained != SIG_IGN && chained != SIG_ERR) {
            chained(number);
        }
    }
    signal(number, SIG_DFL);  // the system ends the process, a core dump included, as usual
    raise(number);
}

}  // namespace

void installProcessGuards() {
    setvbuf(stdout, nullptr, _IOLBF, 0);
    signal(SIGPIPE, SIG_IGN);
    for (size_t i = 0; i < kFatalCount; ++i) {
        void (*const before)(int) = signal(kFatal[i].number, onFatalSignal);
        if (before != onFatalSignal) {  // called twice: never chained to itself
            g_chained[i] = before;
        }
    }
}

void setFatalHook(FatalHook hook) {
    g_hook = hook;
}

}  // namespace aa
