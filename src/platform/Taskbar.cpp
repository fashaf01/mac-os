#include "platform/Taskbar.h"
#include "core/Log.h"

#include <shellapi.h>

namespace md::platform {
namespace {

// The primary taskbar, plus one per additional monitor on multi-monitor setups.
template <typename Fn>
void forEachTaskbarWindow(Fn&& fn) {
    if (HWND primary = FindWindowW(L"Shell_TrayWnd", nullptr)) fn(primary);

    HWND secondary = nullptr;
    while ((secondary = FindWindowExW(nullptr, secondary,
                                      L"Shell_SecondaryTrayWnd", nullptr)) != nullptr) {
        fn(secondary);
    }
}

APPBARDATA taskbarAppBar(HWND primary) {
    APPBARDATA data{};
    data.cbSize = sizeof(data);
    data.hWnd   = primary;
    return data;
}

} // namespace

bool Taskbar::hidden() const {
    HWND primary = FindWindowW(L"Shell_TrayWnd", nullptr);
    return primary != nullptr && !IsWindowVisible(primary);
}

bool Taskbar::hide(int& savedState) {
    HWND primary = FindWindowW(L"Shell_TrayWnd", nullptr);
    if (!primary) {
        MD_LOG(L"taskbar window not found; leaving the screen as it is");
        return false;
    }

    // Only capture the state the first time. Re-reading it after we have
    // already switched to auto-hide would store our own change as if it were
    // the user's setting.
    if (savedState < 0) {
        APPBARDATA query = taskbarAppBar(primary);
        savedState = static_cast<int>(SHAppBarMessage(ABM_GETSTATE, &query));
        MD_LOG(L"remembered taskbar state 0x%X", savedState);
    }

    APPBARDATA set = taskbarAppBar(primary);
    set.lParam = ABS_AUTOHIDE;
    SHAppBarMessage(ABM_SETSTATE, &set);

    int count = 0;
    forEachTaskbarWindow([&count](HWND hwnd) {
        ShowWindow(hwnd, SW_HIDE);
        ++count;
    });

    MD_LOG(L"taskbar hidden (%d window(s))", count);
    return true;
}

void Taskbar::restore(int& savedState) {
    // Unconditional: this has to work when the taskbar was hidden by an
    // earlier run of MacDock that never got to clean up after itself.
    // Showing an already-visible window is a harmless no-op.
    forEachTaskbarWindow([](HWND hwnd) { ShowWindow(hwnd, SW_SHOW); });

    if (HWND primary = FindWindowW(L"Shell_TrayWnd", nullptr)) {
        APPBARDATA set = taskbarAppBar(primary);
        // Without a remembered state, always-on-top is the Windows default and
        // the safe guess. Leaving it alone would strand the taskbar in the
        // auto-hide mode we put it in.
        set.lParam = (savedState >= 0) ? savedState : ABS_ALWAYSONTOP;
        SHAppBarMessage(ABM_SETSTATE, &set);
    }

    savedState = -1;
    MD_LOG(L"taskbar restored");
}

} // namespace md::platform
