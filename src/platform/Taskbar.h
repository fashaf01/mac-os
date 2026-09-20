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
// Explorer keeps running throughout, so nothing here is permanent. But a
// process that is *terminated* rather than closed -- a debugger detaching, a
// crash, End Task -- never gets to run its cleanup, and would leave someone
// without a taskbar. So the previous state is handed in and out rather than
// kept in a member: the caller persists it, and the next run can put things
// back even though it is not the process that took them away.
class Taskbar {
public:
    // savedState is in/out: -1 when nothing is stored yet, otherwise the
    // ABS_* state from before the taskbar was first hidden.
    bool hide(int& savedState);
    void restore(int& savedState);

    // Asks the window itself rather than trusting a flag of ours, so this is
    // still right after a previous run was killed mid-flight.
    bool hidden() const;
};

} // namespace md::platform
