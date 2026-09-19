#include "platform/TrayIcon.h"
#include "core/Log.h"

#include <shellapi.h>

namespace md::platform {
namespace {
constexpr UINT kIconId = 1;

NOTIFYICONDATAW makeData(HWND owner) {
    NOTIFYICONDATAW data{};
    data.cbSize = sizeof(data);
    data.hWnd   = owner;
    data.uID    = kIconId;
    return data;
}
} // namespace

bool TrayIcon::add(HWND owner, UINT callbackMessage, const std::wstring& tooltip) {
    if (added_) return true;

    NOTIFYICONDATAW data = makeData(owner);
    data.uFlags           = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    data.uCallbackMessage = callbackMessage;
    data.hIcon            = LoadIconW(nullptr, IDI_APPLICATION);
    wcsncpy_s(data.szTip, tooltip.c_str(), _TRUNCATE);

    if (!Shell_NotifyIconW(NIM_ADD, &data)) {
        MD_LOG(L"Shell_NotifyIcon NIM_ADD failed (%lu)", GetLastError());
        return false;
    }

    data.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &data);

    owner_ = owner;
    callbackMessage_ = callbackMessage;
    added_ = true;
    return true;
}

void TrayIcon::remove() {
    if (!added_) return;
    NOTIFYICONDATAW data = makeData(owner_);
    Shell_NotifyIconW(NIM_DELETE, &data);
    added_ = false;
}

void TrayIcon::updateTooltip(const std::wstring& tooltip) {
    if (!added_) return;
    NOTIFYICONDATAW data = makeData(owner_);
    data.uFlags = NIF_TIP;
    wcsncpy_s(data.szTip, tooltip.c_str(), _TRUNCATE);
    Shell_NotifyIconW(NIM_MODIFY, &data);
}

} // namespace md::platform
