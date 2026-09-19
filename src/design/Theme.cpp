#include "design/Theme.h"
#include "core/Log.h"

#include <windows.h>
#include <cmath>

namespace md {
namespace {

float channelToLinear(float c) {
    return (c <= 0.03928f) ? (c / 12.92f)
                           : std::pow((c + 0.055f) / 1.055f, 2.4f);
}

// The worst-case backdrop a translucent dock surface can sit on. Mid grey is
// the hardest case for both light and dark materials, so auditing against it
// means the palette holds up on any wallpaper.
const Color kWorstCaseBackdrop = Color::rgba8(128, 128, 128, 1.0f);

void report(const wchar_t* label, const Color& fg, const Color& surface) {
    const Color effective = composite(surface, kWorstCaseBackdrop);
    const Color text      = composite(fg, effective);
    const float ratio     = contrastRatio(text, effective);
    MD_LOG(L"  contrast %-22s %.2f:1  %s", label, ratio,
           ratio >= 4.5f ? L"PASS (AA)" : (ratio >= 3.0f ? L"WARN (AA large only)" : L"FAIL"));
}

} // namespace

float relativeLuminance(const Color& c) {
    return 0.2126f * channelToLinear(c.r)
         + 0.7152f * channelToLinear(c.g)
         + 0.0722f * channelToLinear(c.b);
}

float contrastRatio(const Color& fg, const Color& bg) {
    const float l1 = relativeLuminance(fg);
    const float l2 = relativeLuminance(bg);
    const float hi = l1 > l2 ? l1 : l2;
    const float lo = l1 > l2 ? l2 : l1;
    return (hi + 0.05f) / (lo + 0.05f);
}

Color composite(const Color& over, const Color& under) {
    const float a = over.a + under.a * (1.0f - over.a);
    if (a <= 0.0001f) return { 0, 0, 0, 0 };
    return {
        (over.r * over.a + under.r * under.a * (1.0f - over.a)) / a,
        (over.g * over.a + under.g * under.a * (1.0f - over.a)) / a,
        (over.b * over.a + under.b * under.a * (1.0f - over.a)) / a,
        a
    };
}

Theme Theme::darkTheme() {
    Theme t;
    t.dark = true;

    t.panelFill      = Color::rgba8( 30,  30,  32, 0.62f);
    t.panelStroke    = Color::rgba8(255, 255, 255, 0.16f);
    t.panelHighlight = Color::rgba8(255, 255, 255, 0.10f);
    t.panelShadow    = Color::rgba8(  0,   0,   0, 0.45f);

    t.separator      = Color::rgba8(255, 255, 255, 0.22f);
    t.runningDot     = Color::rgba8(235, 235, 245, 0.80f);
    t.iconShadow     = Color::rgba8(  0,   0,   0, 0.35f);

    t.tooltipFill    = Color::rgba8( 44,  44,  46, 0.94f);
    t.tooltipStroke  = Color::rgba8(255, 255, 255, 0.14f);
    t.tooltipText    = Color::rgba8(255, 255, 255, 0.96f);

    t.pressOverlay   = Color::rgba8(255, 255, 255, 0.12f);
    t.accent         = Color::rgba8( 10, 132, 255, 1.00f);
    return t;
}

Theme Theme::lightTheme() {
    Theme t;
    t.dark = false;

    t.panelFill      = Color::rgba8(246, 246, 248, 0.66f);
    t.panelStroke    = Color::rgba8(  0,   0,   0, 0.12f);
    t.panelHighlight = Color::rgba8(255, 255, 255, 0.55f);
    t.panelShadow    = Color::rgba8(  0,   0,   0, 0.28f);

    t.separator      = Color::rgba8(  0,   0,   0, 0.20f);
    t.runningDot     = Color::rgba8( 28,  28,  30, 0.70f);
    t.iconShadow     = Color::rgba8(  0,   0,   0, 0.22f);

    t.tooltipFill    = Color::rgba8(252, 252, 254, 0.96f);
    t.tooltipStroke  = Color::rgba8(  0,   0,   0, 0.12f);
    t.tooltipText    = Color::rgba8( 16,  16,  18, 0.96f);

    t.pressOverlay   = Color::rgba8(  0,   0,   0, 0.10f);
    t.accent         = Color::rgba8(  0, 122, 255, 1.00f);
    return t;
}

Theme Theme::resolve(const std::wstring& mode) {
    if (mode == L"dark")  return darkTheme();
    if (mode == L"light") return lightTheme();
    return systemUsesDarkTheme() ? darkTheme() : lightTheme();
}

bool systemUsesDarkTheme() {
    DWORD value = 1;           // Windows default is light apps -> 1
    DWORD size  = sizeof(value);
    const LSTATUS st = RegGetValueW(
        HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        L"AppsUseLightTheme", RRF_RT_REG_DWORD, nullptr, &value, &size);

    if (st != ERROR_SUCCESS) return true; // fall back to dark: it is the safer look
    return value == 0;
}

void auditContrast(const Theme& theme) {
    MD_LOG(L"contrast audit (%s theme, worst-case mid-grey backdrop):",
           theme.dark ? L"dark" : L"light");
    report(L"tooltip text",   theme.tooltipText, theme.tooltipFill);
    report(L"running dot",    theme.runningDot,  theme.panelFill);
    report(L"separator",      theme.separator,   theme.panelFill);
    report(L"panel hairline", theme.panelStroke, theme.panelFill);
}

} // namespace md
