#include "dock/DockWindow.h"

#include "core/Log.h"
#include "platform/AppResolver.h"
#include "platform/Dpi.h"
#include "platform/Launcher.h"

#include <windowsx.h>
#include <shellapi.h>
#include <algorithm>
#include <cmath>
#include <vector>

namespace md {
namespace {

constexpr const wchar_t* kClassName = L"MacDockWindow";
constexpr const wchar_t* kWindowName = L"MacDock";

constexpr UINT WM_MD_TRAY         = WM_APP + 1;
constexpr UINT WM_MD_APPBAR       = WM_APP + 2;
constexpr UINT WM_MD_SHELLNOTIFY  = WM_APP + 3;
constexpr UINT WM_MD_WATCHER      = WM_APP + 4;

constexpr UINT_PTR IDT_ANIM    = 1;
constexpr UINT_PTR IDT_REFRESH = 2;

// ~120 Hz while something is moving; the timer is killed the moment
// everything settles, so the idle cost of the dock is a message loop
// sitting in GetMessage.
constexpr UINT kAnimIntervalMs    = 8;
constexpr UINT kRefreshDebounceMs = 180;

enum MenuId : UINT {
    kMenuQuit = 100,
    kMenuOpenSettings,
    kMenuReload,
    kMenuOpenItem = 200,
    kMenuUnpin,
    kMenuPin,
    kMenuShowInFolder,
    kMenuQuitApp,
    kMenuOpenBin = 210,
    kMenuEmptyBin,
};

D2D1_COLOR_F toD2D(const Color& c) {
    return D2D1::ColorF(c.r, c.g, c.b, c.a);
}

} // namespace

// ---------------------------------------------------------------- lifecycle

bool DockWindow::initialize(GraphicsDevice* gfx) {
    QueryPerformanceFrequency(&perfFreq_);
    QueryPerformanceCounter(&lastTick_);

    settings_.load();
    settings_.seedDefaultPinsIfEmpty();
    settings_.save();

    theme_ = Theme::resolve(settings_.theme);
    auditContrast(theme_);

    icons_.attach(gfx);
    model_.rebuild(settings_);

    const DWORD style   = WS_POPUP;
    const DWORD exStyle = WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE
                        | WS_EX_TOPMOST | WS_EX_NOREDIRECTIONBITMAP;

    if (!create(gfx, kClassName, kWindowName, style, exStyle, 0, 0, 800, 120)) {
        return false;
    }

    if (FAILED(gfx->d2d()->CreateSolidColorBrush(D2D1::ColorF(0, 0.0f), brush_.GetAddressOf()))) {
        MD_LOG(L"CreateSolidColorBrush failed");
        return false;
    }

    text_.create(gfx, 12.0f);

    magnify_.configure(300.0f, 30.0f);
    magnify_.snapTo(0.0f);
    reveal_.configure(220.0f, 26.0f);
    reveal_.snapTo(settings_.autoHide ? 0.0f : 1.0f);

    if (settings_.reserveWorkArea) {
        appBar_.registerBar(hwnd_, WM_MD_APPBAR);
    }
    tray_.add(hwnd_, WM_MD_TRAY, L"MacDock");

    if (settings_.backdropBlur) {
        if (backdrop_.create(hwnd_)) {
            backdrop_.setTint(theme_.panelFill);
        }
    }

    watcher_.start([this] {
        // Runs on our own thread from the WinEvent hook. Post rather than
        // rescan: a single app launch can emit dozens of window events.
        if (hwnd_) PostMessageW(hwnd_, WM_MD_WATCHER, 0, 0);
    });
    model_.syncRunningState(watcher_, settings_);

    // Recycle Bin state, without polling: the shell tells us when it changes.
    if (SUCCEEDED(SHGetKnownFolderIDList(FOLDERID_RecycleBinFolder, 0, nullptr, &recycleBinPidl_))) {
        SHChangeNotifyEntry entry{};
        entry.pidl = recycleBinPidl_;
        entry.fRecursive = TRUE;

        shellNotifyId_ = SHChangeNotifyRegister(
            hwnd_,
            SHCNRF_ShellLevel | SHCNRF_InterruptLevel | SHCNRF_NewDelivery,
            SHCNE_UPDATEDIR | SHCNE_CREATE | SHCNE_DELETE | SHCNE_RENAMEITEM |
            SHCNE_MKDIR | SHCNE_RMDIR | SHCNE_UPDATEITEM,
            WM_MD_SHELLNOTIFY, 1, &entry);
    }
    model_.setRecycleBinFull(platform::recycleBinHasItems());

    applyGeometry();
    ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
    invalidate();
    renderNow();

    MD_LOG(L"dock ready with %zu item(s)", model_.size());
    return true;
}

void DockWindow::shutdown() {
    stopAnimating();
    watcher_.stop();

    if (shellNotifyId_) {
        SHChangeNotifyDeregister(shellNotifyId_);
        shellNotifyId_ = 0;
    }
    if (recycleBinPidl_) {
        CoTaskMemFree(recycleBinPidl_);
        recycleBinPidl_ = nullptr;
    }

    backdrop_.destroy();
    tray_.remove();
    appBar_.unregisterBar();

    icons_.clear();
    text_.destroy();
    brush_.Reset();

    destroy();
}

// ----------------------------------------------------------------- geometry

DockMetrics DockWindow::currentMetrics() const {
    DockMetrics m;
    m.iconSize     = static_cast<float>(settings_.iconSize) * scale_;
    m.gap          = static_cast<float>(settings_.itemGap) * scale_;
    m.padding      = static_cast<float>(settings_.panelPadding) * scale_;
    m.dotBand      = settings_.showRunningDots ? 7.0f * scale_ : 2.0f * scale_;
    m.bottomMargin = static_cast<float>(settings_.bottomMargin) * scale_;
    m.maxScale     = settings_.magnificationOn ? settings_.magnification : 1.0f;
    m.influence    = settings_.influenceRadius;
    m.panelRadius  = 18.0f * scale_;
    return m;
}

float DockWindow::tooltipHeightPx() const {
    if (!settings_.showLabels) return 0.0f;
    return 24.0f * scale_;
}

DockLayoutResult DockWindow::buildLayout() const {
    const DockMetrics m = currentMetrics();

    std::vector<float> bounces;
    bounces.reserve(model_.size());
    for (const auto& item : model_.items()) {
        bounces.push_back(item.bounce.offset(m.iconSize));
    }

    DockLayoutResult layout = computeDockLayout(
        model_.size(), m,
        static_cast<float>(widthPx()), static_cast<float>(heightPx()),
        cursorX_, magnify_.value(),
        bounces.empty() ? nullptr : bounces.data());

    // Auto-hide slides the whole strip down past the bottom edge.
    const float hidden = 1.0f - reveal_.value();
    if (hidden > 0.0001f) {
        const float drop = (layout.panelHeight + m.bottomMargin) * hidden;
        layout.panel = layout.panel.offset(0.0f, drop);
        for (auto& item : layout.items) {
            item.icon = item.icon.offset(0.0f, drop);
            item.hit  = item.hit.offset(0.0f, drop);
        }
    }

    return layout;
}

void DockWindow::applyGeometry() {
    scale_ = platform::scaleForWindow(hwnd_);
    text_.setSize(12.0f * scale_);

    const DockMetrics m = currentMetrics();
    const int windowH = static_cast<int>(std::ceil(dockWindowHeight(m, tooltipHeightPx())));

    RECT monitorRect{ 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };
    if (HMONITOR monitor = MonitorFromWindow(hwnd_, MONITOR_DEFAULTTOPRIMARY)) {
        MONITORINFO mi{};
        mi.cbSize = sizeof(mi);
        if (GetMonitorInfoW(monitor, &mi)) monitorRect = mi.rcMonitor;
    }

    const int width = monitorRect.right - monitorRect.left;

    SetWindowPos(hwnd_, HWND_TOPMOST,
                 monitorRect.left, monitorRect.bottom - windowH,
                 width, windowH,
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);

    resizeSurface(width, windowH);

    if (appBar_.registered() && !settings_.autoHide) {
        const int reserve = static_cast<int>(std::ceil(dockPanelHeight(m) + m.bottomMargin));
        appBar_.reserveBottom(hwnd_, reserve);
    }

    invalidate();
}

// ---------------------------------------------------------------- animation

void DockWindow::startAnimating() {
    if (animating_) return;
    animating_ = true;
    QueryPerformanceCounter(&lastTick_);
    SetTimer(hwnd_, IDT_ANIM, kAnimIntervalMs, nullptr);
}

void DockWindow::stopAnimating() {
    if (!animating_) return;
    animating_ = false;
    KillTimer(hwnd_, IDT_ANIM);
}

void DockWindow::tickAnimation() {
    LARGE_INTEGER now{};
    QueryPerformanceCounter(&now);

    float dt = 0.0f;
    if (perfFreq_.QuadPart > 0) {
        dt = static_cast<float>(static_cast<double>(now.QuadPart - lastTick_.QuadPart) /
                                static_cast<double>(perfFreq_.QuadPart));
    }
    lastTick_ = now;
    if (dt <= 0.0f) return;

    bool busy = false;
    busy |= magnify_.step(dt);
    busy |= reveal_.step(dt);
    busy |= model_.stepAnimations(dt);

    invalidate();
    if (!busy) stopAnimating();
}

// -------------------------------------------------------------------- input

void DockWindow::onMouseMove(int x, int y) {
    cursorX_ = static_cast<float>(x);

    if (!trackingRegistered_) {
        TRACKMOUSEEVENT tme{};
        tme.cbSize    = sizeof(tme);
        tme.dwFlags   = TME_LEAVE;
        tme.hwndTrack = hwnd_;
        TrackMouseEvent(&tme);
        trackingRegistered_ = true;
    }
    mouseInside_ = true;

    if (settings_.autoHide) reveal_.setTarget(1.0f);
    magnify_.setTarget(1.0f);

    const DockLayoutResult layout = buildLayout();
    const int hit = hitTestDock(layout, static_cast<float>(x), static_cast<float>(y));
    const int hovered = (hit >= 0 && model_.items()[static_cast<size_t>(hit)].interactive())
                        ? hit : -1;

    if (hovered != hoverIndex_) hoverIndex_ = hovered;

    startAnimating();
    invalidate();
}

void DockWindow::onMouseLeave() {
    trackingRegistered_ = false;
    mouseInside_ = false;
    hoverIndex_ = -1;
    pressIndex_ = -1;
    cursorX_ = -1.0e6f;

    magnify_.setTarget(0.0f);
    if (settings_.autoHide) reveal_.setTarget(0.0f);

    startAnimating();
    invalidate();
}

void DockWindow::onClick(int index) {
    if (index < 0 || static_cast<size_t>(index) >= model_.size()) return;
    DockItem& item = model_.items()[static_cast<size_t>(index)];

    switch (item.kind) {
    case DockItemKind::RecycleBin:
        platform::openRecycleBin();
        break;

    case DockItemKind::Separator:
        break;

    case DockItemKind::App: {
        const std::vector<HWND> windows = watcher_.windowsFor(item.path);
        if (!windows.empty()) {
            platform::activateNext(windows, watcher_.foreground());
        } else if (platform::launch(item.path)) {
            item.bounce.start();
            startAnimating();
            scheduleRefresh();
        }
        break;
    }
    }
    invalidate();
}

void DockWindow::showItemMenu(int index, POINT screenPt) {
    if (index < 0 || static_cast<size_t>(index) >= model_.size()) return;
    const DockItem& item = model_.items()[static_cast<size_t>(index)];
    if (!item.interactive()) return;

    HMENU menu = CreatePopupMenu();
    if (!menu) return;

    if (item.kind == DockItemKind::RecycleBin) {
        AppendMenuW(menu, MF_STRING, kMenuOpenBin, L"Open");
        AppendMenuW(menu, MF_STRING | (model_.recycleBinFull() ? 0u : MF_GRAYED),
                    kMenuEmptyBin, L"Empty Recycle Bin…");
    } else {
        AppendMenuW(menu, MF_STRING, kMenuOpenItem,
                    item.running ? L"Show" : L"Open");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, kMenuShowInFolder, L"Show in File Explorer");
        AppendMenuW(menu, MF_STRING, item.pinned ? kMenuUnpin : kMenuPin,
                    item.pinned ? L"Remove from Dock" : L"Keep in Dock");
    }

