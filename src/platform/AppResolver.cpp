#include "platform/AppResolver.h"

#include <windows.h>
#include <shlwapi.h>
#include <vector>
#include <algorithm>
#include <iterator>
#include <cwctype>

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "version.lib")

namespace md::platform {
namespace {

bool fileExists(const std::wstring& p) {
    const DWORD attrs = GetFileAttributesW(p.c_str());
    return attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY);
}

std::wstring queryAppPaths(HKEY root, const std::wstring& exeName) {
    const std::wstring key =
        L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\" + exeName;

    wchar_t buffer[MAX_PATH * 2]{};
    DWORD size = sizeof(buffer);
    if (RegGetValueW(root, key.c_str(), nullptr, RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ,
                     nullptr, buffer, &size) != ERROR_SUCCESS) {
        return {};
    }

    std::wstring path(buffer);
    // App Paths values are sometimes quoted.
    if (!path.empty() && path.front() == L'"') {
        const size_t close = path.find(L'"', 1);
        path = (close == std::wstring::npos) ? path.substr(1) : path.substr(1, close - 1);
    }

    wchar_t expanded[MAX_PATH * 2]{};
    if (ExpandEnvironmentStringsW(path.c_str(), expanded, static_cast<DWORD>(std::size(expanded)))) {
        path = expanded;
    }
    return fileExists(path) ? path : std::wstring();
}

std::wstring expandEnv(const wchar_t* raw) {
    wchar_t out[MAX_PATH * 2]{};
    if (!ExpandEnvironmentStringsW(raw, out, static_cast<DWORD>(std::size(out)))) return {};
    return out;
}

} // namespace

std::wstring normalizePath(const std::wstring& path) {
    if (path.empty()) return {};
    wchar_t full[MAX_PATH * 2]{};
    const DWORD n = GetFullPathNameW(path.c_str(), static_cast<DWORD>(std::size(full)), full, nullptr);
    std::wstring result = (n > 0 && n < std::size(full)) ? std::wstring(full) : path;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](wchar_t c) { return static_cast<wchar_t>(towlower(c)); });
    return result;
}

bool samePath(const std::wstring& a, const std::wstring& b) {
    return normalizePath(a) == normalizePath(b);
}

std::wstring resolveExecutable(const std::wstring& nameOrPath) {
    if (nameOrPath.empty()) return {};

    // Already an absolute path.
    if (nameOrPath.find(L'\\') != std::wstring::npos ||
        nameOrPath.find(L'/')  != std::wstring::npos) {
        return fileExists(nameOrPath) ? nameOrPath : std::wstring();
    }

    if (std::wstring p = queryAppPaths(HKEY_CURRENT_USER, nameOrPath); !p.empty()) return p;
    if (std::wstring p = queryAppPaths(HKEY_LOCAL_MACHINE, nameOrPath); !p.empty()) return p;

    // PATH lookup.
    {
        wchar_t found[MAX_PATH * 2]{};
        if (SearchPathW(nullptr, nameOrPath.c_str(), nullptr,
                        static_cast<DWORD>(std::size(found)), found, nullptr)) {
            if (fileExists(found)) return found;
        }
    }

    // Common install roots, in the order Windows itself would prefer.
    static const wchar_t* kRoots[] = {
        L"%WINDIR%\\",
        L"%WINDIR%\\System32\\",
        L"%ProgramFiles%\\",
        L"%ProgramFiles(x86)%\\",
        L"%LOCALAPPDATA%\\Programs\\",
    };
    for (const wchar_t* rootRaw : kRoots) {
        const std::wstring root = expandEnv(rootRaw);
        if (root.empty()) continue;
        const std::wstring direct = root + nameOrPath;
        if (fileExists(direct)) return direct;
    }

    // A few well-known vendor sub-paths that App Paths sometimes misses.
    static const wchar_t* kKnown[] = {
        L"%ProgramFiles(x86)%\\Microsoft\\Edge\\Application\\msedge.exe",
        L"%ProgramFiles%\\Google\\Chrome\\Application\\chrome.exe",
        L"%ProgramFiles(x86)%\\Google\\Chrome\\Application\\chrome.exe",
        L"%ProgramFiles%\\Mozilla Firefox\\firefox.exe",
        L"%LOCALAPPDATA%\\Programs\\Microsoft VS Code\\Code.exe",
    };
    for (const wchar_t* candidateRaw : kKnown) {
        const std::wstring candidate = expandEnv(candidateRaw);
        if (candidate.empty()) continue;
        if (!fileExists(candidate)) continue;
        const wchar_t* stem = PathFindFileNameW(candidate.c_str());
        if (stem && _wcsicmp(stem, nameOrPath.c_str()) == 0) return candidate;
    }

    return {};
}

std::wstring friendlyName(const std::wstring& exePath) {
    if (exePath.empty()) return {};

    DWORD handle = 0;
    const DWORD size = GetFileVersionInfoSizeW(exePath.c_str(), &handle);
    if (size > 0) {
        std::vector<BYTE> block(size);
        if (GetFileVersionInfoW(exePath.c_str(), 0, size, block.data())) {
            struct LangCodePage { WORD language; WORD codePage; };
            LangCodePage* translate = nullptr;
            UINT translateBytes = 0;

            if (VerQueryValueW(block.data(), L"\\VarFileInfo\\Translation",
                               reinterpret_cast<LPVOID*>(&translate), &translateBytes) &&
                translateBytes >= sizeof(LangCodePage) && translate) {

                wchar_t subBlock[64]{};
                swprintf_s(subBlock, L"\\StringFileInfo\\%04x%04x\\FileDescription",
                           translate[0].language, translate[0].codePage);

                wchar_t* description = nullptr;
                UINT descriptionLen = 0;
                if (VerQueryValueW(block.data(), subBlock,
                                   reinterpret_cast<LPVOID*>(&description), &descriptionLen) &&
                    description && descriptionLen > 1) {
                    std::wstring name(description, descriptionLen - 1);
                    while (!name.empty() && (name.back() == L' ' || name.back() == L'\0')) name.pop_back();
                    if (!name.empty()) return name;
                }
            }
        }
    }

    std::wstring stem = PathFindFileNameW(exePath.c_str());
    if (const size_t dot = stem.rfind(L'.'); dot != std::wstring::npos) stem.erase(dot);
    return stem;
}

} // namespace md::platform
