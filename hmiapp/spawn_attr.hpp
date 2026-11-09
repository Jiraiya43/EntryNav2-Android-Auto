// Copyright (c) 2026 Jiraiya43
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// posix_spawn attributes used by libEntryAad.so to start the Android Auto service from inside HmiMain3D:
// own process group, empty signal mask (HmiMain3D's threads block signals), default disposition for the classic
// catchable signals HmiMain3D may ignore (SIGPIPE, SIGHUP...; caught handlers are reset by exec anyway).
// NOT sigfillset(): the HU's glibc 2.15 posix_spawn calls sigaction() in the child for every signal of the "default"
// set and _exit(127)s when one is refused (SIGKILL, SIGSTOP...), before running any file action or exec. On the car
// this shows as "service supervisor ... ended (exit 127)" 20 ms after each Start, with an empty service log.
// tests/test_spawn_attr.cpp.
#ifndef ENTRY_AAD_HMIAPP_SPAWN_ATTR_HPP
#define ENTRY_AAD_HMIAPP_SPAWN_ATTR_HPP
#include <signal.h>
#include <spawn.h>

inline int initServiceSpawnAttr(posix_spawnattr_t* at) {
    int rc = posix_spawnattr_init(at);
    if (rc) {
        return rc;
    }
    sigset_t none, def;
    sigemptyset(&none);
    sigemptyset(&def);
    for (int s = 1; s < 32; ++s) {
        if (s != SIGKILL && s != SIGSTOP) {
            sigaddset(&def, s);
        }
    }
    rc = posix_spawnattr_setsigmask(at, &none);
    if (!rc) {
        rc = posix_spawnattr_setsigdefault(at, &def);
    }
    if (!rc) {
        rc = posix_spawnattr_setpgroup(at, 0);
    }
    if (!rc) {
        rc = posix_spawnattr_setflags(at, POSIX_SPAWN_SETPGROUP | POSIX_SPAWN_SETSIGMASK | POSIX_SPAWN_SETSIGDEF);
    }
    if (rc) {
        posix_spawnattr_destroy(at);
    }
    return rc;
}

#endif  // ENTRY_AAD_HMIAPP_SPAWN_ATTR_HPP
