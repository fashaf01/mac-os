#include "platform/WindowWatcher.h"
#include "platform/AppResolver.h"
#include "core/Log.h"

#include <dwmapi.h>
#include <iterator>

#pragma comment(lib, "dwmapi.lib")

namespace md::platform {
namespace {
WindowWatcher* g_instance = nullptr;

bool isCloaked(HWND hwnd) {
    BOOL cloaked = FALSE;
    // UWP windows that are suspended report as cloaked; they should not light
    // up a running dot.
    if (SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked)))) {
        return cloaked != FALSE;
    }
    return false;
}
} // namespace

bool isAppWindow(HWND hwnd) {
    if (!IsWindow(hwnd) || !IsWindowVisible(hwnd)) return false;
    if (GetWindow(hwnd, GW_OWNER) != nullptr)      return false;

    const LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    if (exStyle & WS_EX_TOOLWINDOW) return false;
    if (exStyle & WS_EX_NOACTIVATE) return false;

    if (GetWindowTextLengthW(hwnd) == 0) return false;
    if (isCloaked(hwnd))                 return false;

    RECT rc{};
    if (!GetWindowRect(hwnd, &rc)) return false;
    if ((rc.right - rc.left) < 8 || (rc.bottom - rc.top) < 8) return false;

    return true;
}

std::wstring processPathForWindow(HWND hwnd) {
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (!pid) return {};

    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process) return {};

    wchar_t buffer[MAX_PATH * 2]{};
    DWORD size = static_cast<DWORD>(std::size(buffer));
    const BOOL ok = QueryFullProcessImageNameW(process, 0, buffer, &size);
    CloseHandle(process);

    return ok ? std::wstring(buffer, size) : std::wstring();
}

BOOL CALLBACK WindowWatcher::enumProc(HWND hwnd, LPARAM param) {
    auto* self = reinterpret_cast<WindowWatcher*>(param);
    if (!isAppWindow(hwnd)) return TRUE;

    const std::wstring path = processPathForWindow(hwnd);
    if (path.empty()) return TRUE;

    self->byProcess_[normalizePath(path)].push_back(hwnd);
    return TRUE;
}

void CALLBACK WindowWatcher::hookProc(HWINEVENTHOOK, DWORD event, HWND hwnd,
                                      LONG idObject, LONG idChild, DWORD, DWORD) {
    if (!g_instance) return;
    if (idObject != OBJID_WINDOW || idChild != CHILDID_SELF) return;
    if (!hwnd) return;

    if (event == EVENT_SYSTEM_FOREGROUND) g_instance->foreground_ = hwnd;

    // The hook runs on our message thread; do no real work here, just tell the
    // owner that a rescan is worth scheduling.
    if (g_instance->onChange_) g_instance->onChange_();
}

bool WindowWatcher::start(ChangeCallback onChange) {
    onChange_ = std::move(onChange);
    g_instance = this;

    objectHook_ = SetWinEventHook(
        EVENT_OBJECT_CREATE, EVENT_OBJECT_HIDE, nullptr, &WindowWatcher::hookProc,
        0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);

    foregroundHook_ = SetWinEventHook(
        EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, nullptr, &WindowWatcher::hookProc,
        0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);

    if (!objectHook_ && !foregroundHook_) {
        MD_LOG(L"SetWinEventHook failed; running indicators will be static");
        return false;
    }

    refresh();
    return true;
}

void WindowWatcher::stop() {
    if (objectHook_)     { UnhookWinEvent(objectHook_);     objectHook_ = nullptr; }
    if (foregroundHook_) { UnhookWinEvent(foregroundHook_); foregroundHook_ = nullptr; }
    byProcess_.clear();
    onChange_ = nullptr;
    if (g_instance == this) g_instance = nullptr;
}

void WindowWatcher::refresh() {
    byProcess_.clear();
    EnumWindows(&WindowWatcher::enumProc, reinterpret_cast<LPARAM>(this));
    foreground_ = GetForegroundWindow();
}

bool WindowWatcher::isRunning(const std::wstring& exePath) const {
    if (exePath.empty()) return false;
    return byProcess_.find(normalizePath(exePath)) != byProcess_.end();
}

std::vector<std::wstring> WindowWatcher::runningProcesses() const {
    std::vector<std::wstring> paths;
    paths.reserve(byProcess_.size());
    for (const auto& [path, windows] : byProcess_) {
        if (!windows.empty()) paths.push_back(path);
    }
    return paths;
}

std::vector<HWND> WindowWatcher::windowsFor(const std::wstring& exePath) const {
    if (exePath.empty()) return {};
    const auto it = byProcess_.find(normalizePath(exePath));
    return (it == byProcess_.end()) ? std::vector<HWND>{} : it->second;
}

} // namespace md::platform
