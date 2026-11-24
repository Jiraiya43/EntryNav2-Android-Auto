// Copyright (c) 2026 Jiraiya43
// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
// The picture asked of the phone for a given panel.

// Android Auto streams one of three canvases: 800x480, 1280x720 or 1920x1080. The smallest one that holds the panel is
// asked for; the phone is told to keep its UI in the top-left part the panel shows (insets, core/sdr_builder.cpp),
// and the video surface shows that part only (platform/evo_video.cpp). The reference car is one case of the rule: a
// 1280x480 panel in the 1280x720 canvas, its top 480 rows shown.
// A panel partly hidden by its surround (the MINI's round one, platform/car_brand.hpp) keeps the whole picture; the
// phone is told to keep its UI away from the hidden sides.
// The insets are the phone's margins: its own display is the canvas less them, and it takes touches in that display's
// pixels. A panel pixel x is the phone's x - leftInset() (platform/evo_touch.hpp); without a left margin, the same.
#ifndef ENTRY_AAD_CORE_DISPLAY_PLAN_HPP
#define ENTRY_AAD_CORE_DISPLAY_PLAN_HPP

#include <stdint.h>

namespace aa {

struct DisplayPlan {
    uint32_t width, height;              // the panel
    uint32_t shownWidth, shownHeight;    // the part of the canvas that is shown: the panel's size, unless it is scaled
    uint32_t canvasWidth, canvasHeight;  // what the phone streams
    uint32_t resolution;                 // the canvas as the protocol numbers it: 1, 2 or 3
    uint32_t hiddenSide;                 // panel columns hidden on each side by the surround
    // The reference car.
    DisplayPlan()
        : width(1280),
          height(480),
          shownWidth(1280),
          shownHeight(480),
          canvasWidth(1280),
          canvasHeight(720),
          resolution(2),
          hiddenSide(0) {}
    uint32_t bottomInset() const { return canvasHeight - shownHeight; }
    uint32_t leftInset() const { return hiddenSide; }
    uint32_t rightInset() const { return canvasWidth - shownWidth + hiddenSide; }
    // True when the shown part is stretched to the panel (a panel larger than the largest canvas allowed).
    bool scaled() const { return shownWidth != width || shownHeight != height; }
    // Panel pixels to canvas pixels: 1 unless scaled.
    double touchScale() const { return static_cast<double>(shownWidth) / static_cast<double>(width); }
    // The phone's own display, the canvas less its margins: the area its touches are given in.
    uint32_t phoneWidth() const { return canvasWidth - leftInset() - rightInset(); }
    uint32_t phoneHeight() const { return canvasHeight - bottomInset(); }
    bool reference() const { return width == 1280 && height == 480; }
    // A panel size already run in a car: the reference one, and 800x480 (docs/COMPATIBILITE.md).
    bool triedInCar() const { return reference() || (width == 800 && height == 480); }
};

// The plan for a panel; false for an empty panel or one larger than every canvas.
// largestCanvas (1, 2 or 3) caps the canvas asked of the phone. A phone only sends 1920x1080 on a 5 GHz network, and
// the car's access point is on 2.4 GHz: a wireless session passes 2. A panel that the largest canvas allowed does not
// hold gets that canvas scaled to it, both axes equally: 1440x540 shows the top 1280x480 of 1280x720 at 1.125.
inline bool planDisplay(uint32_t panelWidth, uint32_t panelHeight, DisplayPlan& out, uint32_t largestCanvas = 3) {
    static const uint32_t kCanvas[3][2] = {{800, 480}, {1280, 720}, {1920, 1080}};
    if (!panelWidth || !panelHeight || panelWidth > kCanvas[2][0] || panelHeight > kCanvas[2][1]) {
        return false;
    }
    if (largestCanvas < 1 || largestCanvas > 3) {
        largestCanvas = 3;
    }
    for (uint32_t i = 0; i < largestCanvas; ++i) {
        if (panelWidth <= kCanvas[i][0] && panelHeight <= kCanvas[i][1]) {
            out.width = panelWidth;
            out.height = panelHeight;
            out.shownWidth = panelWidth;
            out.shownHeight = panelHeight;
            out.canvasWidth = kCanvas[i][0];
            out.canvasHeight = kCanvas[i][1];
            out.resolution = i + 1;
            out.hiddenSide = 0;
            return true;
        }
    }
    // Scaled: the whole width of the largest canvas allowed when the panel is wider than it is tall for that canvas,
    // else its whole height.
    const uint32_t cw = kCanvas[largestCanvas - 1][0], ch = kCanvas[largestCanvas - 1][1];
    out.width = panelWidth;
    out.height = panelHeight;
    if (static_cast<uint64_t>(panelWidth) * ch >= static_cast<uint64_t>(panelHeight) * cw) {
        out.shownWidth = cw;
        out.shownHeight = static_cast<uint32_t>(static_cast<uint64_t>(panelHeight) * cw / panelWidth);
    } else {
        out.shownHeight = ch;
        out.shownWidth = static_cast<uint32_t>(static_cast<uint64_t>(panelWidth) * ch / panelHeight);
    }
    out.canvasWidth = cw;
    out.canvasHeight = ch;
    out.resolution = largestCanvas;
    out.hiddenSide = 0;
    return out.shownWidth > 0 && out.shownHeight > 0;
}

}  // namespace aa

#endif
