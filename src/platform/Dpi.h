#pragma once
#include <windows.h>

namespace md::platform {

// Per-monitor DPI, with a graceful path for Windows 10 builds that predate
// GetDpiForWindow.
UINT  dpiForWindow(HWND hwnd);
UINT  dpiForPoint(POINT pt);
float scaleForWindow(HWND hwnd);   // 1.0 at 96 DPI

} // namespace md::platform
