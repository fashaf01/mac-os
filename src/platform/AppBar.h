#pragma once
#include <windows.h>

namespace md::platform {

// Registers the dock with the shell as an application desktop bar so that
// maximized windows stop at the top of the dock instead of sliding under it.
// This is the documented Windows mechanism the taskbar itself uses, which is
// why it costs nothing at runtime: the shell does the work.
class AppBar {
public:
    bool registerBar(HWND hwnd, UINT callbackMessage);
    void unregisterBar();

    // Reserves `heightPx` at the bottom of the monitor containing `hwnd`.
    // Returns the rectangle the shell actually granted.
    RECT reserveBottom(HWND hwnd, int heightPx);

    // Call when the shell asks us to reposition (ABN_POSCHANGED).
    void reposition(HWND hwnd, int heightPx);

    bool registered() const { return registered_; }

private:
    HWND hwnd_ = nullptr;
    UINT callbackMessage_ = 0;
    bool registered_ = false;
};

} // namespace md::platform
