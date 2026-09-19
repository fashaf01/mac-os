#pragma once

#include "render/GraphicsDevice.h"

#include <string>
#include <unordered_map>

namespace md::platform {

// Extracts the highest-quality icon Windows has for a file and caches it as a
// Direct2D bitmap. Extraction happens once per path; after that a dock repaint
// is pure GPU work.
class IconLoader {
public:
    void attach(GraphicsDevice* gfx) { gfx_ = gfx; }

    // Returns nullptr when the icon cannot be produced; callers draw a
    // placeholder in that case.
    ID2D1Bitmap1* iconForPath(const std::wstring& path);
    ID2D1Bitmap1* recycleBinIcon(bool full);

    // Called when the GPU device is rebuilt.
    void invalidateDeviceResources();
    void clear();

private:
    ComPtr<ID2D1Bitmap1> bitmapFromHICON(HICON icon);
    ComPtr<ID2D1Bitmap1> extract(const std::wstring& path);

    GraphicsDevice* gfx_ = nullptr;
    std::unordered_map<std::wstring, ComPtr<ID2D1Bitmap1>> cache_;
};

} // namespace md::platform