    // A non-activating window has to take the foreground itself or the menu
    // will not dismiss when the user clicks elsewhere.
    SetForegroundWindow(hwnd_);
    const int choice = static_cast<int>(TrackPopupMenuEx(
        menu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON | TPM_BOTTOMALIGN,
        screenPt.x, screenPt.y, hwnd_, nullptr));
    DestroyMenu(menu);
    PostMessageW(hwnd_, WM_NULL, 0, 0);

    switch (choice) {
    case kMenuOpenItem:
    case kMenuOpenBin:
        onClick(index);
        break;

    case kMenuEmptyBin:
        platform::emptyRecycleBin(hwnd_);
        break;

    case kMenuShowInFolder: {
        std::wstring arg = L"/select,\"" + item.path + L"\"";
        ShellExecuteW(nullptr, L"open", L"explorer.exe", arg.c_str(), nullptr, SW_SHOWNORMAL);
        break;
    }

    case kMenuUnpin: {
        const std::wstring path = item.path;
        settings_.pinned.erase(
            std::remove_if(settings_.pinned.begin(), settings_.pinned.end(),
                           [&](const std::wstring& p) { return platform::samePath(p, path); }),
            settings_.pinned.end());
        settings_.save();
        reloadSettings();
        break;
    }

    case kMenuPin: {
        settings_.pinned.push_back(item.path);
        settings_.save();
        reloadSettings();
        break;
    }

    default:
        break;
    }
}

