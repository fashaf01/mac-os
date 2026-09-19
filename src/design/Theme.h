#pragma once

#include <cstdint>
#include <string>

namespace md {

// Straight (non-premultiplied) linear-ish sRGB colour with alpha.
struct Color {
    float r = 0.0f, g = 0.0f, b = 0.0f, a = 1.0f;

    static constexpr Color rgba8(uint8_t r8, uint8_t g8, uint8_t b8, float alpha = 1.0f) {
        return { r8 / 255.0f, g8 / 255.0f, b8 / 255.0f, alpha };
    }

    constexpr Color withAlpha(float alpha) const { return { r, g, b, alpha }; }
    constexpr Color scaledAlpha(float k) const   { return { r, g, b, a * k }; }
};

// --- Contrast maths (WCAG 2.1) -------------------------------------------
// These exist so the palette is a measured thing, not a guess. Every text
// token is audited against the surface it lands on at startup.

float relativeLuminance(const Color& c);              // alpha ignored
float contrastRatio(const Color& fg, const Color& bg); // 1.0 .. 21.0
Color composite(const Color& over, const Color& under); // source-over blend

// --- Token set ------------------------------------------------------------
// Named by role, never by value, so a retheme is a data change.
struct Theme {
    bool dark = true;

    // Dock panel surface
    Color panelFill;        // translucent material
    Color panelStroke;      // hairline border
    Color panelHighlight;   // inner top highlight, gives the glass edge
    Color panelShadow;      // drop shadow under the panel

    // Content
    Color separator;
    Color runningDot;
    Color iconShadow;

    // Tooltip / label chip
    Color tooltipFill;
    Color tooltipStroke;
    Color tooltipText;

    // Selection / press feedback
    Color pressOverlay;
    Color accent;

    float panelRadius     = 18.0f; // logical px at iconSize 52
    float tooltipRadius   = 7.0f;

    static Theme darkTheme();
    static Theme lightTheme();

    // mode: "auto" (follow Windows), "dark", or "light".
    static Theme resolve(const std::wstring& mode);
};

bool systemUsesDarkTheme();

// Logs every text-on-surface pair with its measured ratio and a PASS/FAIL
// against WCAG AA (4.5:1). Runs once at startup; costs nothing afterwards.
void auditContrast(const Theme& theme);

} // namespace md
