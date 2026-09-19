#include "platform/IconLoader.h"
#include "platform/AppResolver.h"
#include "core/Log.h"

#include <shlobj.h>
#include <shellapi.h>
#include <commoncontrols.h>

#pragma comment(lib, "shell32.lib")

namespace md::platform {
namespace {

// SHIL_JUMBO gives 256x256 where the app ships one, which is what makes a
// magnified dock icon stay sharp instead of turning into mush.
HICON extractJumbo(const std::wstring& path) {
    SHFILEINFOW info{};
    if (!SHGetFileInfoW(path.c_str(), 0, &info, sizeof(info),
                        SHGFI_SYSICONINDEX | SHGFI_LARGEICON)) {
        return nullptr;
    }

    static const int kSizes[] = { SHIL_JUMBO, SHIL_EXTRALARGE, SHIL_LARGE };
    for (int shil : kSizes) {
        IImageList* list = nullptr;
        if (FAILED(SHGetImageList(shil, IID_PPV_ARGS(&list))) || !list) continue;

        HICON icon = nullptr;
        const HRESULT hr = list->GetIcon(info.iIcon, ILD_TRANSPARENT, &icon);
        list->Release();
        if (SUCCEEDED(hr) && icon) return icon;
    }
    return nullptr;
}

HICON extractFallback(const std::wstring& path) {
    SHFILEINFOW info{};
    if (SHGetFileInfoW(path.c_str(), 0, &info, sizeof(info), SHGFI_ICON | SHGFI_LARGEICON)) {
        return info.hIcon;
    }
    return nullptr;
}

} // namespace

ComPtr<ID2D1Bitmap1> IconLoader::bitmapFromHICON(HICON icon) {
    ComPtr<ID2D1Bitmap1> result;
    if (!icon || !gfx_ || !gfx_->isValid()) return result;

    ComPtr<IWICBitmap> wicBitmap;
    HRESULT hr = gfx_->wic()->CreateBitmapFromHICON(icon, wicBitmap.GetAddressOf());
    if (FAILED(hr)) { MD_LOG(L"CreateBitmapFromHICON failed: 0x%08X", hr); return result; }

    ComPtr<IWICFormatConverter> converter;
    hr = gfx_->wic()->CreateFormatConverter(converter.GetAddressOf());
    if (FAILED(hr)) return result;

    hr = converter->Initialize(wicBitmap.Get(), GUID_WICPixelFormat32bppPBGRA,
                               WICBitmapDitherTypeNone, nullptr, 0.0,
                               WICBitmapPaletteTypeMedianCut);
    if (FAILED(hr)) return result;

    const auto props = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_NONE,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
        96.0f, 96.0f);

    hr = gfx_->d2d()->CreateBitmapFromWicBitmap(converter.Get(), props, result.GetAddressOf());
    if (FAILED(hr)) { MD_LOG(L"CreateBitmapFromWicBitmap failed: 0x%08X", hr); result.Reset(); }
    return result;
}

ComPtr<ID2D1Bitmap1> IconLoader::extract(const std::wstring& path) {
    HICON icon = extractJumbo(path);
    bool ownsIcon = icon != nullptr;
    if (!icon) {
        icon = extractFallback(path);
        ownsIcon = icon != nullptr;
    }
    if (!icon) return {};

    ComPtr<ID2D1Bitmap1> bitmap = bitmapFromHICON(icon);
    if (ownsIcon) DestroyIcon(icon);
    return bitmap;
}

ID2D1Bitmap1* IconLoader::iconForPath(const std::wstring& path) {
    if (path.empty()) return nullptr;

    const std::wstring key = normalizePath(path);
    if (auto it = cache_.find(key); it != cache_.end()) return it->second.Get();

    ComPtr<ID2D1Bitmap1> bitmap = extract(path);
    ID2D1Bitmap1* raw = bitmap.Get();
    cache_.emplace(key, std::move(bitmap));   // cache misses too, so we retry at most once
    return raw;
}

ID2D1Bitmap1* IconLoader::recycleBinIcon(bool full) {
    const std::wstring key = full ? L"\x01recyclebin-full" : L"\x01recyclebin-empty";
    if (auto it = cache_.find(key); it != cache_.end()) return it->second.Get();

    SHSTOCKICONINFO info{};
    info.cbSize = sizeof(info);
    ComPtr<ID2D1Bitmap1> bitmap;

    if (SUCCEEDED(SHGetStockIconInfo(full ? SIID_RECYCLERFULL : SIID_RECYCLER,
                                     SHGSI_ICON | SHGSI_LARGEICON, &info)) && info.hIcon) {
        bitmap = bitmapFromHICON(info.hIcon);
        DestroyIcon(info.hIcon);
    }

    ID2D1Bitmap1* raw = bitmap.Get();
    cache_.emplace(key, std::move(bitmap));
    return raw;
}

void IconLoader::invalidateDeviceResources() { cache_.clear(); }
void IconLoader::clear() { cache_.clear(); }

} // namespace md::platform
