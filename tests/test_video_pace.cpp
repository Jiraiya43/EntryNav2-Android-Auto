// Copyright (c) 2026 Jiraiya43
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// The [Video] line's totals: bytes, gaps between frames, slowest hand-off, lost frames, reset on take and restart.
#include <stdio.h>
#include <stdlib.h>

#include "../platform/video_pace.hpp"

#define CHECK(x)                                                \
    do {                                                        \
        if (!(x)) {                                             \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); \
            exit(1);                                            \
        }                                                       \
    } while (0)

int main() {
    evo::VideoPace pace;
    pace.frame(1000, 2048, 3, false);  // first frame: no gap yet
    pace.frame(1040, 1024, 25, true);
    pace.frame(1300, 512, 1, false);
    evo::VideoPaceTotals t = pace.take();
    CHECK(t.frames == 3 && t.kbytes == 4 && t.gapMaxMs == 260 && t.feedMaxMs == 25 && t.lost == 1);
    t = pace.take();
    CHECK(t.frames == 0 && t.kbytes == 0 && t.gapMaxMs == 0 && t.feedMaxMs == 0 && t.lost == 0);
    pace.frame(5000, 100, 0, false);  // a gap across a take, within one session
    CHECK(pace.take().gapMaxMs == 3700);
    pace.frame(6000, 100, 7, true);
    pace.restart();  // a new session: nothing left over, and its first frame is not a gap
    pace.frame(90000, 100, 0, false);
    t = pace.take();
    CHECK(t.frames == 1 && t.gapMaxMs == 0 && t.feedMaxMs == 0 && t.lost == 0);
    printf("PASS video pace: bytes, gaps, slowest hand-off, lost frames, reset on take and restart\n");
    return 0;
}
