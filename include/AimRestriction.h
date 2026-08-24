#pragma once

#include <cstdint>

#include "AimRestrictionPolicy.h"
#include "HeadMath.h"

namespace AimRestriction {

struct Decision {
    AimRestrictionPolicy::Mode mode{AimRestrictionPolicy::Mode::Native};
    AimRestrictionPolicy::Reason reason{AimRestrictionPolicy::Reason::Disabled};
    std::uint64_t generation{};
    float measuredDot{1.0F};
    float measuredAngleDegrees{};
    float halfAngleDegrees{30.0F};
    bool fresh{};
    bool buildValidated{};
};

struct Status {
    bool installed{};
    bool frameFresh{};
    Decision lastDecision{};
};

bool Install();
void Shutdown();
bool Ready() noexcept;
void PublishCameraFrame(const HeadMath::Quaternion& headRotation) noexcept;
void ClearCameraFrame() noexcept;
void ResetSpatialState() noexcept;
Status GetStatus();

} // namespace AimRestriction
