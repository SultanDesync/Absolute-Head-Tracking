#pragma once

#include <algorithm>
#include <array>
#include <cmath>

namespace HeadMath {
struct Quaternion {
    float w = 1.0F;
    float x = 0.0F;
    float y = 0.0F;
    float z = 0.0F;
};

inline float ShapeAngle(float degrees, float deadzone, float minimum,
                        float center, float maximum) noexcept {
    const float magnitude = std::abs(degrees);
    const float shaped = magnitude <= deadzone
        ? 0.0F : std::copysign(magnitude - deadzone, degrees);
    return std::clamp(center + shaped, minimum, maximum);
}

inline float ShapeAngle(float degrees, float deadzone, float maximum) noexcept {
    return ShapeAngle(degrees, deadzone, -maximum, 0.0F, maximum);
}

inline float Smooth(float current, float target, float smoothing, float dt) noexcept {
    if (dt <= 0.0F) return current;
    const float retention = std::pow(std::clamp(smoothing, 0.0F, 0.99F), dt * 60.0F);
    return current + (target - current) * (1.0F - retention);
}

inline Quaternion Multiply(const Quaternion& left, const Quaternion& right) noexcept {
    return {
        left.w * right.w - left.x * right.x - left.y * right.y - left.z * right.z,
        left.w * right.x + left.x * right.w + left.y * right.z - left.z * right.y,
        left.w * right.y - left.x * right.z + left.y * right.w + left.z * right.x,
        left.w * right.z + left.x * right.y - left.y * right.x + left.z * right.w,
    };
}

inline Quaternion AxisAngle(float x, float y, float z, float radians) noexcept {
    const float half = radians * 0.5F;
    const float sine = std::sin(half);
    return { std::cos(half), x * sine, y * sine, z * sine };
}

inline Quaternion FromDegrees(float yaw, float pitch, float roll) noexcept {
    constexpr float radiansPerDegree = 0.01745329251994329577F;
    const auto pitchRotation = AxisAngle(1.0F, 0.0F, 0.0F, pitch * radiansPerDegree);
    const auto rollRotation = AxisAngle(0.0F, 1.0F, 0.0F, roll * radiansPerDegree);
    const auto yawRotation = AxisAngle(0.0F, 0.0F, 1.0F, yaw * radiansPerDegree);
    return Multiply(Multiply(yawRotation, rollRotation), pitchRotation);
}
} // namespace HeadMath
