#pragma once

#include "anim/Animation.h"
#include "core/Settings.h"
#include "design/Theme.h"
#include "dock/DockLayout.h"
#include "dock/DockModel.h"
#include "platform/AcrylicBackdrop.h"
#include "platform/AppBar.h"
#include "platform/IconLoader.h"
#include "platform/TrayIcon.h"
#include "platform/WindowWatcher.h"
#include "render/CompositionWindow.h"
#include "render/TextRenderer.h"

#include <shlobj.h>

namespace md {

class DockWindow final : public CompositionWindow {
public:
    bool initialize(GraphicsDevice* gfx);
    void shutdown();

protected:
    void onRender(ID2D1DeviceContext* dc, float wPx, float hPx) override;
    LRESULT onMessage(UINT msg, WPARAM wp, LPARAM lp, bool& handled) override;

private:
    // --- geometry -------------------------------------------------------
    void applyGeometry();
    DockMetrics currentMetrics() const;
    DockLayoutResult buildLayout() const;
    float tooltipHeightPx() const;

    // --- animation ------------------------------------------------------
    void startAnimating();
    void stopAnimating();
    void tickAnimation();

    // --- input ----------------------------------------------------------
    void onMouseMove(int x, int y);
    void onMouseLeave();
    void onClick(int index);
    void showItemMenu(int index, POINT screenPt);
    void showTrayMenu(POINT screenPt);

    // --- state ----------------------------------------------------------
    void scheduleRefresh();
    void refreshNow();
    void reloadSettings();
    void updateBackdrop(const DockLayoutResult& layout);

    // --- painting helpers -----------------------------------------------
    void setBrush(const Color& c);
    void drawSoftShadow(ID2D1DeviceContext* dc, const RectF& rect, float radius);
    void drawPanel(ID2D1DeviceContext* dc, const RectF& panel, float radius);
    void drawItem(ID2D1DeviceContext* dc, size_t index, const ItemLayout& item,
                  const RectF& panel);
    void drawTooltip(ID2D1DeviceContext* dc, const ItemLayout& item, const std::wstring& text);
    void drawPlaceholderIcon(ID2D1DeviceContext* dc, const RectF& rect, const std::wstring& name);

    Settings settings_;
    Theme    theme_;
    DockModel model_;

    platform::IconLoader     icons_;
    platform::WindowWatcher  watcher_;
    platform::AppBar         appBar_;
    platform::TrayIcon       tray_;
    platform::AcrylicBackdrop backdrop_;
    TextRenderer             text_;

    ComPtr<ID2D1SolidColorBrush> brush_;

    Spring magnify_;       // 0 = resting, 1 = fully magnified
    Spring reveal_;        // 1 = shown, 0 = hidden (auto-hide)

    float  scale_      = 1.0f;   // monitor DPI scale
    float  cursorX_    = -1.0e6f;
    int    hoverIndex_ = -1;
    int    pressIndex_ = -1;
    bool   mouseInside_ = false;
    bool   animating_  = false;
    bool   trackingRegistered_ = false;

    ULONG  shellNotifyId_ = 0;
    PIDLIST_ABSOLUTE recycleBinPidl_ = nullptr;

    LARGE_INTEGER perfFreq_{};
    LARGE_INTEGER lastTick_{};
};

} // namespace md
