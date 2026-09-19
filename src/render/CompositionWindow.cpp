#include "render/CompositionWindow.h"
#include "core/Log.h"

namespace md {
namespace {
constexpr UINT WM_MD_RENDER = WM_APP + 0x100;
}

CompositionWindow::~CompositionWindow() { destroy(); }

LRESULT CALLBACK CompositionWindow::wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    CompositionWindow* self = nullptr;

    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
        self = static_cast<CompositionWindow*>(cs->lpCreateParams);
        self->hwnd_ = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<CompositionWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (self) {
        if (msg == WM_MD_RENDER) {
            self->paintPosted_ = false;
            self->renderNow();
            return 0;
        }
        bool handled = false;
        const LRESULT r = self->onMessage(msg, wp, lp, handled);
        if (handled) return r;
    }

    return DefWindowProcW(hwnd, msg, wp, lp);
}

bool CompositionWindow::create(GraphicsDevice* gfx, const wchar_t* className,
                               const wchar_t* title, DWORD style, DWORD exStyle,
                               int x, int y, int w, int h) {
    gfx_ = gfx;

    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = &CompositionWindow::wndProc;
    wc.hInstance     = GetModuleHandleW(nullptr);
    wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = className;
    wc.hbrBackground = nullptr;   // we own every pixel
    RegisterClassExW(&wc);        // duplicate registration is harmless

    hwnd_ = CreateWindowExW(exStyle, className, title, style,
                            x, y, w, h, nullptr, nullptr,
                            wc.hInstance, this);
    if (!hwnd_) {
        MD_LOG(L"CreateWindowExW failed: %lu", GetLastError());
        return false;
    }

    HRESULT hr = gfx_->dcomp()->CreateTargetForHwnd(hwnd_, TRUE, target_.GetAddressOf());
    if (FAILED(hr)) { MD_LOG(L"CreateTargetForHwnd failed: 0x%08X", hr); return false; }

    hr = gfx_->dcomp()->CreateVisual(visual_.GetAddressOf());
    if (FAILED(hr)) { MD_LOG(L"CreateVisual failed: 0x%08X", hr); return false; }

    hr = target_->SetRoot(visual_.Get());
    if (FAILED(hr)) { MD_LOG(L"SetRoot failed: 0x%08X", hr); return false; }

    resizeSurface(w, h);
    return true;
}

void CompositionWindow::destroy() {
    releaseSurface();
    visual_.Reset();
    target_.Reset();
    if (hwnd_) {
        SetWindowLongPtrW(hwnd_, GWLP_USERDATA, 0);
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
}

void CompositionWindow::releaseSurface() {
    if (visual_) visual_->SetContent(nullptr);
    surface_.Reset();
}

void CompositionWindow::resizeSurface(int wPx, int hPx) {
    if (wPx < 1) wPx = 1;
    if (hPx < 1) hPx = 1;
    if (wPx == widthPx_ && hPx == heightPx_ && surface_) return;

    widthPx_  = wPx;
    heightPx_ = hPx;
    releaseSurface();
    dirty_ = true;
}

bool CompositionWindow::ensureSurface() {
    if (surface_) return true;
    if (!gfx_ || !gfx_->isValid()) return false;

    HRESULT hr = gfx_->dcomp()->CreateSurface(
        static_cast<UINT>(widthPx_), static_cast<UINT>(heightPx_),
        DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_ALPHA_MODE_PREMULTIPLIED,
        surface_.GetAddressOf());
    if (FAILED(hr)) { MD_LOG(L"CreateSurface failed: 0x%08X", hr); return false; }

    hr = visual_->SetContent(surface_.Get());
    if (FAILED(hr)) { MD_LOG(L"SetContent failed: 0x%08X", hr); return false; }
    return true;
}

void CompositionWindow::invalidate() {
    dirty_ = true;
    if (paintPosted_) return;           // one pending repaint at a time
    paintPosted_ = true;
    if (hwnd_) PostMessageW(hwnd_, WM_MD_RENDER, 0, 0);
}

void CompositionWindow::renderNow() {
    if (!dirty_ || inPaint_) return;
    if (!gfx_ || !gfx_->isValid()) return;
    if (!ensureSurface()) return;

    inPaint_ = true;
    dirty_   = false;

    POINT offset{};
    ComPtr<IDXGISurface> dxgiSurface;
    HRESULT hr = surface_->BeginDraw(nullptr, IID_PPV_ARGS(dxgiSurface.GetAddressOf()), &offset);
    if (FAILED(hr)) {
        MD_LOG(L"IDCompositionSurface::BeginDraw failed: 0x%08X", hr);
        releaseSurface();
        inPaint_ = false;
        return;
    }

    ID2D1DeviceContext* dc = gfx_->d2d();

    const auto props = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
        96.0f, 96.0f);

    ComPtr<ID2D1Bitmap1> bitmap;
    hr = dc->CreateBitmapFromDxgiSurface(dxgiSurface.Get(), props, bitmap.GetAddressOf());
    if (SUCCEEDED(hr)) {
        dc->SetTarget(bitmap.Get());
        dc->BeginDraw();
        // BeginDraw can hand back a tile inside a larger atlas; the offset puts
        // our origin back at (0,0).
        dc->SetTransform(D2D1::Matrix3x2F::Translation(
            static_cast<float>(offset.x), static_cast<float>(offset.y)));
        dc->Clear(D2D1::ColorF(0, 0.0f));

        onRender(dc, static_cast<float>(widthPx_), static_cast<float>(heightPx_));

        const HRESULT end = dc->EndDraw();
        dc->SetTarget(nullptr);

        if (end == static_cast<HRESULT>(D2DERR_RECREATE_TARGET)) {
            MD_LOG(L"D2D target lost, rebuilding device");
            releaseSurface();
            gfx_->create();
            dirty_ = true;
        }
    } else {
        MD_LOG(L"CreateBitmapFromDxgiSurface failed: 0x%08X", hr);
    }

    surface_->EndDraw();
    gfx_->dcomp()->Commit();
    inPaint_ = false;
}

} // namespace md
