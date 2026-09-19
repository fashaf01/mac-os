#pragma once

#include <windows.h>
#include <string>

namespace md::platform {

// A notification-area icon: the only way to quit or reload a window that has
// no title bar and never takes focus.
class TrayIcon {
public:
    bool add(HWND owner, UINT callbackMessage, const std::wstring& tooltip);
    void remove();
    void updateTooltip(const std::wstring& tooltip);

private:
    HWND owner_ = nullptr;
    UINT callbackMessage_ = 0;
    bool added_ = false;
};

} // namespace md::platform
