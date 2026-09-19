#pragma once

#include "core/Geometry.h"
#include <cmath>

namespace md {

// Critically-damped-ish spring. step() returns false once the value has
// settled, which is how the dock knows it can stop its frame timer and drop
// back to zero CPU.
class Spring {
public:
    void configure(float stiffness, float damping) {
        stiffness_ = stiffness;
        damping_   = damping;
    }

    void snapTo(float v) { value_ = target_ = v; velocity_ = 0.0f; }
    void setTarget(float v) { target_ = v; }

    float value()  const { return value_; }
    float target() const { return target_; }
    bool  settled() const {
        return std::fabs(value_ - target_) < kEpsilon && std::fabs(velocity_) < kEpsilon;
    }

    // dt in seconds. Substepped so a hitch cannot make the spring explode.
    bool step(float dt) {
        if (settled()) { value_ = target_; velocity_ = 0.0f; return false; }

        constexpr float kMaxStep = 1.0f / 120.0f;
        float remaining = dt > 0.25f ? 0.25f : dt;
        while (remaining > 0.0f) {
            const float h = remaining > kMaxStep ? kMaxStep : remaining;
            const float accel = stiffness_ * (target_ - value_) - damping_ * velocity_;
            velocity_ += accel * h;
            value_    += velocity_ * h;
            remaining -= h;
        }

        if (settled()) { value_ = target_; velocity_ = 0.0f; return false; }
        return true;
    }

private:
    static constexpr float kEpsilon = 0.0015f;
    float value_ = 0.0f, target_ = 0.0f, velocity_ = 0.0f;
    float stiffness_ = 260.0f, damping_ = 26.0f;
};

// The launch bounce: two decaying hops over ~0.9s, then done.
class Bounce {
public:
    void start() { t_ = 0.0f; active_ = true; }
    void stop()  { active_ = false; t_ = 0.0f; }
    bool active() const { return active_; }

    bool step(float dt) {
        if (!active_) return false;
        t_ += dt;
        if (t_ >= kDuration) { stop(); return false; }
        return true;
    }

    // Upward offset in pixels for the given icon size.
    float offset(float iconSizePx) const {
        if (!active_) return 0.0f;
        const float u = clampf(t_ / kDuration, 0.0f, 1.0f);
        const float decay = 1.0f - u;
        const float hop = std::fabs(std::sin(u * 3.14159265f * 2.0f));
        return hop * decay * decay * iconSizePx * 0.55f;
    }

private:
    static constexpr float kDuration = 0.9f;
    float t_ = 0.0f;
    bool  active_ = false;
};

} // namespace md
