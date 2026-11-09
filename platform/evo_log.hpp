// Copyright (c) 2026 Jiraiya43
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Monotonic clocks and the service's log line writer (ELOG).
#ifndef ENTRY_AAD_PLATFORM_EVO_LOG_HPP
#define ENTRY_AAD_PLATFORM_EVO_LOG_HPP

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

namespace evo {

inline double upSec() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<double>(ts.tv_sec) + static_cast<double>(ts.tv_nsec) / 1e9;
}

inline uint32_t monoMs() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<uint32_t>(ts.tv_sec) * 1000u + static_cast<uint32_t>(ts.tv_nsec / 1000000);
}

}  // namespace evo

namespace evo {
// One whole line per call, written at once, so lines from different threads never mix. Every line starts with the
// uptime, the clock used by the service's other logs.
inline void __attribute__((format(printf, 2, 3))) logLine(const char* tag, const char* fmt, ...) {
    char line[1024];
    int n = snprintf(line, sizeof(line), "[%9.3f] [%s] ", upSec(), tag);
    if (n < 0 || n >= static_cast<int>(sizeof(line))) {
        return;
    }
    va_list ap;
    va_start(ap, fmt);
    const int m = vsnprintf(line + n, sizeof(line) - n - 1, fmt, ap);
    va_end(ap);
    n = m < 0 ? n : n + m >= static_cast<int>(sizeof(line)) - 1 ? static_cast<int>(sizeof(line)) - 2 : n + m;
    line[n++] = '\n';
    line[n] = 0;
    fputs(line, stdout);
    fflush(stdout);
}
}  // namespace evo

#define ELOG(tag, ...) evo::logLine(tag, __VA_ARGS__)

#endif
