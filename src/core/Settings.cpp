#include "core/Settings.h"
#include "core/Geometry.h"
#include "core/Log.h"
#include "platform/AppResolver.h"

#include <windows.h>
#include <shlobj.h>
#include <cstdio>
#include <cwchar>
#include <iterator>
#include <string>

namespace md {
namespace {

std::wstring trim(const std::wstring& s) {
    size_t a = s.find_first_not_of(L" \t\r\n");
    if (a == std::wstring::npos) return L"";
    size_t b = s.find_last_not_of(L" \t\r\n");
    return s.substr(a, b - a + 1);
}

int toInt(const std::wstring& s, int fallback) {
    wchar_t* end = nullptr;
    const long v = wcstol(s.c_str(), &end, 10);
    return (end && end != s.c_str()) ? static_cast<int>(v) : fallback;
}

float toFloat(const std::wstring& s, float fallback) {
    wchar_t* end = nullptr;
    const double v = wcstod(s.c_str(), &end);
    return (end && end != s.c_str()) ? static_cast<float>(v) : fallback;
}

bool toBool(const std::wstring& s, bool fallback) {
    if (s == L"1" || s == L"true" || s == L"yes" || s == L"on")  return true;
    if (s == L"0" || s == L"false" || s == L"no" || s == L"off") return false;
    return fallback;
}

} // namespace

std::wstring Settings::configDir() {
    PWSTR base = nullptr;
    std::wstring dir;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &base))) {
        dir.assign(base);
        dir += L"\\MacDock";
        CoTaskMemFree(base);
        CreateDirectoryW(dir.c_str(), nullptr);
    }
    return dir;
}

std::wstring Settings::configPath() {
    const std::wstring dir = configDir();
    return dir.empty() ? std::wstring() : dir + L"\\settings.ini";
}

void Settings::load() {
    const std::wstring path = configPath();
    if (path.empty()) return;

    FILE* f = nullptr;
    if (_wfopen_s(&f, path.c_str(), L"rt, ccs=UTF-8") != 0 || !f) return;

    pinned.clear();

    wchar_t raw[1024];
    while (fgetws(raw, static_cast<int>(std::size(raw)), f)) {
        std::wstring line = trim(raw);
        if (line.empty() || line[0] == L'#' || line[0] == L';') continue;

        const size_t eq = line.find(L'=');
        if (eq == std::wstring::npos) continue;

        const std::wstring key = trim(line.substr(0, eq));
        const std::wstring val = trim(line.substr(eq + 1));

        if      (key == L"iconSize")          iconSize        = toInt(val, iconSize);
        else if (key == L"magnification")     magnification   = toFloat(val, magnification);
        else if (key == L"magnificationOn")   magnificationOn = toBool(val, magnificationOn);
        else if (key == L"influenceRadius")   influenceRadius = toFloat(val, influenceRadius);
        else if (key == L"itemGap")           itemGap         = toInt(val, itemGap);
        else if (key == L"panelPadding")      panelPadding    = toInt(val, panelPadding);
        else if (key == L"bottomMargin")      bottomMargin    = toInt(val, bottomMargin);
        else if (key == L"reserveWorkArea")   reserveWorkArea = toBool(val, reserveWorkArea);
        else if (key == L"autoHide")          autoHide        = toBool(val, autoHide);
        else if (key == L"showRunningDots")   showRunningDots = toBool(val, showRunningDots);
        else if (key == L"showRunningApps")   showRunningApps = toBool(val, showRunningApps);
        else if (key == L"showLabels")        showLabels      = toBool(val, showLabels);
        else if (key == L"showRecycleBin")    showRecycleBin  = toBool(val, showRecycleBin);
        else if (key == L"backdropBlur")      backdropBlur    = toBool(val, backdropBlur);
        else if (key == L"hideWindowsTaskbar") hideWindowsTaskbar = toBool(val, hideWindowsTaskbar);
        else if (key == L"theme")             theme           = val;
        else if (key == L"pin" && !val.empty()) pinned.push_back(val);
    }
    fclose(f);

    // Guard rails so a hand-edited file can never produce an unusable dock.
    iconSize        = (clampi(iconSize, 24, 128));
    magnification   = magnification < 1.0f ? 1.0f : (magnification > 3.0f ? 3.0f : magnification);
    influenceRadius = influenceRadius < 0.5f ? 0.5f : (influenceRadius > 8.0f ? 8.0f : influenceRadius);
    itemGap         = (clampi(itemGap, 0, 40));
    panelPadding    = (clampi(panelPadding, 0, 40));
    bottomMargin    = (clampi(bottomMargin, 0, 200));

    MD_LOG(L"settings loaded: %zu pinned item(s), iconSize=%d, theme=%s",
           pinned.size(), iconSize, theme.c_str());
}

bool Settings::save() const {
    const std::wstring path = configPath();
    if (path.empty()) return false;

    FILE* f = nullptr;
    if (_wfopen_s(&f, path.c_str(), L"wt, ccs=UTF-8") != 0 || !f) return false;

    fwprintf(f, L"# MacDock settings. Restart the dock after editing.\n\n");
    fwprintf(f, L"theme            = %s\n", theme.c_str());
    fwprintf(f, L"iconSize         = %d\n", iconSize);
    fwprintf(f, L"magnificationOn  = %s\n", magnificationOn ? L"true" : L"false");
    fwprintf(f, L"magnification    = %.2f\n", magnification);
    fwprintf(f, L"influenceRadius  = %.2f\n", influenceRadius);
    fwprintf(f, L"itemGap          = %d\n", itemGap);
    fwprintf(f, L"panelPadding     = %d\n", panelPadding);
    fwprintf(f, L"bottomMargin     = %d\n", bottomMargin);
    fwprintf(f, L"reserveWorkArea  = %s\n", reserveWorkArea ? L"true" : L"false");
    fwprintf(f, L"autoHide         = %s\n", autoHide ? L"true" : L"false");
    fwprintf(f, L"showRunningDots  = %s\n", showRunningDots ? L"true" : L"false");
    fwprintf(f, L"showRunningApps  = %s\n", showRunningApps ? L"true" : L"false");
    fwprintf(f, L"showLabels       = %s\n", showLabels ? L"true" : L"false");
    fwprintf(f, L"showRecycleBin   = %s\n", showRecycleBin ? L"true" : L"false");
    fwprintf(f, L"backdropBlur     = %s\n", backdropBlur ? L"true" : L"false");
    fwprintf(f, L"hideWindowsTaskbar = %s\n", hideWindowsTaskbar ? L"true" : L"false");
    fwprintf(f, L"\n# Dock order, one absolute path per line.\n");
    for (const auto& p : pinned) {
        fwprintf(f, L"pin = %s\n", p.c_str());
    }
    fclose(f);
    return true;
}

void Settings::seedDefaultPinsIfEmpty() {
    if (!pinned.empty()) return;

    // Only apps that are genuinely present get pinned, so the first launch
    // never shows a broken tile.
    static const wchar_t* kCandidates[] = {
        L"explorer.exe",
        L"msedge.exe",
        L"chrome.exe",
        L"firefox.exe",
        L"WindowsTerminal.exe",
        L"Code.exe",
        L"notepad.exe",
        L"mspaint.exe",
        L"taskmgr.exe",
    };

    for (const wchar_t* name : kCandidates) {
        std::wstring resolved = platform::resolveExecutable(name);
        if (!resolved.empty()) pinned.push_back(resolved);
    }

    MD_LOG(L"seeded %zu default pin(s)", pinned.size());
}

} // namespace md
