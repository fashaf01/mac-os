#pragma once

#include <windows.h>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace md::platform {

// Tracks which applications currently have a real window on screen.
//
// Event-driven via SetWinEventHook: there is no polling loop, so an idle
// desktop costs literally nothing. Bursts of window events are coalesced by
// the owner through a short debounce timer.
class WindowWatcher {
public:
    using ChangeCallback = std::function<void()>;

    bool start(ChangeCallback onChange);
    void stop();

    // Rescans the desktop. Call this from the debounce timer, not the hook.
    void refresh();

    bool isRunning(const std::wstring& exePath) const;
    std::vector<HWND> windowsFor(const std::wstring& exePath) const;

    // Normalised image paths of every process with a visible app window.
    std::vector<std::wstring> runningProcesses() const;
    HWND foreground() const { return foreground_; }

private:
    static void CALLBACK hookProc(HWINEVENTHOOK, DWORD, HWND, LONG, LONG, DWORD, DWORD);
    static BOOL CALLBACK enumProc(HWND, LPARAM);

    HWINEVENTHOOK objectHook_ = nullptr;
    HWINEVENTHOOK foregroundHook_ = nullptr;
    ChangeCallback onChange_;

    // key: lower-cased full path of the process image
    std::unordered_map<std::wstring, std::vector<HWND>> byProcess_;
    HWND foreground_ = nullptr;
};

// True for top-level windows a user would consider "an open app window".
bool isAppWindow(HWND hwnd);

// Full image path of the process owning a window, or "" if inaccessible.
std::wstring processPathForWindow(HWND hwnd);

} // namespace md::platform
