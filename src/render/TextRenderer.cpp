#include "render/TextRenderer.h"
#include "core/Log.h"

namespace md {
namespace {
// Segoe UI Variable is the Windows 11 UI face and is the closest available
// match to the macOS system font's proportions. We fall back automatically
// on Windows 10, where DirectWrite resolves the family to Segoe UI.
constexpr const wchar_t* kFontFamily = L"Segoe UI Variable Display";
constexpr const wchar_t* kFontFallback = L"Segoe UI";
}

bool TextRenderer::create(GraphicsDevice* gfx, float sizePx) {
    gfx_    = gfx;
    sizePx_ = sizePx;
    return build();
}

void TextRenderer::destroy() {
    format_.Reset();
    gfx_ = nullptr;
}

bool TextRenderer::setSize(float sizePx) {
    if (format_ && sizePx == sizePx_) return true;
    sizePx_ = sizePx;
    return build();
}

bool TextRenderer::build() {
    format_.Reset();
    if (!gfx_ || !gfx_->dwrite()) return false;

    HRESULT hr = gfx_->dwrite()->CreateTextFormat(
        kFontFamily, nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
        sizePx_, L"", format_.GetAddressOf());

    if (FAILED(hr)) {
        hr = gfx_->dwrite()->CreateTextFormat(
            kFontFallback, nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD,
            DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
            sizePx_, L"", format_.GetAddressOf());
    }
    if (FAILED(hr)) { MD_LOG(L"CreateTextFormat failed: 0x%08X", hr); return false; }

    format_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
    format_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    format_->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
    return true;
}

Vec2 TextRenderer::measure(const std::wstring& text, float maxWidthPx) const {
    if (!format_ || text.empty()) return {};

    ComPtr<IDWriteTextLayout> layout;
    HRESULT hr = gfx_->dwrite()->CreateTextLayout(
        text.c_str(), static_cast<UINT32>(text.size()), format_.Get(),
        maxWidthPx, sizePx_ * 3.0f, layout.GetAddressOf());
    if (FAILED(hr)) return {};

    DWRITE_TEXT_METRICS m{};
    if (FAILED(layout->GetMetrics(&m))) return {};
    return { m.widthIncludingTrailingWhitespace, m.height };
}

void TextRenderer::draw(ID2D1DeviceContext* dc, const std::wstring& text,
                        const RectF& box, ID2D1Brush* brush) const {
    if (!format_ || text.empty() || !brush) return;
    dc->DrawTextW(text.c_str(), static_cast<UINT32>(text.size()), format_.Get(),
                  D2D1::RectF(box.left, box.top, box.right, box.bottom), brush,
                  D2D1_DRAW_TEXT_OPTIONS_CLIP);
}

} // namespace md
