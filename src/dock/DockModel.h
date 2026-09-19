#pragma once

#include "anim/Animation.h"
#include "core/Settings.h"
#include "platform/WindowWatcher.h"

#include <string>
#include <vector>

namespace md {

enum class DockItemKind {
    App,
    Separator,
    RecycleBin,
};

struct DockItem {
    DockItemKind kind = DockItemKind::App;
    std::wstring path;      // absolute exe path for App items
    std::wstring name;      // tooltip label
    bool pinned  = true;
    bool running = false;
    Bounce bounce;          // launch feedback

    bool interactive() const { return kind != DockItemKind::Separator; }
};

class DockModel {
public:
    // Builds tiles from the pinned list, dropping anything that no longer
    // exists on disk so a stale config can never show a dead tile.
    void rebuild(const Settings& settings);

    // Marks tiles whose app currently has a window, and adds temporary tiles
    // for running apps that are not pinned (the macOS behaviour).
    void syncRunningState(const platform::WindowWatcher& watcher, const Settings& settings);

    void setRecycleBinFull(bool full) { recycleBinFull_ = full; }
    bool recycleBinFull() const { return recycleBinFull_; }

    std::vector<DockItem>&       items()       { return items_; }
    const std::vector<DockItem>& items() const { return items_; }
    size_t size() const { return items_.size(); }

    // Returns true if any bounce is still running.
    bool stepAnimations(float dt);

    int indexOfPath(const std::wstring& path) const;

private:
    void appendUnpinnedRunning(const platform::WindowWatcher& watcher);

    std::vector<DockItem> items_;
    bool recycleBinFull_ = false;
};

} // namespace md
