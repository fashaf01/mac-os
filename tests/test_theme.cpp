#include "design/Theme.h"
#include <cstdio>
#include <cmath>

using namespace md;
static int failures = 0;
#define CHECK(c, m) do { if(!(c)) { printf("FAIL: %s\n", m); ++failures; } } while(0)

// Known-good WCAG reference values.
static void referenceChecks() {
    const Color white = Color::rgba8(255,255,255);
    const Color black = Color::rgba8(0,0,0);
    CHECK(std::fabs(contrastRatio(white, black) - 21.0f) < 0.01f, "white/black == 21:1");
    CHECK(std::fabs(contrastRatio(white, white) - 1.0f)  < 0.01f, "white/white == 1:1");
    CHECK(std::fabs(relativeLuminance(white) - 1.0f) < 0.001f, "white luminance == 1");
    CHECK(std::fabs(relativeLuminance(black)) < 0.001f, "black luminance == 0");
    // #767676 on white is the canonical 4.54:1 AA boundary colour.
    CHECK(std::fabs(contrastRatio(Color::rgba8(0x76,0x76,0x76), white) - 4.54f) < 0.05f,
          "#767676 on white == 4.54:1");
    // compositing is source-over
    Color half = Color::rgba8(255,255,255,0.5f);
    Color over = composite(half, black);
    CHECK(std::fabs(over.r - 0.5f) < 0.01f && std::fabs(over.a - 1.0f) < 0.01f, "50% white over black");
    CHECK(std::fabs(composite(white, black).r - 1.0f) < 0.001f, "opaque over is identity");
}

// Every text token must clear WCAG AA on the hardest backdrop.
static void auditTheme(const Theme& t, const char* name) {
    const Color worst = Color::rgba8(128,128,128);
    struct Pair { const char* label; Color fg; Color surface; float min; };
    const Pair pairs[] = {
        { "tooltip text",   t.tooltipText, t.tooltipFill, 4.5f },  // AA body text
        { "running dot",    t.runningDot,  t.panelFill,   3.0f },  // AA non-text
        { "separator",      t.separator,   t.panelFill,   1.3f },  // decorative
        { "panel hairline", t.panelStroke, t.panelFill,   1.1f },  // decorative
    };
    for (const auto& p : pairs) {
        const Color surface = composite(p.surface, worst);
        const float ratio = contrastRatio(composite(p.fg, surface), surface);
        printf("  %-6s %-15s %5.2f:1 (min %.1f) %s\n", name, p.label, ratio, p.min,
               ratio >= p.min ? "ok" : "TOO LOW");
        if (ratio < p.min) ++failures;
    }
    // Panel material must actually be translucent, or it is not glass.
    CHECK(t.panelFill.a > 0.3f && t.panelFill.a < 0.85f, "panel fill is translucent");
    CHECK(t.tooltipFill.a > 0.85f, "tooltip is near-opaque so its text stays legible");
}

int main() {
    referenceChecks();
    printf("contrast on worst-case mid-grey backdrop:\n");
    auditTheme(Theme::darkTheme(),  "dark");
    auditTheme(Theme::lightTheme(), "light");

    CHECK(Theme::darkTheme().dark && !Theme::lightTheme().dark, "theme flags");
    CHECK(Theme::resolve(L"dark").dark, "explicit dark honoured");
    CHECK(!Theme::resolve(L"light").dark, "explicit light honoured");

    printf(failures ? "\n%d CHECK(S) FAILED\n" : "\nall checks passed\n", failures);
    return failures ? 1 : 0;
}
