// Copyright (c) 2026 Jiraiya43
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Host test of platform/volume_pill.hpp: shape, transparency outside the pill, bar length, digits.

// With a folder as argument, also writes the pills as raw 8-bit RGBA files (pill-<value>.rgba, 360x60) to look at.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "volume_pill.hpp"

#define CHECK(x)                                                \
    do {                                                        \
        if (!(x)) {                                             \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); \
            exit(1);                                            \
        }                                                       \
    } while (0)

using evo::pill::kHeight;
using evo::pill::kWidth;

namespace {

unsigned alpha(uint32_t p) {
    return p >> 24;
}
unsigned blue(uint32_t p) {
    return p & 0xff;
}

// Rightmost column of the bar's blue fill on the middle row (end dot excluded: white there).
int fillEnd(const std::vector<uint32_t>& px) {
    int last = -1;
    for (int x = 0; x < kWidth; ++x) {
        const uint32_t p = px[(kHeight / 2) * kWidth + x];
        const unsigned r = (p >> 16) & 0xff;
        if (alpha(p) > 200 && blue(p) > r + 60) {
            last = x;
        }
    }
    return last;
}

void save(const std::vector<uint32_t>& px, const char* dir, unsigned value) {
    char path[512];
    snprintf(path, sizeof(path), "%s/pill-%u.rgba", dir, value);
    FILE* f = fopen(path, "wb");
    CHECK(f);
    for (size_t i = 0; i < px.size(); ++i) {
        const uint32_t p = px[i];
        const unsigned a = alpha(p);
        // Back from premultiplied, for an ordinary image viewer.
        unsigned char rgba[4] = {0, 0, 0, static_cast<unsigned char>(a)};
        for (int k = 0; k < 3 && a; ++k) {
            const unsigned v = (p >> (16 - 8 * k)) & 0xff;
            rgba[k] = static_cast<unsigned char>(v * 255 / a > 255 ? 255 : v * 255 / a);
        }
        fwrite(rgba, 1, 4, f);
    }
    fclose(f);
}

}  // namespace

int main(int argc, char** argv) {
    std::vector<uint32_t> px(kWidth * kHeight, 0xdeadbeef);
    const unsigned values[] = {0, 1, 10, 17, 20, 39, 40};
    int previous = -1;
    for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); ++i) {
        evo::pill::draw(&px[0], kWidth, values[i], 0, 40);
        // Rounded ends: the corners stay fully transparent, the body is nearly opaque.
        CHECK(px[0] == 0 && px[kWidth - 1] == 0 && px[(kHeight - 1) * kWidth] == 0);
        CHECK(alpha(px[(kHeight / 2) * kWidth + kWidth / 2 + 40]) >= 200);
        CHECK(alpha(px[(kHeight / 2) * kWidth + 2]) >= 200);  // the middle of the left end
        // Premultiplied: no colour channel above alpha.
        for (size_t k = 0; k < px.size(); ++k) {
            const uint32_t p = px[k];
            CHECK(((p >> 16) & 0xff) <= alpha(p) && ((p >> 8) & 0xff) <= alpha(p) && blue(p) <= alpha(p));
        }
        const int end = fillEnd(px);
        if (values[i] == 0) {
            CHECK(end < 0);            // empty bar
        } else if (values[i] >= 10) {  // below that, the end dot covers the whole fill
            CHECK(end > previous);     // a higher value gives a longer bar
            previous = end;
        }
        if (argc > 1) {
            save(px, argv[1], values[i]);
        }
    }
    // Segments: 8 lights all seven, 1 only two.
    CHECK(evo::pill::digitSegments(8) == 0x7f && evo::pill::digitSegments(1) == 0x06 &&
          evo::pill::digitSegments(10) == 0);
    // Out of range values are drawn at the ends, never past them.
    evo::pill::draw(&px[0], kWidth, 99, 0, 40);
    const int full = fillEnd(px);
    evo::pill::draw(&px[0], kWidth, 40, 0, 40);
    CHECK(fillEnd(px) == full);
    printf("PASS volume pill (%dx%d, transparent corners, bar follows the level)\n", kWidth, kHeight);
    return 0;
}
