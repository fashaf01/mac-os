#pragma once

#include "core/Geometry.h"
#include <vector>

namespace md {

// Everything in physical pixels; the caller multiplies logical sizes by the
// monitor scale before filling this in.
struct DockMetrics {
    float iconSize      = 52.0f;
    float gap           = 8.0f;
    float padding       = 8.0f;
    float dotBand       = 7.0f;   // strip under the icons that holds running dots
    float bottomMargin  = 8.0f;
    float maxScale      = 1.75f;
    float influence     = 2.4f;   // in multiples of iconSize
    float panelRadius   = 18.0f;
};

struct ItemLayout {
    RectF icon;      // where the (possibly magnified) icon is drawn
    RectF hit;       // full-height click target inside the panel strip
    float scale = 1.0f;
};

struct DockLayoutResult {
    std::vector<ItemLayout> items;
    RectF panel;                 // the glass strip
    float panelHeight = 0.0f;
};

// Resting height of the panel, which is what we reserve from the shell.
float dockPanelHeight(const DockMetrics& m);

// Total window height needed to hold the panel plus the headroom that
// magnified icons, the launch bounce and the tooltip grow into.
float dockWindowHeight(const DockMetrics& m, float tooltipHeight);

// `magAmount` blends between the resting layout (0) and full magnification (1),
// which is what makes the dock ease in and out instead of snapping.
// `bounceOffsets` may be null; when given it must hold `count` entries of
// upward pixel offsets.
DockLayoutResult computeDockLayout(size_t count,
                                   const DockMetrics& metrics,
                                   float windowWidth,
                                   float windowHeight,
                                   float cursorX,
                                   float magAmount,
                                   const float* bounceOffsets);

// Index of the item under a point, or -1.
int hitTestDock(const DockLayoutResult& layout, float x, float y);

} // namespace md
