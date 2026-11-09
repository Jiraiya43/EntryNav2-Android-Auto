// Copyright (c) 2026 Jiraiya43
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
/* PC/QEMU fixture for the real BMW CLI parser. No compositor calls. */
#include <stdio.h>
#include <stdlib.h>
int ilm_init(void) {
    puts("FIXTURE init");
    return 0;
}
int ilm_destroy(void) {
    puts("FIXTURE destroy");
    return 0;
}
int ilm_commitChanges(void) {
    puts("FIXTURE commit");
    return 0;
}
int ilm_layerSetRenderOrder(unsigned layer, const unsigned* ids, unsigned count) {
    printf("FIXTURE order %x count=%u", layer, count);
    for (unsigned i = 0; i < count; ++i) {
        printf(" %x", ids[i]);
    }
    puts("");
    return 0;
}
int ilm_surfaceSetSourceRectangle(unsigned surface, unsigned x, unsigned y, unsigned w, unsigned h) {
    printf("FIXTURE source %x %u %u %u %u\n", surface, x, y, w, h);
    return 0;
}
int ilm_surfaceSetDestinationRectangle(unsigned surface, unsigned x, unsigned y, unsigned w, unsigned h) {
    printf("FIXTURE destination %x %u %u %u %u\n", surface, x, y, w, h);
    return 0;
}
