#pragma once

#include "render/GraphicsDevice.h"

#include <windows.h>

namespace md {

// A borderless, per-pixel-alpha window whose content is a DirectComposition
// surface. No WS_EX_LAYERED: the compositor blends it directly, so resizing
// and redrawing never round-trip through GDI.
//
// Subclasses implement onRender() and call invalidate() when something
// actually changed. Nothing redraws on a timer by default.
class CompositionWindow {
public:
    virtual ~CompositionWindow();

    bool create(GraphicsDevice* gfx, const wchar_t* className, const wchar_t* title,
                DWORD style, DWORD exStyle, int x, int y, int w, int h);
    void destroy();

    HWND hwnd() const { return hwnd_; }
    int  widthPx()  const { return widthPx_; }
    int  heightPx() const { return heightPx_; }

    void invalidate();          // schedules exactly one repaint
    void renderNow();           // draws immediately if dirty
    void resizeSurface(int wPx, int hPx);

protected:
    virtual void onRender(ID2D1DeviceContext* dc, float wPx, float hPx) = 0;
    virtual LRESULT onMessage(UINT msg, WPARAM wp, LPARAM lp, bool& handled) {
        (void)msg; (void)wp; (void)lp; handled = false; return 0;
    }

    GraphicsDevice* gfx_ = nullptr;
    HWND hwnd_ = nullptr;

private:
    static LRESULT CALLBACK wndProc(HWND, UINT, WPARAM, LPARAM);
    bool ensureSurface();
    void releaseSurface();

    ComPtr<IDCompositionTarget>  target_;
    ComPtr<IDCompositionVisual>  visual_;
    ComPtr<IDCompositionSurface> surface_;

    int  widthPx_  = 0;
    int  heightPx_ = 0;
    bool dirty_        = true;
    bool paintPosted_  = false;
    bool inPaint_      = false;
};

} // namespace md
