#pragma once

#include <windows.h>

namespace md::platform {

// Takes the Windows taskbar off the screen so the dock can have the bottom
// edge to itself, and puts it back exactly as it was on the way out.
//
// Two steps are needed, not one. Switching the taskbar to auto-hide releases
// the screen space it had reserved, so maximized windows can use the full
// height. Hiding the window then stops it sliding back in when the cursor
// reaches the bottom edge, which would otherwise fight the dock for the same
// few pixels.
//
// Explorer keeps running throughout; nothing here is permanent, and restore()
// is safe to call at any time, including when nothing was hidden.
class Taskbar {
public:
    bool hide();
    void restore();

    bool hidden() const { return hidden_; }

private:
    bool hidden_ = false;
    UINT previousState_ = 0;
};

} // namespace md::platform
