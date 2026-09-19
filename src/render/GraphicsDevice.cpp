#include "render/GraphicsDevice.h"
#include "core/Log.h"

#include <iterator>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "dcomp.lib")
#pragma comment(lib, "windowscodecs.lib")

namespace md {

bool GraphicsDevice::create() {
    destroy();

    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT; // required by Direct2D
#ifdef _DEBUG
    // Only if the graphics tools optional feature is installed; we retry
    // without it below, so a missing debug layer is not fatal.
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    const D3D_FEATURE_LEVEL levels[] = {
        D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0,
    };

    HRESULT hr = D3D11CreateDevice(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
        levels, static_cast<UINT>(std::size(levels)), D3D11_SDK_VERSION,
        d3dDevice_.GetAddressOf(), nullptr, d3dContext_.GetAddressOf());

#ifdef _DEBUG
    if (FAILED(hr)) {
        flags &= ~static_cast<UINT>(D3D11_CREATE_DEVICE_DEBUG);
        hr = D3D11CreateDevice(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
            levels, static_cast<UINT>(std::size(levels)), D3D11_SDK_VERSION,
            d3dDevice_.GetAddressOf(), nullptr, d3dContext_.GetAddressOf());
    }
#endif

    if (FAILED(hr)) {
        MD_LOG(L"hardware D3D11 device failed (0x%08X), falling back to WARP", hr);
        hr = D3D11CreateDevice(
            nullptr, D3D_DRIVER_TYPE_WARP, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
            levels, static_cast<UINT>(std::size(levels)), D3D11_SDK_VERSION,
            d3dDevice_.GetAddressOf(), nullptr, d3dContext_.GetAddressOf());
    }
    if (FAILED(hr)) { MD_LOG(L"D3D11CreateDevice failed: 0x%08X", hr); return false; }

    hr = d3dDevice_.As(&dxgiDevice_);
    if (FAILED(hr)) { MD_LOG(L"QI IDXGIDevice failed: 0x%08X", hr); return false; }

    // One frame of latency keeps hover tracking feeling attached to the cursor.
    if (ComPtr<IDXGIDevice1> dxgi1; SUCCEEDED(dxgiDevice_.As(&dxgi1))) {
        dxgi1->SetMaximumFrameLatency(1);
    }

    D2D1_FACTORY_OPTIONS opts{};
#ifdef _DEBUG
    opts.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#endif
    hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,
                           __uuidof(ID2D1Factory1), &opts,
                           reinterpret_cast<void**>(d2dFactory_.GetAddressOf()));
    if (FAILED(hr)) { MD_LOG(L"D2D1CreateFactory failed: 0x%08X", hr); return false; }

    hr = d2dFactory_->CreateDevice(dxgiDevice_.Get(), d2dDevice_.GetAddressOf());
    if (FAILED(hr)) { MD_LOG(L"ID2D1Factory1::CreateDevice failed: 0x%08X", hr); return false; }

    hr = d2dDevice_->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE,
                                         d2dContext_.GetAddressOf());
    if (FAILED(hr)) { MD_LOG(L"CreateDeviceContext failed: 0x%08X", hr); return false; }

    // We lay out in physical pixels and scale by the monitor factor ourselves,
    // which keeps the DirectComposition surface offset maths trivial.
    d2dContext_->SetDpi(96.0f, 96.0f);
    d2dContext_->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);

    hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                             reinterpret_cast<IUnknown**>(dwriteFactory_.GetAddressOf()));
    if (FAILED(hr)) { MD_LOG(L"DWriteCreateFactory failed: 0x%08X", hr); return false; }

    hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                          IID_PPV_ARGS(wicFactory_.GetAddressOf()));
    if (FAILED(hr)) { MD_LOG(L"WIC factory failed: 0x%08X", hr); return false; }

    hr = DCompositionCreateDevice(dxgiDevice_.Get(), __uuidof(IDCompositionDevice),
                                  reinterpret_cast<void**>(dcompDevice_.GetAddressOf()));
    if (FAILED(hr)) { MD_LOG(L"DCompositionCreateDevice failed: 0x%08X", hr); return false; }

    MD_LOG(L"graphics device ready");
    return true;
}

void GraphicsDevice::destroy() {
    if (d2dContext_) d2dContext_->SetTarget(nullptr);
    dcompDevice_.Reset();
    wicFactory_.Reset();
    dwriteFactory_.Reset();
    d2dContext_.Reset();
    d2dDevice_.Reset();
    d2dFactory_.Reset();
    dxgiDevice_.Reset();
    d3dContext_.Reset();
    d3dDevice_.Reset();
}

} // namespace md
