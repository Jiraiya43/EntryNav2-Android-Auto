// Copyright (c) 2026 Jiraiya43
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// Draws the volume pill that platform/evo_pill.cpp shows over Android Auto; pure drawing, no I/O.

// Draws into a premultiplied ARGB8888 buffer (wl_shm format 0, 0xAARRGGBB words), anti-aliased with signed distances.
// No font: digits are seven rounded segments. Tested by tests/test_volume_pill.cpp.
#ifndef ENTRY_AAD_PLATFORM_VOLUME_PILL_HPP
#define ENTRY_AAD_PLATFORM_VOLUME_PILL_HPP

#include <math.h>
#include <stdint.h>

namespace evo {
namespace pill {

const int kWidth = 360, kHeight = 60;  // at 1280x480
const int kBottomMargin = 28;          // above the bottom edge of the screen

struct Rgba {
    float r, g, b, a;  // 0..1, not premultiplied
};

inline float clamp01(float v) {
    return v < 0.f ? 0.f : v > 1.f ? 1.f : v;
}

// Signed distance from (px, py) to a box of half-size (hx, hy) centred on (cx, cy), corners rounded by r.
inline float roundBoxDistance(float px, float py, float cx, float cy, float hx, float hy, float r) {
    const float qx = fabsf(px - cx) - (hx - r), qy = fabsf(py - cy) - (hy - r);
    const float ox = qx > 0.f ? qx : 0.f, oy = qy > 0.f ? qy : 0.f;
    const float inside = qx > qy ? qx : qy;
    return sqrtf(ox * ox + oy * oy) + (inside < 0.f ? inside : 0.f) - r;
}

class Canvas {
public:
    Canvas(uint32_t* px, int w, int h, int stridePx) : px_(px), w_(w), h_(h), stride_(stridePx) {}

