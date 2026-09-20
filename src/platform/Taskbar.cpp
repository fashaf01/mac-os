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

} // namespace

bool Taskbar::hide() {
    if (hidden_) return true;

    HWND primary = FindWindowW(L"Shell_TrayWnd", nullptr);
    if (!primary) {
        MD_LOG(L"taskbar window not found; leaving the screen as it is");
        return false;
    }

    APPBARDATA data{};
    data.cbSize = sizeof(data);
    data.hWnd   = primary;
    previousState_ = static_cast<UINT>(SHAppBarMessage(ABM_GETSTATE, &data));

    data.lParam = ABS_AUTOHIDE;
    SHAppBarMessage(ABM_SETSTATE, &data);

    int count = 0;
    forEachTaskbarWindow([&count](HWND hwnd) {
        ShowWindow(hwnd, SW_HIDE);
        ++count;
    });

    hidden_ = true;
    MD_LOG(L"taskbar hidden (%d window(s), previous state 0x%X)", count, previousState_);
    return true;
}

void Taskbar::restore() {
    if (!hidden_) return;

    forEachTaskbarWindow([](HWND hwnd) { ShowWindow(hwnd, SW_SHOW); });

    if (HWND primary = FindWindowW(L"Shell_TrayWnd", nullptr)) {
        APPBARDATA data{};
        data.cbSize = sizeof(data);
        data.hWnd   = primary;
        data.lParam = previousState_;
        SHAppBarMessage(ABM_SETSTATE, &data);
    }

    hidden_ = false;
    MD_LOG(L"taskbar restored");
}

} // namespace md::platform
