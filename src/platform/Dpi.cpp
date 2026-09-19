#include "platform/Dpi.h"
#include <shellscalingapi.h>

namespace md::platform {
namespace {

using GetDpiForWindowFn  = UINT (WINAPI*)(HWND);
using GetDpiForMonitorFn = HRESULT (WINAPI*)(HMONITOR, MONITOR_DPI_TYPE, UINT*, UINT*);

GetDpiForWindowFn loadGetDpiForWindow() {
    static GetDpiForWindowFn fn = [] {
        HMODULE user32 = GetModuleHandleW(L"user32.dll");
        return user32 ? reinterpret_cast<GetDpiForWindowFn>(
                            GetProcAddress(user32, "GetDpiForWindow"))
                      : nullptr;
    }();
    return fn;
}

GetDpiForMonitorFn loadGetDpiForMonitor() {
    static GetDpiForMonitorFn fn = [] {
        HMODULE shcore = LoadLibraryW(L"shcore.dll");
        return shcore ? reinterpret_cast<GetDpiForMonitorFn>(
                            GetProcAddress(shcore, "GetDpiForMonitor"))
                      : nullptr;
    }();
    return fn;
}

UINT dpiForMonitor(HMONITOR mon) {
    if (auto fn = loadGetDpiForMonitor(); fn && mon) {
        UINT x = 96, y = 96;
        if (SUCCEEDED(fn(mon, MDT_EFFECTIVE_DPI, &x, &y))) return x;
    }
    HDC dc = GetDC(nullptr);
    const UINT dpi = dc ? static_cast<UINT>(GetDeviceCaps(dc, LOGPIXELSX)) : 96u;
    if (dc) ReleaseDC(nullptr, dc);
    return dpi ? dpi : 96u;
}

} // namespace

UINT dpiForWindow(HWND hwnd) {
    if (auto fn = loadGetDpiForWindow(); fn && hwnd) {
        if (const UINT dpi = fn(hwnd); dpi) return dpi;
    }
    return dpiForMonitor(MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY));
}

UINT dpiForPoint(POINT pt) {
    return dpiForMonitor(MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST));
}

float scaleForWindow(HWND hwnd) {
    return static_cast<float>(dpiForWindow(hwnd)) / 96.0f;
}

} // namespace md::platform
