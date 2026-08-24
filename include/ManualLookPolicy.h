#pragma once

#include <algorithm>
#include <cmath>

namespace ManualLookPolicy {

inline float ShapeAnalog(float normalized, float deadzone) noexcept {
    normalized = std::clamp(normalized, -1.0F, 1.0F);
    deadzone = std::clamp(deadzone, 0.0F, 0.95F);
    const float magnitude = std::abs(normalized);
    if (magnitude <= deadzone) return 0.0F;
    return std::copysign((magnitude - deadzone) / (1.0F - deadzone), normalized);
}

inline float AnalogOffset(float normalized, float deadzone, float minimum,
                          float center, float maximum) noexcept {
    const float shaped = ShapeAnalog(normalized, deadzone);
    return shaped < 0.0F ? shaped * (center - minimum)
                         : shaped * (maximum - center);
}

inline float Composite(float base, float retainedTrim, float analogOffset,
                       float minimum, float maximum) noexcept {
    return std::clamp(base + retainedTrim + analogOffset, minimum, maximum);
}

} // namespace ManualLookPolicy
