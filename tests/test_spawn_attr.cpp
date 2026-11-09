// Copyright (c) 2026 Jiraiya43
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Reproduces the failure seen on the car with sigfillset() attributes (Start -> supervisor "exit 127", empty log)
// and checks the fix, with the HU's own glibc 2.15 under QEMU (qemu-arm -L rootfs). A file action (open of a marker)
// runs in the child only AFTER the signal-default step, so "marker created" proves the child got past it, independent
// of the exec target.
//   test-spawn-attr-arm --hu-glibc   sigfillset() attributes must die with 127 before the marker, fixed ones must
//                                    create it
//   test-spawn-attr                  (host) fixed attributes must create the marker
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "../hmiapp/spawn_attr.hpp"

extern char** environ;

static bool runCase(bool fixed, const char* marker, int* status) {
    unlink(marker);
    posix_spawnattr_t at;
    if (fixed) {
        if (initServiceSpawnAttr(&at) != 0) {
            return false;
        }
    } else {  // the attributes launchDaemon() first used: every signal back to default
        posix_spawnattr_init(&at);
        sigset_t none, all;
        sigemptyset(&none);
        sigfillset(&all);
        posix_spawnattr_setsigmask(&at, &none);
        posix_spawnattr_setsigdefault(&at, &all);
        posix_spawnattr_setpgroup(&at, 0);
        posix_spawnattr_setflags(&at, POSIX_SPAWN_SETPGROUP | POSIX_SPAWN_SETSIGMASK | POSIX_SPAWN_SETSIGDEF);
    }
    posix_spawn_file_actions_t fa;
    posix_spawn_file_actions_init(&fa);
    posix_spawn_file_actions_addopen(&fa, 9, marker, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    char* argv[] = {const_cast<char*>("/bin/true"), NULL};
    pid_t pid = -1;
    int rc = posix_spawn(&pid, "/bin/true", &fa, &at, argv, environ);
    posix_spawnattr_destroy(&at);
    posix_spawn_file_actions_destroy(&fa);
    *status = -1;
    if (rc == 0) {
        while (waitpid(pid, status, 0) < 0 && errno == EINTR) {
        }
    }
    struct stat st;
    bool reached = stat(marker, &st) == 0;
    unlink(marker);
    return reached;
}

int main(int argc, char** argv) {
    const bool hu = argc > 1 && !strcmp(argv[1], "--hu-glibc");
    char marker[64];
    snprintf(marker, sizeof(marker), "/tmp/entry-spawn-marker-%d", (int)getpid());
    signal(SIGPIPE, SIG_IGN);  // like HmiMain3D: an ignored signal the child must get back
    int st1 = 0, st2 = 0;
    bool oldReached = runCase(false, marker, &st1);
    bool newReached = runCase(true, marker, &st2);
    printf("sigfillset attributes: child %s, status %s %d\n", oldReached ? "went on" : "STOPPED",
           WIFEXITED(st1) ? "exit" : "other", WIFEXITED(st1) ? WEXITSTATUS(st1) : st1);
    printf("fixed attributes:      child %s\n", newReached ? "went on" : "STOPPED");
    bool ok = newReached && (!hu || (!oldReached && WIFEXITED(st1) && WEXITSTATUS(st1) == 127));
    printf(ok ? "PASS service spawn: %s\n" : "FAIL service spawn: %s\n",
           hu ? "the car's failure reproduced with the HU's glibc 2.15 (exit 127 before exec), fix confirmed"
              : "fixed attributes work (host glibc)");
    return ok ? 0 : 1;
}