void DockWindow::showTrayMenu(POINT screenPt) {
    HMENU menu = CreatePopupMenu();
    if (!menu) return;

    AppendMenuW(menu, MF_STRING, kMenuOpenSettings, L"Open settings file…");
    AppendMenuW(menu, MF_STRING, kMenuReload,       L"Reload settings");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kMenuQuit,         L"Quit MacDock");

    SetForegroundWindow(hwnd_);
    const int choice = static_cast<int>(TrackPopupMenuEx(
        menu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON,
        screenPt.x, screenPt.y, hwnd_, nullptr));
    DestroyMenu(menu);
    PostMessageW(hwnd_, WM_NULL, 0, 0);

    switch (choice) {
    case kMenuOpenSettings:
        platform::openShellLocation(Settings::configPath());
        break;
    case kMenuReload:
        reloadSettings();
        break;
    case kMenuQuit:
        PostMessageW(hwnd_, WM_CLOSE, 0, 0);
        break;
    default:
        break;
    }
}

// -------------------------------------------------------------------- state

void DockWindow::scheduleRefresh() {
    SetTimer(hwnd_, IDT_REFRESH, kRefreshDebounceMs, nullptr);
}

void DockWindow::refreshNow() {
    KillTimer(hwnd_, IDT_REFRESH);
    watcher_.refresh();
    model_.syncRunningState(watcher_, settings_);
    invalidate();
}

