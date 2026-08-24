#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace AimRestrictionPolicy {

inline constexpr float kDegreesToRadians = 0.01745329251994329577F;
inline constexpr float kHysteresisDegrees = 1.0F;

struct Vector3 {
    float x{};
    float y{};
    float z{};
};

enum class Mode : std::uint8_t {
    Native,
    CameraDirected,
    ConeLimited,
    TargetAssisted,
};

enum class Reason : std::uint8_t {
    Disabled,
    NoTarget,
    InvalidSolution,
    OutsideCone,
    Eligible,
};

struct RimHistory {
    Vector3 lastBaseAim{};
    bool valid{};
};

struct BaseResult {
    Vector3 ray{};
    Mode mode{Mode::Native};
    float shipCameraDot{1.0F};
    float angleDegrees{};
    bool valid{};
};

struct TargetResult {
    bool eligible{};
    float shipTargetDot{-1.0F};
    float angleDegrees{180.0F};
    Reason reason{Reason::InvalidSolution};
};

inline float Dot(const Vector3& left, const Vector3& right) noexcept {
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

inline Vector3 Add(const Vector3& left, const Vector3& right) noexcept {
    return {left.x + right.x, left.y + right.y, left.z + right.z};
}

inline Vector3 Scale(const Vector3& value, float scale) noexcept {
    return {value.x * scale, value.y * scale, value.z * scale};
}

inline Vector3 TransformDirection(const Vector3& right, const Vector3& forward,
                                  const Vector3& up, const Vector3& local) noexcept {
    return Add(Add(Scale(right, local.x), Scale(forward, local.y)),
               Scale(up, local.z));
}

inline Vector3 InverseTransformDirection(const Vector3& right, const Vector3& forward,
                                         const Vector3& up, const Vector3& value) noexcept {
    return {Dot(right, value), Dot(forward, value), Dot(up, value)};
}

inline bool Normalize(const Vector3& input, Vector3& output) noexcept {
    const float squared = Dot(input, input);
    if (!std::isfinite(squared) || squared < 1.0e-10F) return false;
    const float inverse = 1.0F / std::sqrt(squared);
    output = Scale(input, inverse);
    return std::isfinite(output.x) && std::isfinite(output.y) && std::isfinite(output.z);
}

inline float AngleDegrees(float dot) noexcept {
    return std::acos(std::clamp(dot, -1.0F, 1.0F)) / kDegreesToRadians;
}

inline BaseResult ComputeBaseRay(const Vector3& rawShipForward, const Vector3& rawCameraForward,
                                 bool limitEnabled, float halfAngleDegrees,
                                 RimHistory& history) noexcept {
    BaseResult result;
    Vector3 ship;
    Vector3 camera;
    if (!Normalize(rawShipForward, ship) || !Normalize(rawCameraForward, camera)) {
        history.valid = false;
        return result;
    }

    result.shipCameraDot = std::clamp(Dot(ship, camera), -1.0F, 1.0F);
    result.angleDegrees = AngleDegrees(result.shipCameraDot);
    result.valid = true;
    if (!limitEnabled) {
        result.ray = camera;
        result.mode = Mode::CameraDirected;
        history.lastBaseAim = camera;
        history.valid = true;
        return result;
    }

    const float halfAngle = std::clamp(halfAngleDegrees, 1.0F, 90.0F);
    if (result.angleDegrees <= halfAngle) {
        result.ray = camera;
        result.mode = Mode::CameraDirected;
        history.lastBaseAim = camera;
        history.valid = true;
        return result;
    }

    const float alpha = halfAngle * kDegreesToRadians;
    Vector3 tangent;
    const Vector3 rejected = Add(camera, Scale(ship, -result.shipCameraDot));
    if (!Normalize(rejected, tangent)) {
        if (history.valid) {
            const float projected = Dot(history.lastBaseAim, ship);
            Normalize(Add(history.lastBaseAim, Scale(ship, -projected)), tangent);
        }
        if (!Normalize(tangent, tangent)) {
            // With no continuous azimuth history, activation/recenter resolves the
            // antiparallel singularity to ship-forward until motion establishes one.
            result.ray = ship;
            result.mode = Mode::ConeLimited;
            history.lastBaseAim = ship;
            history.valid = true;
            return result;
        }
    }

    Vector3 bounded = Add(Scale(ship, std::cos(alpha)), Scale(tangent, std::sin(alpha)));
    if (!Normalize(bounded, result.ray)) {
        result.valid = false;
        history.valid = false;
        return result;
    }
    result.mode = Mode::ConeLimited;
    history.lastBaseAim = result.ray;
    history.valid = true;
    return result;
}

inline TargetResult EvaluateTarget(const Vector3& rawShipForward, const Vector3& rawCandidate,
                                   float halfAngleDegrees, bool previouslyEligible) noexcept {
    TargetResult result;
    Vector3 ship;
    Vector3 candidate;
    if (!Normalize(rawShipForward, ship) || !Normalize(rawCandidate, candidate)) {
        return result;
    }

    result.shipTargetDot = std::clamp(Dot(ship, candidate), -1.0F, 1.0F);
    result.angleDegrees = AngleDegrees(result.shipTargetDot);
    const float halfAngle = std::clamp(halfAngleDegrees, 1.0F, 90.0F);
    const float threshold =
        previouslyEligible ? halfAngle + kHysteresisDegrees : halfAngle - kHysteresisDegrees;
    result.eligible =
        previouslyEligible ? result.angleDegrees <= threshold : result.angleDegrees < threshold;
    result.reason = result.eligible ? Reason::Eligible : Reason::OutsideCone;
    return result;
}

inline Vector3 DirectionFromLaunchAngles(float pitch, float yaw) noexcept {
    const float cosinePitch = std::cos(pitch);
    return {
        std::sin(yaw) * cosinePitch,
        std::cos(yaw) * cosinePitch,
        -std::sin(pitch),
    };
}

} // namespace AimRestrictionPolicy
