#include "dock/DockLayout.h"

#include <cmath>

namespace md {
namespace {

constexpr float kPi = 3.14159265358979323846f;

// Raised-cosine falloff: 1 directly under the cursor, 0 at the edge of the
// influence radius, with zero slope at both ends. That flat slope at the edge
// is what stops the outermost icons from visibly popping as the cursor
// crosses the boundary.
float falloff(float normalizedDistance) {
    if (normalizedDistance >= 1.0f) return 0.0f;
    if (normalizedDistance <= 0.0f) return 1.0f;
    return 0.5f * (1.0f + std::cos(kPi * normalizedDistance));
}

} // namespace

float dockPanelHeight(const DockMetrics& m) {
    return m.iconSize + m.padding * 2.0f + m.dotBand;
}

float dockWindowHeight(const DockMetrics& m, float tooltipHeight) {
    const float grow = m.iconSize * (m.maxScale - 1.0f);
    const float bounceHeadroom = m.iconSize * 0.55f;
    return dockPanelHeight(m) + m.bottomMargin + grow + bounceHeadroom
         + tooltipHeight + m.gap * 2.0f;
}

DockLayoutResult computeDockLayout(size_t count,
                                   const DockMetrics& m,
                                   float windowWidth,
                                   float windowHeight,
                                   float cursorX,
                                   float magAmount,
                                   const float* bounceOffsets) {
    DockLayoutResult result;
    result.panelHeight = dockPanelHeight(m);
    if (count == 0) return result;

    magAmount = clampf(magAmount, 0.0f, 1.0f);

    const float unit      = m.iconSize + m.gap;
    const float restTotal = unit * static_cast<float>(count);
    const float restStart = (windowWidth - restTotal) * 0.5f;
    const float influencePx = m.influence * m.iconSize;

    // 1. Scale per item, from the cursor's distance in *resting* space. Using
    //    resting positions keeps the mapping stable: if scales were measured
    //    against magnified positions the layout would chase itself.
    std::vector<float> scales(count, 1.0f);
    std::vector<float> widths(count, unit);

    for (size_t i = 0; i < count; ++i) {
        const float restCenter = restStart + unit * static_cast<float>(i) + unit * 0.5f;
        const float d = std::fabs(cursorX - restCenter) / influencePx;
        const float boost = (m.maxScale - 1.0f) * falloff(d) * magAmount;
        scales[i] = 1.0f + boost;
        widths[i] = m.iconSize * scales[i] + m.gap;
    }

    // 2. Anchor the magnified strip so the content directly beneath the cursor
    //    stays beneath the cursor. Without this the whole dock slides sideways
    //    as icons grow and the icon you aimed at escapes the pointer.
    const float t   = clampf(cursorX - restStart, 0.0f, restTotal);
    const size_t idx = static_cast<size_t>(clampf(std::floor(t / unit), 0.0f,
                                                  static_cast<float>(count - 1)));
    const float frac = clampf((t - unit * static_cast<float>(idx)) / unit, 0.0f, 1.0f);

    float prefix = 0.0f;
    for (size_t i = 0; i < idx; ++i) prefix += widths[i];

    const float cursorInMagnified = prefix + frac * widths[idx];

    float magTotal = 0.0f;
    for (size_t i = 0; i < count; ++i) magTotal += widths[i];

    float magStart = cursorX - cursorInMagnified;

    // At rest (or when the cursor is away) fall back to a centred strip, and
    // blend between the two so there is no jump as magnification fades out.
    const float centeredStart = (windowWidth - magTotal) * 0.5f;
    magStart = lerp(centeredStart, magStart, magAmount);

    // 3. Vertical: the panel keeps a constant height and magnified icons grow
    //    upward out of it, exactly as the macOS dock does.
    const float panelBottom = windowHeight - m.bottomMargin;
    const float panelTop    = panelBottom - result.panelHeight;
    const float iconBottom  = panelBottom - m.padding - m.dotBand;

    result.items.resize(count);

    float x = magStart;
    for (size_t i = 0; i < count; ++i) {
        const float drawn = m.iconSize * scales[i];
        const float bounce = bounceOffsets ? bounceOffsets[i] : 0.0f;

        ItemLayout& item = result.items[i];
        item.scale = scales[i];
        item.icon = {
            x + m.gap * 0.5f,
            iconBottom - drawn - bounce,
            x + m.gap * 0.5f + drawn,
            iconBottom - bounce
        };
        item.hit = { x, panelTop, x + widths[i], panelBottom };

        x += widths[i];
    }

    result.panel = {
        magStart + m.gap * 0.5f - m.padding,
        panelTop,
        magStart + magTotal - m.gap * 0.5f + m.padding,
        panelBottom
    };

    return result;
}

int hitTestDock(const DockLayoutResult& layout, float x, float y) {
    for (size_t i = 0; i < layout.items.size(); ++i) {
        const ItemLayout& item = layout.items[i];
        // Accept a hit on the magnified icon too, since it overhangs the panel.
        if (item.hit.contains(x, y) || item.icon.contains(x, y)) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

} // namespace md