void DockWindow::reloadSettings() {
    settings_.load();
    settings_.seedDefaultPinsIfEmpty();

    theme_ = Theme::resolve(settings_.theme);
    if (backdrop_.available()) backdrop_.setTint(theme_.panelFill);

    model_.rebuild(settings_);
    model_.syncRunningState(watcher_, settings_);

    reveal_.snapTo(settings_.autoHide && !mouseInside_ ? 0.0f : 1.0f);

    applyGeometry();
    invalidate();
}

void DockWindow::updateBackdrop(const DockLayoutResult& layout) {
    if (!backdrop_.available()) return;

    if (model_.size() == 0 || reveal_.value() < 0.02f) {
        backdrop_.setVisible(false);
        return;
    }

    RECT client{};
    GetWindowRect(hwnd_, &client);

    RECT bounds{
        client.left + static_cast<LONG>(std::floor(layout.panel.left)),
        client.top  + static_cast<LONG>(std::floor(layout.panel.top)),
        client.left + static_cast<LONG>(std::ceil(layout.panel.right)),
        client.top  + static_cast<LONG>(std::ceil(layout.panel.bottom)),
    };

    backdrop_.setBounds(bounds, static_cast<int>(currentMetrics().panelRadius));
    backdrop_.setVisible(true);
}

