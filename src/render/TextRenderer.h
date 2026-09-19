#pragma once

#include "render/GraphicsDevice.h"
#include "core/Geometry.h"

#include <string>

namespace md {

// Thin DirectWrite wrapper: one cached text format, measure + draw.
class TextRenderer {
public:
    bool create(GraphicsDevice* gfx, float sizePx);
    void destroy();

    // Rebuilds the format when the monitor scale changes.
    bool setSize(float sizePx);

    Vec2 measure(const std::wstring& text, float maxWidthPx) const;
    void draw(ID2D1DeviceContext* dc, const std::wstring& text,
              const RectF& box, ID2D1Brush* brush) const;

private:
    bool build();

    GraphicsDevice* gfx_ = nullptr;
    ComPtr<IDWriteTextFormat> format_;
    float sizePx_ = 12.0f;
};

} // namespace md
