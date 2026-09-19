#include "core/Log.h"

#include <windows.h>
#include <shlobj.h>
#include <cstdio>
#include <cstdarg>

namespace md::log {
namespace {
HANDLE g_file = INVALID_HANDLE_VALUE;
CRITICAL_SECTION g_lock{};
bool g_ready = false;
} // namespace

void init() {
    if (g_ready) return;
    InitializeCriticalSection(&g_lock);
    g_ready = true;

    PWSTR base = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &base))) {
        wchar_t dir[MAX_PATH];
        swprintf_s(dir, L"%s\\MacDock", base);
        CreateDirectoryW(dir, nullptr);

        wchar_t path[MAX_PATH];
        swprintf_s(path, L"%s\\macdock.log", dir);
        g_file = CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ, nullptr,
                             OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        CoTaskMemFree(base);
    }
}

void shutdown() {
    if (!g_ready) return;
    if (g_file != INVALID_HANDLE_VALUE) {
        CloseHandle(g_file);
        g_file = INVALID_HANDLE_VALUE;
    }
    DeleteCriticalSection(&g_lock);
    g_ready = false;
}

void write(const wchar_t* fmt, ...) {
    wchar_t body[1024];
    va_list args;
    va_start(args, fmt);
    _vsnwprintf_s(body, _TRUNCATE, fmt, args);
    va_end(args);

    SYSTEMTIME st{};
    GetLocalTime(&st);

    wchar_t line[1200];
    swprintf_s(line, L"[%02d:%02d:%02d.%03d] %s\r\n",
               st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, body);

    OutputDebugStringW(line);

    if (!g_ready) return;
    EnterCriticalSection(&g_lock);
    if (g_file != INVALID_HANDLE_VALUE) {
        char utf8[2400];
        const int n = WideCharToMultiByte(CP_UTF8, 0, line, -1, utf8, sizeof(utf8), nullptr, nullptr);
        if (n > 1) {
            DWORD written = 0;
            WriteFile(g_file, utf8, static_cast<DWORD>(n - 1), &written, nullptr);
        }
    }
    LeaveCriticalSection(&g_lock);
}

} // namespace md::log
