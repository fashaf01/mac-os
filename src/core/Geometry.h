#pragma once

namespace md {

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

struct RectF {
    float left = 0.0f, top = 0.0f, right = 0.0f, bottom = 0.0f;

    constexpr float width()   const { return right - left; }
    constexpr float height()  const { return bottom - top; }
    constexpr float centerX() const { return (left + right) * 0.5f; }
    constexpr float centerY() const { return (top + bottom) * 0.5f; }

    constexpr bool contains(float x, float y) const {
        return x >= left && x < right && y >= top && y < bottom;
    }

    constexpr RectF inflated(float dx, float dy) const {
        return { left - dx, top - dy, right + dx, bottom + dy };
    }

    constexpr RectF offset(float dx, float dy) const {
        return { left + dx, top + dy, right + dx, bottom + dy };
    }
};

constexpr float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

constexpr float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

constexpr int clampi(int v, int lo, int hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

} // namespace md
