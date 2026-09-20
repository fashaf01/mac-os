#pragma once

#include <string>
#include <vector>

namespace md {

// Plain key=value file at %APPDATA%\MacDock\settings.ini.
// Deliberately hand-rolled: a JSON library would add more binary size than
// the whole settings feature is worth.
struct Settings {
    int   iconSize            = 52;    // logical px, resting size
    float magnification       = 1.75f; // peak scale under the cursor
    bool  magnificationOn     = true;
    float influenceRadius     = 2.4f;  // in multiples of iconSize
    int   itemGap             = 8;
    int   panelPadding        = 8;
    int   bottomMargin        = 8;
    bool  reserveWorkArea     = true;  // keeps maximized windows off the dock
    bool  autoHide            = false;
    bool  showRunningDots     = true;
    bool  showRunningApps     = true;  // temporary tiles for unpinned running apps
    bool  showLabels          = true;
    bool  showRecycleBin      = true;
    bool  backdropBlur        = true;  // real DWM blur behind the panel
    bool  hideWindowsTaskbar  = true;  // the dock replaces it rather than sitting beside it
    std::wstring theme        = L"auto"; // auto | dark | light

    std::vector<std::wstring> pinned;   // absolute exe paths, in dock order

    static std::wstring configDir();
    static std::wstring configPath();

    void load();
    bool save() const;

    // Fills `pinned` with whatever common apps actually exist on this machine.
    void seedDefaultPinsIfEmpty();
};

} // namespace md