    void clear() {
        for (int y = 0; y < h_; ++y) {
            for (int x = 0; x < w_; ++x) {
                px_[y * stride_ + x] = 0;
            }
        }
    }
    // Rounded box from (x0, y0) to (x1, y1). ring > 0 draws only a border of that width.
    void roundBox(float x0, float y0, float x1, float y1, float r, const Rgba& c, float ring = 0.f) {
        const float cx = (x0 + x1) / 2, cy = (y0 + y1) / 2, hx = (x1 - x0) / 2, hy = (y1 - y0) / 2;
        for (int y = clampY(y0 - 1); y <= clampY(y1 + 1); ++y) {
            for (int x = clampX(x0 - 1); x <= clampX(x1 + 1); ++x) {
                float d = roundBoxDistance(x + .5f, y + .5f, cx, cy, hx, hy, r);
                if (ring > 0.f) {
                    d = fabsf(d + ring / 2) - ring / 2;
                }
                blend(x, y, c, clamp01(.5f - d));
            }
        }
    }
    void disc(float cx, float cy, float r, const Rgba& c) {
        for (int y = clampY(cy - r - 1); y <= clampY(cy + r + 1); ++y) {
            for (int x = clampX(cx - r - 1); x <= clampX(cx + r + 1); ++x) {
                const float dx = x + .5f - cx, dy = y + .5f - cy;
                blend(x, y, c, clamp01(.5f - (sqrtf(dx * dx + dy * dy) - r)));
            }
        }
    }
    // Arc of radius rad and width t, on the right of (cx, cy), within +/- 50 degrees: a sound wave.
    void wave(float cx, float cy, float rad, float t, const Rgba& c) {
        for (int y = clampY(cy - rad - t); y <= clampY(cy + rad + t); ++y) {
            for (int x = clampX(cx); x <= clampX(cx + rad + t); ++x) {
                const float dx = x + .5f - cx, dy = y + .5f - cy;
                const float ring = fabsf(sqrtf(dx * dx + dy * dy) - rad) - t / 2;
                const float side = dx * .766f - fabsf(dy) * .643f;  // inside the 50-degree cone
                blend(x, y, c, clamp01(.5f - ring) * clamp01(side + .5f));
            }
        }
    }
    // Convex polygon, corners in either order.
    void polygon(const float* xs, const float* ys, int n, const Rgba& c) {
        float x0 = xs[0], x1 = xs[0], y0 = ys[0], y1 = ys[0], area = 0.f;
        for (int i = 0; i < n; ++i) {
            const int j = (i + 1) % n;
            area += xs[i] * ys[j] - xs[j] * ys[i];
            x0 = fminf(x0, xs[i]);
            x1 = fmaxf(x1, xs[i]);
            y0 = fminf(y0, ys[i]);
            y1 = fmaxf(y1, ys[i]);
        }
        const float side = area < 0.f ? -1.f : 1.f;  // inside = same side of every edge
        for (int y = clampY(y0 - 1); y <= clampY(y1 + 1); ++y) {
            for (int x = clampX(x0 - 1); x <= clampX(x1 + 1); ++x) {
                float d = -1e9f;
                for (int i = 0; i < n; ++i) {
                    const int j = (i + 1) % n;
                    const float ex = xs[j] - xs[i], ey = ys[j] - ys[i];
                    const float len = sqrtf(ex * ex + ey * ey);
                    // Distance to the edge, positive inside.
                    const float dist =
                        side * ((x + .5f - xs[i]) * -ey + (y + .5f - ys[i]) * ex) / (len > 0.f ? len : 1.f);
                    d = fmaxf(d, -dist);
                }
                blend(x, y, c, clamp01(.5f - d));
            }
        }
    }
    uint32_t at(int x, int y) const { return px_[y * stride_ + x]; }

private:
    int clampX(float v) const { return v < 0 ? 0 : v > w_ - 1 ? w_ - 1 : static_cast<int>(v); }
    int clampY(float v) const { return v < 0 ? 0 : v > h_ - 1 ? h_ - 1 : static_cast<int>(v); }
    void blend(int x, int y, const Rgba& c, float cov) {
        if (cov <= 0.f) {
            return;
        }
        const float a = c.a * cov;
        uint32_t& d = px_[y * stride_ + x];
        const float k = 1.f - a;
        const float da = (d >> 24) / 255.f, dr = ((d >> 16) & 0xff) / 255.f, dg = ((d >> 8) & 0xff) / 255.f,
                    db = (d & 0xff) / 255.f;
        const float oa = a + da * k, orr = c.r * a + dr * k, og = c.g * a + dg * k, ob = c.b * a + db * k;
        d = (static_cast<uint32_t>(oa * 255.f + .5f) << 24) | (static_cast<uint32_t>(orr * 255.f + .5f) << 16) |
            (static_cast<uint32_t>(og * 255.f + .5f) << 8) | static_cast<uint32_t>(ob * 255.f + .5f);
    }
    uint32_t* px_;
    int w_, h_, stride_;
};

// Seven segments a..g for 0..9 (bit 0 = a, top; then b, c clockwise; d bottom; e, f; g middle).
inline unsigned digitSegments(unsigned d) {
    static const unsigned char kSegments[10] = {0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07, 0x7f, 0x6f};
    return d < 10 ? kSegments[d] : 0;
}

// One digit with its top-left corner at (x, y), w wide and h high, strokes t thick.
inline void digit(Canvas& c, float x, float y, float w, float h, float t, unsigned d, const Rgba& col) {
    const unsigned s = digitSegments(d);
    const float mid = y + h / 2, r = t / 2, g = t * .12f;  // g: gap between segments
    const float hx0 = x + r + g, hx1 = x + w - r - g;
    if (s & 0x01) {
        c.roundBox(hx0, y, hx1, y + t, r, col);  // a
    }
    if (s & 0x02) {
        c.roundBox(x + w - t, y + r + g, x + w, mid - g, r, col);  // b
    }
    if (s & 0x04) {
        c.roundBox(x + w - t, mid + g, x + w, y + h - r - g, r, col);  // c
    }
    if (s & 0x08) {
        c.roundBox(hx0, y + h - t, hx1, y + h, r, col);  // d
    }
    if (s & 0x10) {
        c.roundBox(x, mid + g, x + t, y + h - r - g, r, col);  // e
    }
    if (s & 0x20) {
        c.roundBox(x, y + r + g, x + t, mid - g, r, col);  // f
    }
    if (s & 0x40) {
        c.roundBox(hx0, mid - t / 2, hx1, mid + t / 2, r, col);  // g
    }
}

// The whole pill for value in [min, max] into a kWidth x kHeight area of px.
inline void draw(uint32_t* px, int stridePx, unsigned value, unsigned min, unsigned max) {
    Canvas c(px, kWidth, kHeight, stridePx);
    c.clear();
    const float W = kWidth, H = kHeight, R = H / 2;
    const Rgba body = {14 / 255.f, 17 / 255.f, 22 / 255.f, .88f}, edge = {1.f, 1.f, 1.f, .10f};
    const Rgba white = {1.f, 1.f, 1.f, .95f}, track = {1.f, 1.f, 1.f, .16f}, fill = {.27f, .58f, 1.f, 1.f};
    c.roundBox(0, 0, W, H, R, body);
    c.roundBox(0, 0, W, H, R, edge, 1.f);
    const float frac = max > min ? (value > max   ? 1.f
                                    : value < min ? 0.f
                                                  : float(value - min) / float(max - min))
                                 : 0.f;

    // Speaker: box and cone, then one or two waves (a cross when muted).
    const float sx = 26, sy = H / 2;
    c.roundBox(sx, sy - 6, sx + 7, sy + 6, 1.5f, white);
    const float cx[4] = {sx + 6, sx + 16, sx + 16, sx + 6}, cy[4] = {sy - 6, sy - 13, sy + 13, sy + 6};
    c.polygon(cx, cy, 4, white);
    if (value <= min) {
        const float mx = sx + 27;
        const float ax[4] = {mx - 6, mx - 4, mx + 6, mx + 4}, ay[4] = {sy - 6, sy - 8, sy + 6, sy + 8};
        const float bx[4] = {mx + 4, mx + 6, mx - 4, mx - 6}, by[4] = {sy - 8, sy - 6, sy + 8, sy + 6};
        c.polygon(ax, ay, 4, white);
        c.polygon(bx, by, 4, white);
    } else {
        c.wave(sx + 17, sy, 8, 2.6f, white);
        if (frac > .5f) {
            c.wave(sx + 17, sy, 15, 2.6f, white);
        }
    }

    // Bar with its end dot.
    const float bx0 = 76, bx1 = W - 84, by = H / 2;
    c.roundBox(bx0, by - 3, bx1, by + 3, 3, track);
    const float end = bx0 + (bx1 - bx0) * frac;
    if (frac > 0.f) {
        c.roundBox(bx0, by - 3, end > bx0 + 6 ? end : bx0 + 6, by + 3, 3, fill);
    }
    c.disc(end, by, 7, white);

    // Value, right-aligned: up to three digits.
    const float dh = 24, dw = 14, t = 3.8f, gap = 5, right = W - 26, top = (H - dh) / 2;
    unsigned v = value > 999 ? 999 : value;
    float x = right - dw;
    do {
        digit(c, x, top, dw, dh, t, v % 10, white);
        v /= 10;
        x -= dw + gap;
    } while (v);
}

}  // namespace pill
}  // namespace evo

#endif
