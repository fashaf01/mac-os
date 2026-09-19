#include "platform/AcrylicBackdrop.h"
#include "core/Log.h"

namespace md::platform {
namespace {

enum AccentState : int {
    ACCENT_DISABLED                  = 0,
    ACCENT_ENABLE_BLURBEHIND         = 3,
    ACCENT_ENABLE_ACRYLICBLURBEHIND  = 4,
};

struct ACCENT_POLICY {
    int          accentState;
    int          accentFlags;
    unsigned int gradientColor;   // 0xAABBGGRR
    int          animationId;
};

struct WINDOWCOMPOSITIONATTRIBDATA {
    int    attribute;             // 19 == WCA_ACCENT_POLICY
    PVOID  data;
    SIZE_T size;
};

using SetWindowCompositionAttributeFn = BOOL (WINAPI*)(HWND, WINDOWCOMPOSITIONATTRIBDATA*);

SetWindowCompositionAttributeFn resolveSetter() {
    static SetWindowCompositionAttributeFn fn = [] {
        HMODULE user32 = GetModuleHandleW(L"user32.dll");
        return user32 ? reinterpret_cast<SetWindowCompositionAttributeFn>(
                            GetProcAddress(user32, "SetWindowCompositionAttribute"))
                      : nullptr;
    }();
    return fn;
}

unsigned int toAbgr(const Color& c) {
    const auto ch = [](float v) {
        const int i = static_cast<int>(v * 255.0f + 0.5f);
        return static_cast<unsigned int>(i < 0 ? 0 : (i > 255 ? 255 : i));
    };
    return (ch(c.a) << 24) | (ch(c.b) << 16) | (ch(c.g) << 8) | ch(c.r);
}

constexpr const wchar_t* kClassName = L"MacDockBackdrop";

LRESULT CALLBACK backdropProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_ERASEBKGND:
        return 1;                       // DWM owns every pixel here
    case WM_NCHITTEST:
        return HTTRANSPARENT;           // never steals a click from the dock
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        BeginPaint(hwnd, &ps);
        EndPaint(hwnd, &ps);
        return 0;
    }
    default:
        break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

} // namespace

bool AcrylicBackdrop::create(HWND owner) {
    if (!resolveSetter()) {
        MD_LOG(L"SetWindowCompositionAttribute unavailable; using painted glass");
        return false;
    }

    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = &backdropProc;
    wc.hInstance     = GetModuleHandleW(nullptr);
    wc.lpszClassName = kClassName;
    RegisterClassExW(&wc);

    hwnd_ = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT | WS_EX_LAYERED,
        kClassName, L"", WS_POPUP,
        0, 0, 1, 1, owner, nullptr, wc.hInstance, nullptr);

    if (!hwnd_) {
        MD_LOG(L"backdrop window creation failed (%lu)", GetLastError());
        return false;
    }

    SetLayeredWindowAttributes(hwnd_, 0, 255, LWA_ALPHA);
    available_ = true;
    applyAccent();
    return true;
}

void AcrylicBackdrop::destroy() {
    if (hwnd_) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
    available_ = false;
    visible_ = false;
}

void AcrylicBackdrop::applyAccent() {
    auto fn = resolveSetter();
    if (!fn || !hwnd_) return;

    ACCENT_POLICY policy{};
    policy.accentState   = ACCENT_ENABLE_ACRYLICBLURBEHIND;
    policy.accentFlags   = 2;             // draw the tint over the whole client area
    policy.gradientColor = toAbgr(tint_);
    policy.animationId   = 0;

    WINDOWCOMPOSITIONATTRIBDATA data{};
    data.attribute = 19;                  // WCA_ACCENT_POLICY
    data.data      = &policy;
    data.size      = sizeof(policy);

    if (!fn(hwnd_, &data)) {
        // Acrylic is refused on some configurations (notably with transparency
        // effects turned off in Settings); plain blur still looks right.
        policy.accentState = ACCENT_ENABLE_BLURBEHIND;
        fn(hwnd_, &data);
    }
}

void AcrylicBackdrop::setTint(const Color& tint) {
    tint_ = tint;
    applyAccent();
}

void AcrylicBackdrop::applyCornerRegion() {
    if (!hwnd_) return;
    const int w = bounds_.right - bounds_.left;
    const int h = bounds_.bottom - bounds_.top;
    if (w <= 0 || h <= 0) return;

    const int d = cornerRadius_ * 2;
    HRGN region = (d > 0) ? CreateRoundRectRgn(0, 0, w + 1, h + 1, d, d)
                          : CreateRectRgn(0, 0, w, h);
    // The window takes ownership of the region; do not delete it here.
    SetWindowRgn(hwnd_, region, FALSE);
}

void AcrylicBackdrop::setBounds(const RECT& screenRect, int cornerRadiusPx) {
    if (!hwnd_) return;

    const bool sizeChanged =
        (screenRect.right - screenRect.left) != (bounds_.right - bounds_.left) ||
        (screenRect.bottom - screenRect.top) != (bounds_.bottom - bounds_.top) ||
        cornerRadiusPx != cornerRadius_;

    bounds_ = screenRect;
    cornerRadius_ = cornerRadiusPx;

    SetWindowPos(hwnd_, nullptr,
                 bounds_.left, bounds_.top,
                 bounds_.right - bounds_.left, bounds_.bottom - bounds_.top,
                 SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOREDRAW);

    if (sizeChanged) applyCornerRegion();
}

void AcrylicBackdrop::setVisible(bool visible) {
    if (!hwnd_ || visible == visible_) return;
    visible_ = visible;
    ShowWindow(hwnd_, visible ? SW_SHOWNOACTIVATE : SW_HIDE);
}

} // namespace md::platform
