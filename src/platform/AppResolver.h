#pragma once
#include <string>

namespace md::platform {

// Turns "msedge.exe" into a full path using the App Paths registry key, the
// PATH, and the usual install roots. Returns "" when the app is not installed.
std::wstring resolveExecutable(const std::wstring& nameOrPath);

// Human-readable name for a dock tile: the FileDescription from the version
// resource when present, otherwise the file stem.
std::wstring friendlyName(const std::wstring& exePath);

// Case-insensitive path comparison with normalisation.
bool samePath(const std::wstring& a, const std::wstring& b);
std::wstring normalizePath(const std::wstring& path);

} // namespace md::platform
