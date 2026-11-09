// Copyright (c) 2026 Jiraiya43
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Process guards, and the trail the session leaves for the crash line.
#ifndef ENTRY_AAD_CORE_CRASH_HANDLER_HPP
#define ENTRY_AAD_CORE_CRASH_HANDLER_HPP

namespace aa {

// Where the session thread was: plain stores as it goes, printed by the crash line.
enum TrailStage { kTrailIdle = 0, kTrailFrame = 1, kTrailDecrypt = 2, kTrailHandler = 3, kTrailVideo = 4 };
struct SessionTrail {
    volatile int stage;    // TrailStage
    volatile int channel;  // last channel received, -1 before the first
    volatile int message;  // last message id handled, -1 before the first
};
extern SessionTrail g_trail;

// Early in main(), before any thread: stdout line-buffered (the end of a log explains a crash), SIGPIPE ignored (a
// send() to a phone that just left would kill the service silently), and a crash line on fatal signals. A handler
// already installed for one of them runs after ours, before the signal is raised again.
void installProcessGuards();

// Runs once from the crash handler, after the crash line and before the signal is raised again: async-signal-safe
// only. nullptr removes it.
using FatalHook = void (*)();
void setFatalHook(FatalHook hook);

}  // namespace aa

#endif  // ENTRY_AAD_CORE_CRASH_HANDLER_HPP
