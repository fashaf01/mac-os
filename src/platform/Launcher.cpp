#include "platform/Launcher.h"
#include "core/Log.h"

#include <shlobj.h>
#include <shellapi.h>

#pragma comment(lib, "shell32.lib")

namespace md::platform {

bool launch(const std::wstring& path) {
    if (path.empty()) return false;

    SHELLEXECUTEINFOW info{};
    info.cbSize = sizeof(info);
    info.fMask  = SEE_MASK_NOASYNC | SEE_MASK_FLAG_NO_UI;
    info.lpVerb = L"open";
    info.lpFile = path.c_str();
    info.nShow  = SW_SHOWNORMAL;

    // Start in the app's own folder; some apps misbehave when inherited.
    std::wstring directory = path;
    if (const size_t slash = directory.find_last_of(L"\\/"); slash != std::wstring::npos) {
        directory.erase(slash);
        info.lpDirectory = directory.c_str();
    }

    if (!ShellExecuteExW(&info)) {
        MD_LOG(L"launch failed for %s (%lu)", path.c_str(), GetLastError());
        return false;
    }
    return true;
}

bool openShellLocation(const std::wstring& verbOrPath) {
    const HINSTANCE result = ShellExecuteW(nullptr, L"open", verbOrPath.c_str(),
                                           nullptr, nullptr, SW_SHOWNORMAL);
    return reinterpret_cast<INT_PTR>(result) > 32;
}

bool activateWindow(HWND hwnd) {
    if (!IsWindow(hwnd)) return false;

    if (IsIconic(hwnd)) ShowWindow(hwnd, SW_RESTORE);

    if (SetForegroundWindow(hwnd)) return true;

    // Windows refuses SetForegroundWindow from a process that does not own the
    // foreground. Borrowing the foreground thread's input queue is the
    // documented way around it.
    const DWORD ourThread = GetCurrentThreadId();
    const DWORD theirThread = GetWindowThreadProcessId(GetForegroundWindow(), nullptr);

    if (theirThread && theirThread != ourThread &&
        AttachThreadInput(ourThread, theirThread, TRUE)) {
        BringWindowToTop(hwnd);
        SetForegroundWindow(hwnd);
        SetFocus(hwnd);
        AttachThreadInput(ourThread, theirThread, FALSE);
        return true;
    }

    BringWindowToTop(hwnd);
    return false;
}

bool activateNext(const std::vector<HWND>& windows, HWND currentForeground) {
    if (windows.empty()) return false;

    size_t start = 0;
    for (size_t i = 0; i < windows.size(); ++i) {
        if (windows[i] == currentForeground) { start = i + 1; break; }
    }
    return activateWindow(windows[start % windows.size()]);
}

void openRecycleBin() {
    openShellLocation(L"shell:RecycleBinFolder");
}

bool recycleBinHasItems() {
    SHQUERYRBINFO info{};
    info.cbSize = sizeof(info);
    if (FAILED(SHQueryRecycleBinW(nullptr, &info))) return false;
    return info.i64NumItems > 0;
}

bool emptyRecycleBin(HWND owner) {
    // Keeps the confirmation prompt: silently destroying a user's files from a
    // dock click is not a decision this program gets to make.
    return SUCCEEDED(SHEmptyRecycleBinW(owner, nullptr, SHERB_NOPROGRESSUI));
}

} // namespace md::platform