// ----------------------------------------------------------------- painting

void DockWindow::setBrush(const Color& c) {
    if (brush_) brush_->SetColor(toD2D(c));
}

void DockWindow::drawSoftShadow(ID2D1DeviceContext* dc, const RectF& rect, float radius) {
    // A stack of expanding rings approximates a Gaussian drop shadow for a
    // fraction of the cost of a real blur effect graph, and at dock scale the
    // difference is not visible.
    //
    // Rings, not filled rectangles: the panel above is translucent, so a
    // filled shadow would show through and muddy the glass instead of sitting
    // behind it.
    constexpr int kLayers = 10;
    const float spread = 10.0f * scale_;
    const float ring = (spread / kLayers) * 2.2f;   // overlap, so no banding

    for (int i = kLayers; i >= 1; --i) {
        const float t = static_cast<float>(i) / kLayers;
        const float grow = spread * t;
        const float alpha = theme_.panelShadow.a * (1.0f - t) * (1.0f - t) * 0.9f;

        setBrush(theme_.panelShadow.withAlpha(alpha));
        const RectF r = rect.inflated(grow, grow * 0.6f).offset(0.0f, grow * 0.25f);
        dc->DrawRoundedRectangle(
            D2D1::RoundedRect(D2D1::RectF(r.left, r.top, r.right, r.bottom),
                              radius + grow, radius + grow),
            brush_.Get(), ring);
    }
}

void DockWindow::drawPanel(ID2D1DeviceContext* dc, const RectF& panel, float radius) {
    const auto rounded = D2D1::RoundedRect(
        D2D1::RectF(panel.left, panel.top, panel.right, panel.bottom), radius, radius);

    // The painted glass. When the acrylic backdrop window is live it is
    // already supplying the tint and blur, so we only lay down the edge
    // treatment here and skip the fill.
    if (!backdrop_.available()) {
        setBrush(theme_.panelFill);
        dc->FillRoundedRectangle(rounded, brush_.Get());
    }

    // Inner top highlight: the single detail that makes a flat translucent
    // rectangle read as a pane of glass rather than a grey box.
    setBrush(theme_.panelHighlight);
    const float inset = 1.0f * scale_;
    dc->DrawRoundedRectangle(
        D2D1::RoundedRect(D2D1::RectF(panel.left + inset, panel.top + inset,
                                      panel.right - inset, panel.top + radius * 2.0f),
                          radius - inset, radius - inset),
        brush_.Get(), 1.0f * scale_);

    setBrush(theme_.panelStroke);
    dc->DrawRoundedRectangle(rounded, brush_.Get(), 1.0f * scale_);
}

void DockWindow::drawPlaceholderIcon(ID2D1DeviceContext* dc, const RectF& rect,
                                     const std::wstring& name) {
    setBrush(theme_.accent.withAlpha(0.85f));
    const float r = rect.width() * 0.22f;
    dc->FillRoundedRectangle(
        D2D1::RoundedRect(D2D1::RectF(rect.left, rect.top, rect.right, rect.bottom), r, r),
        brush_.Get());

    if (!name.empty()) {
        setBrush(Color::rgba8(255, 255, 255, 0.95f));
        text_.draw(dc, name.substr(0, 1), rect, brush_.Get());
    }
}

