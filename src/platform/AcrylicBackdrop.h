#pragma once

#include "design/Theme.h"
#include <windows.h>

namespace md::platform {

// An optional blur-behind pane that sits under the dock panel.
//
// Real backdrop blur on Windows comes from DWM, and DWM applies it to a whole
// window rectangle. The dock's own window spans the full screen width (so
// magnified icons have room to overhang), which means enabling blur on it
// would paint a blurred bar across the entire screen. So the blur lives in its
// own small window that is kept exactly on the panel rectangle.
//
// The API behind this (SetWindowCompositionAttribute) is undocumented, so it
// is resolved at runtime and the dock falls back to its painted glass when it
// is unavailable. Nothing here is load-bearing.
class AcrylicBackdrop {
public:
    bool create(HWND owner);
    void destroy();

    bool available() const { return available_; }

    void setTint(const Color& tint);
    void setBounds(const RECT& screenRect, int cornerRadiusPx);
    void setVisible(bool visible);

private:
    void applyAccent();
    void applyCornerRegion();

    HWND hwnd_ = nullptr;
    bool available_ = false;
    bool visible_ = false;
    Color tint_{};
    RECT bounds_{};
    int cornerRadius_ = 0;
};

} // namespace md::platform
