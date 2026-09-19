#include "dock/DockModel.h"
#include "platform/AppResolver.h"
#include "core/Log.h"

#include <windows.h>
#include <algorithm>
#include <iterator>

namespace md {

void DockModel::rebuild(const Settings& settings) {
    // Preserve any bounce already in flight across a rebuild.
    std::vector<std::pair<std::wstring, Bounce>> carried;
    for (const auto& item : items_) {
        if (item.bounce.active()) carried.emplace_back(item.path, item.bounce);
    }

    items_.clear();

    for (const std::wstring& raw : settings.pinned) {
        const std::wstring resolved = platform::resolveExecutable(raw);
        if (resolved.empty()) {
            MD_LOG(L"pinned app not found, skipping: %s", raw.c_str());
            continue;
        }

        DockItem item;
        item.kind   = DockItemKind::App;
        item.path   = resolved;
        item.name   = platform::friendlyName(resolved);
        item.pinned = true;
        items_.push_back(std::move(item));
    }

    if (settings.showRecycleBin) {
        DockItem separator;
        separator.kind = DockItemKind::Separator;
        items_.push_back(std::move(separator));

        DockItem bin;
        bin.kind = DockItemKind::RecycleBin;
        bin.name = L"Recycle Bin";
        items_.push_back(std::move(bin));
    }

    for (auto& item : items_) {
        for (const auto& [path, bounce] : carried) {
            if (!path.empty() && platform::samePath(path, item.path)) item.bounce = bounce;
        }
    }
}

void DockModel::appendUnpinnedRunning(const platform::WindowWatcher& watcher) {
    // A running app with no tile of its own gets a temporary one, inserted
    // before the separator so the Recycle Bin stays at the far end.
    size_t insertAt = items_.size();
    for (size_t i = 0; i < items_.size(); ++i) {
        if (items_[i].kind == DockItemKind::Separator) { insertAt = i; break; }
    }

    for (const std::wstring& path : watcher.runningProcesses()) {
        if (path.empty()) continue;
        if (indexOfPath(path) >= 0) continue;          // already has a tile

        // Explorer owns the desktop and the taskbar; it would otherwise show
        // up permanently even when the user has no folder window open.
        const std::wstring lower = platform::normalizePath(path);
        if (lower.size() >= 13 &&
            lower.compare(lower.size() - 13, 13, L"\\explorer.exe") == 0) {
            bool hasFolderWindow = false;
            for (HWND hwnd : watcher.windowsFor(path)) {
                wchar_t cls[64]{};
                GetClassNameW(hwnd, cls, static_cast<int>(std::size(cls)));
                if (_wcsicmp(cls, L"CabinetWClass") == 0 ||
                    _wcsicmp(cls, L"ExploreWClass") == 0) {
                    hasFolderWindow = true;
                    break;
                }
            }
            if (!hasFolderWindow) continue;
        }

        DockItem item;
        item.kind    = DockItemKind::App;
        item.path    = path;
        item.name    = platform::friendlyName(path);
        item.pinned  = false;
        item.running = true;

        items_.insert(items_.begin() + static_cast<ptrdiff_t>(insertAt), std::move(item));
        ++insertAt;
    }
}

void DockModel::syncRunningState(const platform::WindowWatcher& watcher,
                                 const Settings& settings) {
    // Drop the temporary tiles from the previous pass, then recompute.
    items_.erase(std::remove_if(items_.begin(), items_.end(),
                                [](const DockItem& item) {
                                    return item.kind == DockItemKind::App && !item.pinned;
                                }),
                 items_.end());

    for (auto& item : items_) {
        if (item.kind != DockItemKind::App) continue;
        item.running = watcher.isRunning(item.path);
    }

    if (settings.showRunningApps) appendUnpinnedRunning(watcher);
}

bool DockModel::stepAnimations(float dt) {
    bool active = false;
    for (auto& item : items_) {
        if (item.bounce.step(dt)) active = true;
    }
    return active;
}

int DockModel::indexOfPath(const std::wstring& path) const {
    for (size_t i = 0; i < items_.size(); ++i) {
        if (items_[i].kind == DockItemKind::App && platform::samePath(items_[i].path, path)) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

} // namespace md