void DockWindow::drawItem(ID2D1DeviceContext* dc, size_t index, const ItemLayout& item,
                          const RectF& panel) {
    const DockItem& data = model_.items()[index];

    if (data.kind == DockItemKind::Separator) {
        setBrush(theme_.separator);
        const float x = std::round(item.hit.centerX());
        const float inset = panel.height() * 0.18f;
        dc->DrawLine(D2D1::Point2F(x, panel.top + inset),
                     D2D1::Point2F(x, panel.bottom - inset),
                     brush_.Get(), 1.0f * scale_);
        return;
    }

    RectF iconRect = item.icon;

    // Press feedback: a small inward nudge, released on mouse-up.
    if (pressIndex_ == static_cast<int>(index)) {
        const float shrink = iconRect.width() * 0.05f;
        iconRect = iconRect.inflated(-shrink, -shrink).offset(0.0f, shrink);
    }

    ID2D1Bitmap1* bitmap = (data.kind == DockItemKind::RecycleBin)
                         ? icons_.recycleBinIcon(model_.recycleBinFull())
                         : icons_.iconForPath(data.path);

    if (bitmap) {
        dc->DrawBitmap(bitmap,
                       D2D1::RectF(iconRect.left, iconRect.top, iconRect.right, iconRect.bottom),
                       1.0f, D2D1_INTERPOLATION_MODE_HIGH_QUALITY_CUBIC);
    } else {
        drawPlaceholderIcon(dc, iconRect, data.name);
    }

    if (settings_.showRunningDots && data.running) {
        const float radius = 2.0f * scale_;
        const float cy = panel.bottom - currentMetrics().padding - radius - 1.0f * scale_;
        setBrush(theme_.runningDot);
        dc->FillEllipse(D2D1::Ellipse(D2D1::Point2F(item.icon.centerX(), cy), radius, radius),
                        brush_.Get());
    }
}

void DockWindow::drawTooltip(ID2D1DeviceContext* dc, const ItemLayout& item,
                             const std::wstring& label) {
    if (label.empty()) return;

    const float padX = 9.0f * scale_;
    const float padY = 4.0f * scale_;
    const Vec2 textSize = text_.measure(label, static_cast<float>(widthPx()));
    if (textSize.x <= 0.0f) return;

    const float w = textSize.x + padX * 2.0f;
    const float h = textSize.y + padY * 2.0f;

    float left = item.icon.centerX() - w * 0.5f;
    left = clampf(left, 4.0f * scale_, static_cast<float>(widthPx()) - w - 4.0f * scale_);
    const float bottom = item.icon.top - 8.0f * scale_;
    const RectF box{ left, bottom - h, left + w, bottom };

    const auto rounded = D2D1::RoundedRect(
        D2D1::RectF(box.left, box.top, box.right, box.bottom),
        theme_.tooltipRadius * scale_, theme_.tooltipRadius * scale_);

    setBrush(theme_.panelShadow.withAlpha(theme_.panelShadow.a * 0.35f));
    dc->FillRoundedRectangle(
        D2D1::RoundedRect(D2D1::RectF(box.left, box.top + scale_, box.right, box.bottom + scale_),
                          theme_.tooltipRadius * scale_, theme_.tooltipRadius * scale_),
        brush_.Get());

    setBrush(theme_.tooltipFill);
    dc->FillRoundedRectangle(rounded, brush_.Get());

    setBrush(theme_.tooltipStroke);
    dc->DrawRoundedRectangle(rounded, brush_.Get(), 1.0f * scale_);

    setBrush(theme_.tooltipText);
    text_.draw(dc, label, box, brush_.Get());
}

void DockWindow::onRender(ID2D1DeviceContext* dc, float wPx, float hPx) {
    (void)wPx;
    (void)hPx;
    if (model_.size() == 0) return;

    const DockLayoutResult layout = buildLayout();
    const float radius = currentMetrics().panelRadius;

    // Position the blur pane before painting, so the glass and its contents
    // land on screen in the same compositor frame.
    updateBackdrop(layout);

    drawSoftShadow(dc, layout.panel, radius);
    drawPanel(dc, layout.panel, radius);

    for (size_t i = 0; i < layout.items.size(); ++i) {
        drawItem(dc, i, layout.items[i], layout.panel);
    }

    if (settings_.showLabels && hoverIndex_ >= 0 &&
        static_cast<size_t>(hoverIndex_) < layout.items.size()) {
        const DockItem& hovered = model_.items()[static_cast<size_t>(hoverIndex_)];
        drawTooltip(dc, layout.items[static_cast<size_t>(hoverIndex_)], hovered.name);
    }
}

// ---------------------------------------------------------------- messages

