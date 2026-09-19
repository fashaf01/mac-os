#include "platform/AppBar.h"
#include "core/Log.h"

#include <shellapi.h>

namespace md::platform {

bool AppBar::registerBar(HWND hwnd, UINT callbackMessage) {
    if (registered_) return true;

    APPBARDATA data{};
    data.cbSize           = sizeof(data);
    data.hWnd             = hwnd;
    data.uCallbackMessage = callbackMessage;

    if (!SHAppBarMessage(ABM_NEW, &data)) {
        MD_LOG(L"ABM_NEW failed; the dock will overlap maximized windows");
        return false;
    }

    hwnd_ = hwnd;
    callbackMessage_ = callbackMessage;
    registered_ = true;
    return true;
}

void AppBar::unregisterBar() {
    if (!registered_) return;

    APPBARDATA data{};
    data.cbSize = sizeof(data);
    data.hWnd   = hwnd_;
    SHAppBarMessage(ABM_REMOVE, &data);

    registered_ = false;
    hwnd_ = nullptr;
}

RECT AppBar::reserveBottom(HWND hwnd, int heightPx) {
    RECT monitorRect{};
    if (HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY)) {
        MONITORINFO mi{};
        mi.cbSize = sizeof(mi);
        if (GetMonitorInfoW(monitor, &mi)) monitorRect = mi.rcMonitor;
    }
    if (IsRectEmpty(&monitorRect)) {
        monitorRect = { 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };
    }

    APPBARDATA data{};
    data.cbSize = sizeof(data);
    data.hWnd   = hwnd;
    data.uEdge  = ABE_BOTTOM;
    data.rc     = monitorRect;
    data.rc.top = data.rc.bottom - heightPx;

    if (!registered_) return data.rc;

    SHAppBarMessage(ABM_QUERYPOS, &data);
    // QUERYPOS may pull the edge in; re-anchor so we always own exactly the
    // height we asked for at the bottom edge.
    data.rc.top = data.rc.bottom - heightPx;
    SHAppBarMessage(ABM_SETPOS, &data);
    return data.rc;
}

void AppBar::reposition(HWND hwnd, int heightPx) {
    if (!registered_) return;
    reserveBottom(hwnd, heightPx);
}

} // namespace md::platform
