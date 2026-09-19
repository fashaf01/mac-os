#include "dock/DockLayout.h"
#include "anim/Animation.h"
#include <cstdio>
#include <cmath>
#include <cassert>

using namespace md;

static int failures = 0;
#define CHECK(cond, msg) do { if(!(cond)) { printf("FAIL: %s\n", msg); ++failures; } } while(0)

int main() {
    DockMetrics m;
    m.iconSize = 52; m.gap = 8; m.padding = 8; m.dotBand = 7;
    m.bottomMargin = 8; m.maxScale = 1.75f; m.influence = 2.4f;

    const float W = 1920, H = dockWindowHeight(m, 24.0f);
    const size_t N = 9;

    // --- resting layout -------------------------------------------------
    auto rest = computeDockLayout(N, m, W, H, -1e6f, 0.0f, nullptr);
    CHECK(rest.items.size() == N, "rest item count");
    for (size_t i = 0; i < N; ++i)
        CHECK(std::fabs(rest.items[i].scale - 1.0f) < 1e-4f, "rest scale is 1");

    const float panelW = rest.panel.width();
    const float expectW = N * (m.iconSize + m.gap) - m.gap + 2 * m.padding;
    CHECK(std::fabs(panelW - expectW) < 0.5f, "rest panel width");
    CHECK(std::fabs(rest.panel.centerX() - W / 2) < 0.5f, "rest panel centered");
    CHECK(std::fabs(rest.panel.height() - dockPanelHeight(m)) < 0.01f, "panel height");
    CHECK(std::fabs(rest.panel.bottom - (H - m.bottomMargin)) < 0.01f, "panel bottom margin");

    // no overlaps, uniform spacing at rest
    for (size_t i = 1; i < N; ++i) {
        const float gap = rest.items[i].icon.left - rest.items[i-1].icon.right;
        CHECK(std::fabs(gap - m.gap) < 0.5f, "rest gap uniform");
    }

    // --- magnified layout -----------------------------------------------
    const float cursor = rest.items[4].icon.centerX();
    auto mag = computeDockLayout(N, m, W, H, cursor, 1.0f, nullptr);

    CHECK(mag.items[4].scale > 1.70f, "hovered item near max scale");
    for (size_t i = 0; i < N; ++i)
        CHECK(mag.items[i].scale >= 1.0f && mag.items[i].scale <= m.maxScale + 1e-3f,
              "scale within bounds");

    // monotonic falloff away from the cursor
    for (size_t i = 4; i + 1 < N; ++i)
        CHECK(mag.items[i].scale >= mag.items[i+1].scale - 1e-4f, "scale falls off right");
    for (size_t i = 4; i > 0; --i)
        CHECK(mag.items[i].scale >= mag.items[i-1].scale - 1e-4f, "scale falls off left");

    // THE key property: the icon you point at stays under the pointer.
    CHECK(mag.items[4].icon.contains(cursor, mag.items[4].icon.centerY()),
          "hovered icon stays under cursor");

    // icons must never overlap when magnified
    for (size_t i = 1; i < N; ++i)
        CHECK(mag.items[i].icon.left >= mag.items[i-1].icon.right - 0.01f,
              "no overlap when magnified");

    // magnified icons grow upward out of a constant-height panel
    CHECK(std::fabs(mag.panel.height() - rest.panel.height()) < 0.01f, "panel height constant");
    CHECK(mag.items[4].icon.top < mag.panel.top, "magnified icon overhangs panel top");
    CHECK(mag.items[4].icon.bottom <= mag.panel.bottom + 0.01f, "icon sits on baseline");
    CHECK(dockWindowHeight(m, 24.0f) > dockPanelHeight(m) + m.bottomMargin
          + m.iconSize * (m.maxScale - 1.0f), "window has magnification headroom");

    // hit testing round-trips
    for (size_t i = 0; i < N; ++i) {
        const int hit = hitTestDock(mag, mag.items[i].icon.centerX(), mag.items[i].icon.centerY());
        CHECK(hit == (int)i, "hit test finds the right icon");
    }
    CHECK(hitTestDock(mag, 5.0f, 5.0f) == -1, "miss returns -1");

    // sweeping the cursor must never jump the layout (continuity check)
    float prevLeft = 0; bool first = true;
    for (float x = 0; x < W; x += 3.0f) {
        auto L = computeDockLayout(N, m, W, H, x, 1.0f, nullptr);
        if (!first) {
            CHECK(std::fabs(L.items[0].icon.left - prevLeft) < 6.0f, "layout is continuous");
        }
        prevLeft = L.items[0].icon.left; first = false;
    }

    // blending between rest and magnified must also be continuous
    prevLeft = 0; first = true;
    for (float a = 0; a <= 1.0f; a += 0.01f) {
        auto L = computeDockLayout(N, m, W, H, cursor, a, nullptr);
        if (!first) CHECK(std::fabs(L.items[0].icon.left - prevLeft) < 3.0f, "mag blend continuous");
        prevLeft = L.items[0].icon.left; first = false;
    }

    // --- edge cases -------------------------------------------------------
    auto empty = computeDockLayout(0, m, W, H, cursor, 1.0f, nullptr);
    CHECK(empty.items.empty(), "zero items is safe");
    auto one = computeDockLayout(1, m, W, H, cursor, 1.0f, nullptr);
    CHECK(one.items.size() == 1, "single item is safe");
    auto edge = computeDockLayout(N, m, W, H, 0.0f, 1.0f, nullptr);
    CHECK(edge.items.size() == N, "cursor at screen edge is safe");

    // bounce offsets shift icons upward only
    std::vector<float> bounces(N, 0.0f); bounces[2] = 20.0f;
    auto bounced = computeDockLayout(N, m, W, H, cursor, 1.0f, bounces.data());
    CHECK(std::fabs(bounced.items[2].icon.bottom - (mag.items[2].icon.bottom - 20.0f)) < 0.01f,
          "bounce lifts the icon");

    // --- animation --------------------------------------------------------
    Spring s; s.configure(300.0f, 30.0f); s.snapTo(0.0f); s.setTarget(1.0f);
    int steps = 0;
    while (s.step(1.0f/120.0f) && steps < 2000) ++steps;
    CHECK(steps < 2000, "spring settles");
    CHECK(std::fabs(s.value() - 1.0f) < 0.01f, "spring reaches target");
    CHECK(!s.step(1.0f/120.0f), "settled spring reports idle (timer can stop)");
    printf("  spring settled in %d frames (%.2fs)\n", steps, steps/120.0f);

    // a huge dt must not make the spring explode
    Spring s2; s2.snapTo(0.0f); s2.setTarget(1.0f);
    s2.step(5.0f);
    CHECK(std::fabs(s2.value()) < 10.0f, "spring survives a hitch");

    Bounce b; b.start();
    float peak = 0; int frames = 0;
    while (b.step(1.0f/60.0f) && frames < 1000) { peak = std::fmax(peak, b.offset(52.0f)); ++frames; }
    CHECK(frames < 1000 && !b.active(), "bounce terminates");
    CHECK(peak > 5.0f && peak < 52.0f, "bounce amplitude sane");
    CHECK(b.offset(52.0f) == 0.0f, "bounce returns to zero");
    printf("  bounce peak %.1fpx over %d frames\n", peak, frames);

    printf(failures ? "\n%d CHECK(S) FAILED\n" : "\nall checks passed\n", failures);
    return failures ? 1 : 0;
}
