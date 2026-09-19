#pragma once

#include <windows.h>
#include <string>
#include <vector>

namespace md::platform {

// Starts an application, or brings its existing windows forward.
bool launch(const std::wstring& path);
bool openShellLocation(const std::wstring& verbOrPath);

// Raises a window past the foreground lock that Windows applies to a
// non-activating caller like the dock.
bool activateWindow(HWND hwnd);

// Cycles through an app's windows on repeated clicks, the way the macOS dock
// cycles through an app's documents.
bool activateNext(const std::vector<HWND>& windows, HWND currentForeground);

void openRecycleBin();
bool emptyRecycleBin(HWND owner);
bool recycleBinHasItems();

} // namespace md::platform