LRESULT DockWindow::onMessage(UINT msg, WPARAM wp, LPARAM lp, bool& handled) {
    handled = true;

    switch (msg) {
    case WM_NCHITTEST: {
        POINT pt{ GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        ScreenToClient(hwnd_, &pt);
        const float x = static_cast<float>(pt.x);
        const float y = static_cast<float>(pt.y);

        // Auto-hide keeps a thin reveal strip alive along the bottom edge.
        if (settings_.autoHide && reveal_.value() < 0.5f) {
            return (y >= heightPx() - 2) ? HTCLIENT : HTTRANSPARENT;
        }

        const DockLayoutResult layout = buildLayout();
        if (layout.panel.contains(x, y)) return HTCLIENT;
        if (hitTestDock(layout, x, y) >= 0) return HTCLIENT;
        return HTTRANSPARENT;   // every other pixel belongs to whatever is below
    }

    case WM_MOUSEMOVE:
        onMouseMove(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
        return 0;

    case WM_MOUSELEAVE:
        onMouseLeave();
        return 0;

    case WM_LBUTTONDOWN: {
        const DockLayoutResult layout = buildLayout();
        pressIndex_ = hitTestDock(layout, static_cast<float>(GET_X_LPARAM(lp)),
                                  static_cast<float>(GET_Y_LPARAM(lp)));
        invalidate();
        return 0;
    }

    case WM_LBUTTONUP: {
        const DockLayoutResult layout = buildLayout();
        const int released = hitTestDock(layout, static_cast<float>(GET_X_LPARAM(lp)),
                                         static_cast<float>(GET_Y_LPARAM(lp)));
        const int pressed = pressIndex_;
        pressIndex_ = -1;
        if (pressed >= 0 && pressed == released) onClick(released);
        invalidate();
        return 0;
    }

    case WM_RBUTTONUP: {
        const DockLayoutResult layout = buildLayout();
        const int index = hitTestDock(layout, static_cast<float>(GET_X_LPARAM(lp)),
                                      static_cast<float>(GET_Y_LPARAM(lp)));
        if (index >= 0) {
            POINT screenPt{ GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
            ClientToScreen(hwnd_, &screenPt);
            showItemMenu(index, screenPt);
        }
        return 0;
    }

    case WM_TIMER:
        if (wp == IDT_ANIM)    { tickAnimation(); return 0; }
        if (wp == IDT_REFRESH) { refreshNow();    return 0; }
        break;

    case WM_MD_WATCHER:
        scheduleRefresh();
        return 0;

    case WM_MD_SHELLNOTIFY: {
        // SHCNRF_NewDelivery hands us a shared-memory handle we must lock.
        LPITEMIDLIST* pidls = nullptr;
        LONG event = 0;
        if (HANDLE lock = SHChangeNotification_Lock(reinterpret_cast<HANDLE>(wp),
                                                    static_cast<DWORD>(lp), &pidls, &event)) {
            SHChangeNotification_Unlock(lock);
        }
        model_.setRecycleBinFull(platform::recycleBinHasItems());
        invalidate();
        return 0;
    }

    case WM_MD_TRAY:
        switch (LOWORD(lp)) {
        case WM_RBUTTONUP:
        case WM_CONTEXTMENU: {
            POINT pt{};
            GetCursorPos(&pt);
            showTrayMenu(pt);
            return 0;
        }
        default:
            break;
        }
        break;

    case WM_MD_APPBAR:
        if (wp == ABN_POSCHANGED || wp == ABN_FULLSCREENAPP) {
            applyGeometry();
            return 0;
        }
        break;

    case WM_SETTINGCHANGE:
        // Catches the light/dark switch in Windows Settings.
        if (settings_.theme == L"auto") {
            const Theme updated = Theme::resolve(settings_.theme);
            if (updated.dark != theme_.dark) {
                theme_ = updated;
                if (backdrop_.available()) backdrop_.setTint(theme_.panelFill);
                auditContrast(theme_);
                invalidate();
            }
        }
        break;

    case WM_DPICHANGED:
    case WM_DISPLAYCHANGE:
        applyGeometry();
        return 0;

    case WM_CLOSE:
        PostQuitMessage(0);
        return 0;

    default:
        break;
    }

    handled = false;
    return 0;
}

} // namespace md
