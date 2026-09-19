#pragma once

#include <d3d11.h>
#include <dxgi1_3.h>
#include <d2d1_1.h>
#include <d2d1_1helper.h>
#include <dwrite.h>
#include <wincodec.h>
#include <dcomp.h>
#include <wrl/client.h>

namespace md {

using Microsoft::WRL::ComPtr;

// One GPU device shared by every surface we draw. Created once, rebuilt only
// if the driver resets. Direct2D on top of Direct3D 11, presented through
// DirectComposition so the compositor does the blending on the GPU and the
// CPU is idle whenever nothing is animating.
class GraphicsDevice {
public:
    bool create();
    void destroy();

    // True if the device was lost and needs create() again.
    bool isValid() const { return d2dContext_ != nullptr; }

    ID3D11Device*          d3d()        const { return d3dDevice_.Get(); }
    IDXGIDevice*           dxgi()       const { return dxgiDevice_.Get(); }
    ID2D1Factory1*         d2dFactory() const { return d2dFactory_.Get(); }
    ID2D1Device*           d2dDevice()  const { return d2dDevice_.Get(); }
    ID2D1DeviceContext*    d2d()        const { return d2dContext_.Get(); }
    IDWriteFactory*        dwrite()     const { return dwriteFactory_.Get(); }
    IWICImagingFactory*    wic()        const { return wicFactory_.Get(); }
    IDCompositionDevice*   dcomp()      const { return dcompDevice_.Get(); }

private:
    ComPtr<ID3D11Device>        d3dDevice_;
    ComPtr<ID3D11DeviceContext> d3dContext_;
    ComPtr<IDXGIDevice>         dxgiDevice_;
    ComPtr<ID2D1Factory1>       d2dFactory_;
    ComPtr<ID2D1Device>         d2dDevice_;
    ComPtr<ID2D1DeviceContext>  d2dContext_;
    ComPtr<IDWriteFactory>      dwriteFactory_;
    ComPtr<IWICImagingFactory>  wicFactory_;
    ComPtr<IDCompositionDevice> dcompDevice_;
};

} // namespace md
