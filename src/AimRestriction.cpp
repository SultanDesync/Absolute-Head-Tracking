#include "AimRestriction.h"

#include <MinHook.h>
#include <intrin.h>

#include "Config.h"
#include "PCH.h"
#include "RuntimePaths.h"

namespace {
using AimRestrictionPolicy::BaseResult;
using AimRestrictionPolicy::Mode;
using AimRestrictionPolicy::Reason;
using AimRestrictionPolicy::RimHistory;
using AimRestrictionPolicy::TargetResult;
using AimRestrictionPolicy::Vector3;

// Qualified Starfield 1.16.244 seams. Every address is additionally byte-gated
// at runtime; a mismatched 1.16.242 executable remains safely unsupported.
constexpr std::uintptr_t kProjectileConsumerRva = 0x1B3BD50;
constexpr std::uintptr_t kMatrixToAnglesRva = 0x2BCDF60;
constexpr std::uintptr_t kBaseAnglesReturnRvaA = 0x1B53EC9;
constexpr std::uintptr_t kBaseAnglesReturnRvaB = 0x1B53EF9;
constexpr std::uintptr_t kDirectionToAnglesRva = 0x1C25900;
constexpr std::uintptr_t kTargetAngleReturnRva = 0x1B541E4;
constexpr std::uintptr_t kHudUpdateRva = 0x1558B70;
constexpr std::uintptr_t kHudTargetSolverRva = 0x210F0E0;
constexpr std::uintptr_t kHudTargetSolverReturnRva = 0x155A1CF;
constexpr std::uintptr_t kHudTargetSolverCallRva = 0x155A1CA;
constexpr std::uintptr_t kHudModelRegistrationRva = 0x1544020;
constexpr std::uintptr_t kGetActiveCameraRva = 0x12F0CB0;
constexpr std::uintptr_t kProjectWorldToHudRva = 0x2BE4280;
constexpr std::uintptr_t kActiveCameraManagerPointerRva = 0x61E2700;
constexpr std::uintptr_t kHudGetActiveCameraCallRva = 0x155A10F;
constexpr std::uintptr_t kHudWorldToScreenCallRva = 0x155A28F;
constexpr std::uintptr_t kBaseAngleWriterRva = 0x1B53F0A;
constexpr std::uintptr_t kTargetAngleWriterRva = 0x1B541E9;
constexpr std::int64_t kFrameFreshMilliseconds = 250;
constexpr std::int64_t kTargetTagFreshMilliseconds = 250;

using ProjectileConsumerFunction = void* (*)(void*, void*, std::uint32_t);
using MatrixToAnglesFunction = bool (*)(const void*, float*, float*, float*);
using DirectionToAnglesFunction = void (*)(const Vector3*, float*, float*);
using HudUpdateFunction = void (*)(void*);
using HudTargetSolverFunction = bool (*)(void*, std::uint32_t, const Vector3*, float, void*,
                                         Vector3*);
using GetActiveCameraFunction = void* (*)(void*);
using ProjectWorldToHudFunction = bool (*)(void*, const Vector3*, float*, float*);

ProjectileConsumerFunction g_projectileConsumerOriginal{};
MatrixToAnglesFunction g_matrixToAnglesOriginal{};
DirectionToAnglesFunction g_directionToAnglesOriginal{};
HudUpdateFunction g_hudUpdateOriginal{};
HudTargetSolverFunction g_hudTargetSolverOriginal{};
GetActiveCameraFunction g_getActiveCamera{};
ProjectWorldToHudFunction g_projectWorldToHud{};
std::uintptr_t g_activeCameraManagerPointer{};
std::array<void*, 5> g_hookTargets{};
std::atomic<bool> g_installed{};
std::atomic<bool> g_loggedLaunchBasis{};
std::atomic<bool> g_loggedTargetTag{};
std::atomic<bool> g_loggedTargetMatch{};
std::atomic<bool> g_loggedHudProjection{};
std::atomic<bool> g_baseReticleOwned{};

struct CameraFrame {
    Vector3 localRight{};
    Vector3 localCameraForward{};
    Vector3 localUp{};
    Vector3 shipForward{};
    Vector3 cameraForward{};
    std::int64_t timestampMilliseconds{};
    std::uint64_t generation{};
    bool valid{};
};

struct LaunchBase {
    Vector3 forward{};
    Vector3 right{};
    Vector3 up{};
    std::int64_t timestampMilliseconds{};
    bool valid{};
};

std::mutex g_stateMutex;
CameraFrame g_frame;
LaunchBase g_launchBase;
RimHistory g_localRimHistory;
bool g_hudTargetEligible{};
bool g_ballisticTargetEligible{};
AimRestriction::Decision g_lastDecision;

struct TargetTag {
    Vector3 direction{};
    std::int64_t timestampMilliseconds{};
    bool valid{};
};

TargetTag g_targetTag;

std::int64_t SteadyMilliseconds() noexcept {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

#pragma warning(push)
#pragma warning(disable : 4733)

template <class T> bool SafeRead(std::uintptr_t address, T& value) noexcept {
    if (!address) return false;
    __try {
        value = *reinterpret_cast<volatile T*>(address);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool SafeCopy(void* destination, const void* source, std::size_t size) noexcept {
    if (!destination || !source || size == 0) return false;
    __try {
        std::memcpy(destination, source, size);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool SetHudFloat(std::uintptr_t wrapper, float value) noexcept {
    if (!wrapper || !std::isfinite(value)) return false;
    __try {
        auto* stored = reinterpret_cast<float*>(wrapper + 0x18);
        if (*stored == value) return true;
        *stored = value;
        auto** vtable = *reinterpret_cast<void***>(wrapper);
        if (!vtable || !vtable[9]) return false;
        reinterpret_cast<void (*)(void*, bool)>(vtable[9])(reinterpret_cast<void*>(wrapper), true);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool SetHudBool(std::uintptr_t wrapper, bool value) noexcept {
    if (!wrapper) return false;
    __try {
        auto* stored = reinterpret_cast<bool*>(wrapper + 0x18);
        if (*stored == value) return true;
        *stored = value;
        auto** vtable = *reinterpret_cast<void***>(wrapper);
        if (!vtable || !vtable[9]) return false;
        reinterpret_cast<void (*)(void*, bool)>(vtable[9])(reinterpret_cast<void*>(wrapper), true);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

void* HudModel(void* owner) noexcept {
    if (!owner) return nullptr;
    __try {
        auto* source = reinterpret_cast<std::byte*>(owner) + 0x1BD0;
        auto** vtable = *reinterpret_cast<void***>(source);
        if (!vtable || !vtable[11]) return nullptr;
        return reinterpret_cast<void* (*)(void*)>(vtable[11])(source);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
}

#pragma warning(pop)

template <std::size_t Size>
bool Matches(std::uintptr_t address, const std::array<std::uint8_t, Size>& expected) noexcept {
    std::array<std::uint8_t, Size> actual{};
    return SafeCopy(actual.data(), reinterpret_cast<const void*>(address), Size) &&
           actual == expected;
}

bool ProjectBaseRayToHud(const CameraFrame& frame, const Vector3& boundedShipRay,
                         Vector3& cameraRay, float& x, float& y) noexcept {
    cameraRay = AimRestrictionPolicy::InverseTransformDirection(
        frame.localRight, frame.localCameraForward, frame.localUp, boundedShipRay);
    Vector3 normalizedCameraRay;
    if (!AimRestrictionPolicy::Normalize(cameraRay, normalizedCameraRay) ||
        normalizedCameraRay.y <= 1.0e-4F || !g_getActiveCamera || !g_projectWorldToHud ||
        !g_activeCameraManagerPointer) {
        return false;
    }
    cameraRay = normalizedCameraRay;

    void* manager = nullptr;
    if (!SafeRead(g_activeCameraManagerPointer, manager) || !manager) return false;
    void* camera = g_getActiveCamera(manager);
    if (!camera) return false;

    const auto cameraAddress = reinterpret_cast<std::uintptr_t>(camera);
    Vector3 position;
    std::array<float, 4> clipX{};
    std::array<float, 4> clipY{};
    std::array<float, 4> clipW{};
    if (!SafeCopy(&position, reinterpret_cast<const void*>(cameraAddress + 0xB0),
                  sizeof(position)) ||
        !SafeCopy(clipX.data(), reinterpret_cast<const void*>(cameraAddress + 0x180),
                  sizeof(clipX)) ||
        !SafeCopy(clipY.data(), reinterpret_cast<const void*>(cameraAddress + 0x190),
                  sizeof(clipY)) ||
        !SafeCopy(clipW.data(), reinterpret_cast<const void*>(cameraAddress + 0x1B0),
                  sizeof(clipW))) {
        return false;
    }

    Vector3 worldRight;
    Vector3 worldUp;
    Vector3 worldForward;
    if (!AimRestrictionPolicy::Normalize({clipX[0], clipX[1], clipX[2]}, worldRight) ||
        !AimRestrictionPolicy::Normalize({clipY[0], clipY[1], clipY[2]}, worldUp) ||
        !AimRestrictionPolicy::Normalize({clipW[0], clipW[1], clipW[2]}, worldForward)) {
        return false;
    }

    Vector3 worldRay = AimRestrictionPolicy::TransformDirection(
        worldRight, worldForward, worldUp, normalizedCameraRay);
    if (!AimRestrictionPolicy::Normalize(worldRay, worldRay)) return false;
    constexpr float kProjectionDistance = 10000.0F;
    const Vector3 point = AimRestrictionPolicy::Add(
        position, AimRestrictionPolicy::Scale(worldRay, kProjectionDistance));
    float projectedX = 0.0F;
    float projectedY = 0.0F;
    if (!g_projectWorldToHud(camera, &point, &projectedX, &projectedY) ||
        !std::isfinite(projectedX) || !std::isfinite(projectedY)) {
        return false;
    }

    // AimAssistCrosshairX consumes the projector's absolute viewport X. The
    // native HUD writer flips projector Y around 1.0 before publishing
    // AimAssistCrosshairY because Scaleform's vertical axis points down.
    x = projectedX;
    y = 1.0F - projectedY;
    return std::isfinite(x) && std::isfinite(y);
}

Vector3 Forward(const HeadMath::Quaternion& raw) noexcept {
    const float squared = raw.w * raw.w + raw.x * raw.x + raw.y * raw.y + raw.z * raw.z;
    if (!std::isfinite(squared) || squared < 1.0e-10F) return {0, 0, 0};
    const float inverse = 1.0F / std::sqrt(squared);
    const float w = raw.w * inverse;
    const float x = raw.x * inverse;
    const float y = raw.y * inverse;
    const float z = raw.z * inverse;
    // Starfield's view-forward axis is local +Y; roll is consequently local Y.
    return {
        2.0F * (x * y - w * z),
        1.0F - 2.0F * (x * x + z * z),
        2.0F * (y * z + w * x),
    };
}

Vector3 Right(const HeadMath::Quaternion& raw) noexcept {
    const float squared = raw.w * raw.w + raw.x * raw.x + raw.y * raw.y + raw.z * raw.z;
    if (!std::isfinite(squared) || squared < 1.0e-10F) return {0, 0, 0};
    const float inverse = 1.0F / std::sqrt(squared);
    const float w = raw.w * inverse;
    const float x = raw.x * inverse;
    const float y = raw.y * inverse;
    const float z = raw.z * inverse;
    return {
        1.0F - 2.0F * (y * y + z * z),
        2.0F * (x * y + w * z),
        2.0F * (x * z - w * y),
    };
}

Vector3 Up(const HeadMath::Quaternion& raw) noexcept {
    const float squared = raw.w * raw.w + raw.x * raw.x + raw.y * raw.y + raw.z * raw.z;
    if (!std::isfinite(squared) || squared < 1.0e-10F) return {0, 0, 0};
    const float inverse = 1.0F / std::sqrt(squared);
    const float w = raw.w * inverse;
    const float x = raw.x * inverse;
    const float y = raw.y * inverse;
    const float z = raw.z * inverse;
    return {
        2.0F * (x * z + w * y),
        2.0F * (y * z - w * x),
        1.0F - 2.0F * (x * x + y * y),
    };
}

BaseResult CurrentBase(const Config& config, CameraFrame& frame) noexcept {
    std::lock_guard lock(g_stateMutex);
    frame = g_frame;
    const auto now = SteadyMilliseconds();
    if (!frame.valid || frame.timestampMilliseconds <= 0 || now < frame.timestampMilliseconds ||
        now - frame.timestampMilliseconds > kFrameFreshMilliseconds || !g_launchBase.valid ||
        g_launchBase.timestampMilliseconds <= 0 || now < g_launchBase.timestampMilliseconds ||
        now - g_launchBase.timestampMilliseconds > kFrameFreshMilliseconds) {
        return {};
    }

    // The detached HUD marker and direct-fire ballistics must consume one
    // authoritative cone decision. Clamp once in ship-local space using the
    // shared rim history, then transform that exact ray into the native launch
    // basis. Re-clamping a reconstructed world camera ray here previously kept
    // the original ship-center fallback logically separate from the marker.
    const BaseResult localBase = AimRestrictionPolicy::ComputeBaseRay(
        {0.0F, 1.0F, 0.0F}, frame.localCameraForward,
        config.limitAimToShipForwardCone,
        config.firingConeHalfAngleDegrees, g_localRimHistory);
    if (!localBase.valid) return {};

    frame.shipForward = g_launchBase.forward;
    frame.cameraForward = AimRestrictionPolicy::TransformDirection(
        g_launchBase.right, g_launchBase.forward, g_launchBase.up,
        frame.localCameraForward);

    BaseResult worldBase = localBase;
    const Vector3 transformed = AimRestrictionPolicy::TransformDirection(
        g_launchBase.right, g_launchBase.forward, g_launchBase.up,
        localBase.ray);
    if (!AimRestrictionPolicy::Normalize(transformed, worldBase.ray)) return {};
    return worldBase;
}

BaseResult CurrentLocalBase(const Config& config, CameraFrame& frame) noexcept {
    std::lock_guard lock(g_stateMutex);
    frame = g_frame;
    const auto now = SteadyMilliseconds();
    if (!frame.valid || frame.timestampMilliseconds <= 0 || now < frame.timestampMilliseconds ||
        now - frame.timestampMilliseconds > kFrameFreshMilliseconds) {
        return {};
    }
    return AimRestrictionPolicy::ComputeBaseRay(
        {0.0F, 1.0F, 0.0F}, frame.localCameraForward, config.limitAimToShipForwardCone,
        config.firingConeHalfAngleDegrees, g_localRimHistory);
}

void PublishDecision(Mode mode, Reason reason, const CameraFrame& frame, float dot, float angle,
                     float halfAngle, bool fresh) noexcept {
    std::lock_guard lock(g_stateMutex);
    g_lastDecision = {
        mode,
        reason,
        frame.generation,
        dot,
        angle,
        halfAngle,
        fresh,
        g_installed.load(std::memory_order_acquire),
    };
}

TargetResult EvaluateTarget(const CameraFrame& frame, const Vector3& candidate, float halfAngle,
                            bool hud) noexcept {
    std::lock_guard lock(g_stateMutex);
    bool& previous = hud ? g_hudTargetEligible : g_ballisticTargetEligible;
    const auto result =
        AimRestrictionPolicy::EvaluateTarget(frame.shipForward, candidate, halfAngle, previous);
    previous = result.eligible;
    return result;
}

void InvalidateTarget(bool hud) noexcept {
    std::lock_guard lock(g_stateMutex);
    (hud ? g_hudTargetEligible : g_ballisticTargetEligible) = false;
}

void ReleaseBaseReticle(void* owner) noexcept {
    if (!g_baseReticleOwned.exchange(false, std::memory_order_acq_rel)) return;
    {
        std::lock_guard lock(g_stateMutex);
        // HookedHudUpdate runs the native update first. If that update produced
        // an eligible target, its aim-assist visibility now owns this field.
        if (g_hudTargetEligible) return;
    }
    auto* rawModel = HudModel(owner);
    if (!rawModel) return;
    SetHudBool(reinterpret_cast<std::uintptr_t>(rawModel) + 0x1E0, false);
}

bool IsTaggedTarget(const Vector3& candidate) noexcept {
    std::lock_guard lock(g_stateMutex);
    const auto now = SteadyMilliseconds();
    if (!g_targetTag.valid || now < g_targetTag.timestampMilliseconds ||
        now - g_targetTag.timestampMilliseconds > kTargetTagFreshMilliseconds) {
        g_targetTag.valid = false;
        return false;
    }
    Vector3 normalizedCandidate;
    Vector3 normalizedTag;
    if (!AimRestrictionPolicy::Normalize(candidate, normalizedCandidate) ||
        !AimRestrictionPolicy::Normalize(g_targetTag.direction, normalizedTag)) {
        return false;
    }
    const bool matches = AimRestrictionPolicy::Dot(normalizedCandidate, normalizedTag) > 0.9999F;
    if (matches && !g_loggedTargetMatch.exchange(true, std::memory_order_acq_rel)) {
        RuntimePaths::Log("[AimRestriction]",
                          "First target-linked solution reached projectile consumer.", true);
    }
    return matches;
}

void ApplyLaunchPolicy(void* rawContext) noexcept {
    if (!g_installed.load(std::memory_order_acquire) || !rawContext) return;
    const Config config = Configuration::Get();
    CameraFrame frame;
    const BaseResult base = CurrentBase(config, frame);
    if (!base.valid) {
        InvalidateTarget(false);
        return;
    }
    const auto context = reinterpret_cast<std::uintptr_t>(rawContext);
    float pitch = 0.0F;
    float yaw = 0.0F;
    if (!SafeRead(context + 0x24, pitch) || !SafeRead(context + 0x2C, yaw) ||
        !std::isfinite(pitch) || !std::isfinite(yaw)) {
        InvalidateTarget(false);
        PublishDecision(base.mode, Reason::InvalidSolution, frame, -1.0F, 180.0F,
                        config.firingConeHalfAngleDegrees, true);
        return;
    }

    const Vector3 candidate = AimRestrictionPolicy::DirectionFromLaunchAngles(pitch, yaw);
    const bool taggedTarget = IsTaggedTarget(candidate);
    if (!config.limitAimToShipForwardCone) {
        InvalidateTarget(false);
        if (taggedTarget) {
            const float targetDot =
                std::clamp(AimRestrictionPolicy::Dot(frame.shipForward, candidate), -1.0F, 1.0F);
            PublishDecision(Mode::TargetAssisted, Reason::Disabled, frame, targetDot,
                            AimRestrictionPolicy::AngleDegrees(targetDot),
                            config.firingConeHalfAngleDegrees, true);
            return;
        }
        PublishDecision(base.mode, Reason::NoTarget, frame, base.shipCameraDot, base.angleDegrees,
                        config.firingConeHalfAngleDegrees, true);
        // The completed native context already contains the unrestricted camera
        // or target solution. An explicit opt-out must be a genuine pass-through.
        return;
    } else if (taggedTarget) {
        const TargetResult target =
            EvaluateTarget(frame, candidate, config.firingConeHalfAngleDegrees, false);
        if (target.eligible) {
            PublishDecision(Mode::TargetAssisted, Reason::Eligible, frame, target.shipTargetDot,
                            target.angleDegrees, config.firingConeHalfAngleDegrees, true);
            return;
        }
        PublishDecision(base.mode, target.reason, frame, target.shipTargetDot, target.angleDegrees,
                        config.firingConeHalfAngleDegrees, true);
    } else {
        InvalidateTarget(false);
        PublishDecision(base.mode, Reason::NoTarget, frame, base.shipCameraDot, base.angleDegrees,
                        config.firingConeHalfAngleDegrees, true);
        if (base.mode == Mode::CameraDirected) {
            // Preserve Starfield's completed camera-directed solution verbatim
            // while it lies inside the cone. Only cone-rim fallback needs a write.
            return;
        }
    }

    if (g_directionToAnglesOriginal) {
        float boundedYaw = 0.0F;
        float boundedPitch = 0.0F;
        g_directionToAnglesOriginal(&base.ray, &boundedYaw, &boundedPitch);
        if (std::isfinite(boundedYaw) && std::isfinite(boundedPitch)) {
            if (!g_loggedLaunchBasis.exchange(true, std::memory_order_acq_rel)) {
                RuntimePaths::Log(
                    "[AimRestriction]",
                    std::format(
                        "First direct-fire basis: native pitch/yaw={:.4f}/{:.4f}, "
                        "ship=({:.4f},{:.4f},{:.4f}), camera=({:.4f},{:.4f},{:.4f}), "
                        "local camera=({:.4f},{:.4f},{:.4f}), angle={:.2f}, "
                        "target={}, bounded pitch/yaw={:.4f}/{:.4f}.",
                        pitch, yaw, frame.shipForward.x, frame.shipForward.y,
                        frame.shipForward.z, frame.cameraForward.x, frame.cameraForward.y,
                        frame.cameraForward.z, frame.localCameraForward.x,
                        frame.localCameraForward.y, frame.localCameraForward.z,
                        base.angleDegrees, taggedTarget, boundedPitch, boundedYaw),
                    true);
            }
            SafeCopy(reinterpret_cast<void*>(context + 0x2C), &boundedYaw, sizeof(boundedYaw));
            SafeCopy(reinterpret_cast<void*>(context + 0x24), &boundedPitch, sizeof(boundedPitch));
        }
    }
}

void* HookedProjectileConsumer(void* owner, void* context, std::uint32_t flags) {
    ApplyLaunchPolicy(context);
    return g_projectileConsumerOriginal ? g_projectileConsumerOriginal(owner, context, flags)
                                        : nullptr;
}

bool HookedMatrixToAngles(const void* matrix, float* yaw, float* pitch, float* roll) {
    const bool result =
        g_matrixToAnglesOriginal && g_matrixToAnglesOriginal(matrix, yaw, pitch, roll);
    const auto module = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    const auto returnAddress = reinterpret_cast<std::uintptr_t>(_ReturnAddress());
    if (!module ||
        (returnAddress != module + kBaseAnglesReturnRvaA &&
         returnAddress != module + kBaseAnglesReturnRvaB) ||
        !yaw || !pitch || !roll || !std::isfinite(*yaw) || !std::isfinite(*pitch) ||
        !std::isfinite(*roll)) {
        return result;
    }

    Vector3 forward = AimRestrictionPolicy::DirectionFromLaunchAngles(*pitch, *yaw);
    Vector3 normalizedForward;
    if (!AimRestrictionPolicy::Normalize(forward, normalizedForward)) return result;

    const Vector3 levelRight{std::cos(*yaw), -std::sin(*yaw), 0.0F};
    const Vector3 levelUp{
        std::sin(*yaw) * std::sin(*pitch),
        std::cos(*yaw) * std::sin(*pitch),
        std::cos(*pitch),
    };
    const float cosineRoll = std::cos(*roll);
    const float sineRoll = std::sin(*roll);
    Vector3 right = AimRestrictionPolicy::Add(AimRestrictionPolicy::Scale(levelRight, cosineRoll),
                                              AimRestrictionPolicy::Scale(levelUp, -sineRoll));
    Vector3 up = AimRestrictionPolicy::Add(AimRestrictionPolicy::Scale(levelRight, sineRoll),
                                           AimRestrictionPolicy::Scale(levelUp, cosineRoll));
    Vector3 normalizedRight;
    Vector3 normalizedUp;
    if (!AimRestrictionPolicy::Normalize(right, normalizedRight) ||
        !AimRestrictionPolicy::Normalize(up, normalizedUp)) {
        return result;
    }

    std::lock_guard lock(g_stateMutex);
    g_launchBase = {
        normalizedForward, normalizedRight, normalizedUp, SteadyMilliseconds(), true,
    };
    return result;
}

void HookedDirectionToAngles(const Vector3* direction, float* yaw, float* pitch) {
    if (g_directionToAnglesOriginal) g_directionToAnglesOriginal(direction, yaw, pitch);
    const auto module = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    if (!module ||
        reinterpret_cast<std::uintptr_t>(_ReturnAddress()) != module + kTargetAngleReturnRva ||
        !direction)
        return;
    Vector3 normalized;
    if (!AimRestrictionPolicy::Normalize(*direction, normalized)) {
        std::lock_guard lock(g_stateMutex);
        g_targetTag.valid = false;
        return;
    }
    {
        std::lock_guard lock(g_stateMutex);
        g_targetTag = {normalized, SteadyMilliseconds(), true};
    }
    if (!g_loggedTargetTag.exchange(true, std::memory_order_acq_rel)) {
        RuntimePaths::Log("[AimRestriction]", "First target-linked solution captured.", true);
    }
}

bool HookedHudTargetSolver(void* target, std::uint32_t id, const Vector3* origin, float interval,
                           void* transform, Vector3* aimPoint) {
    const bool nativeValid =
        g_hudTargetSolverOriginal &&
        g_hudTargetSolverOriginal(target, id, origin, interval, transform, aimPoint);
    const auto module = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    if (!module ||
        reinterpret_cast<std::uintptr_t>(_ReturnAddress()) != module + kHudTargetSolverReturnRva)
        return nativeValid;
    if (!nativeValid) {
        InvalidateTarget(true);
        return false;
    }

    const Config config = Configuration::Get();
    CameraFrame frame;
    const BaseResult base = CurrentBase(config, frame);
    if (!base.valid) {
        InvalidateTarget(true);
        return nativeValid;
    }
    if (!config.limitAimToShipForwardCone) {
        InvalidateTarget(true);
        PublishDecision(Mode::Native, Reason::Disabled, frame, 1.0F, 0.0F,
                        config.firingConeHalfAngleDegrees, true);
        return true;
    }
    if (!origin || !aimPoint) {
        InvalidateTarget(true);
        return false;
    }
    Vector3 rawOrigin;
    Vector3 rawAim;
    if (!SafeCopy(&rawOrigin, origin, sizeof(rawOrigin)) ||
        !SafeCopy(&rawAim, aimPoint, sizeof(rawAim))) {
        InvalidateTarget(true);
        return false;
    }
    const Vector3 direction{
        rawAim.x - rawOrigin.x,
        rawAim.y - rawOrigin.y,
        rawAim.z - rawOrigin.z,
    };
    const TargetResult result =
        EvaluateTarget(frame, direction, config.firingConeHalfAngleDegrees, true);
    PublishDecision(result.eligible ? Mode::TargetAssisted : Mode::ConeLimited, result.reason,
                    frame, result.shipTargetDot, result.angleDegrees,
                    config.firingConeHalfAngleDegrees, true);
    return result.eligible;
}

void UpdateBaseReticle(void* owner) noexcept {
    const Config config = Configuration::Get();
    CameraFrame frame;
    const BaseResult base = CurrentLocalBase(config, frame);
    if (!base.valid) {
        ReleaseBaseReticle(owner);
        return;
    }
    if (!config.limitAimToShipForwardCone ||
        base.angleDegrees <= config.firingConeHalfAngleDegrees) {
        ReleaseBaseReticle(owner);
        return;
    }

    // Preserve a real, eligible native target solution. When there is no such
    // target, the otherwise overlaid lead circle becomes the independent
    // bounded-aim marker while shipDirectionCrosshair remains native VJoy data.
    {
        std::lock_guard lock(g_stateMutex);
        if (g_hudTargetEligible) {
            // The native update has replaced our marker with its target
            // solution; relinquish ownership without hiding native state.
            g_baseReticleOwned.store(false, std::memory_order_release);
            return;
        }
    }

    auto* rawModel = HudModel(owner);
    if (!rawModel) return;
    const auto model = reinterpret_cast<std::uintptr_t>(rawModel);

    float x = 0.5F;
    float y = 0.5F;
    Vector3 cameraRay;
    const bool projected = ProjectBaseRayToHud(frame, base.ray, cameraRay, x, y);
    if (!projected) {
        // The Scaleform mapping clamps coordinates to the visible rectangle.
        // Supply a far coordinate in the true azimuth so the marker reaches the
        // correct edge instead of claiming an unrelated center-screen solution.
        const float magnitude = std::hypot(cameraRay.x, cameraRay.z);
        x = magnitude > 1.0e-4F ? 0.5F + cameraRay.x / magnitude * 50.0F : 50.5F;
        y = magnitude > 1.0e-4F ? 0.5F - cameraRay.z / magnitude * 50.0F : 0.5F;
    }
    if (!g_loggedHudProjection.exchange(true, std::memory_order_acq_rel)) {
        RuntimePaths::Log(
            "[AimRestriction]",
            std::format("First cone-limited HUD ray: camera=({:.4f},{:.4f},{:.4f}), "
                        "projected={}, lead=({:.4f},{:.4f}).",
                        cameraRay.x, cameraRay.y, cameraRay.z, projected, x, y),
            true);
    }

    SetHudFloat(model + 0x140, x);
    SetHudFloat(model + 0x160, y);
    if (SetHudBool(model + 0x1E0, true)) {
        g_baseReticleOwned.store(true, std::memory_order_release);
    }
}

void HookedHudUpdate(void* owner) {
    if (g_hudUpdateOriginal) g_hudUpdateOriginal(owner);
    UpdateBaseReticle(owner);
}

bool ValidateBuild(std::uintptr_t module) noexcept {
    return module &&
           Matches(module + kProjectileConsumerRva,
                   std::array<std::uint8_t, 12>{0x48, 0x8B, 0xC4, 0x48, 0x89, 0x58, 0x18, 0x48,
                                                0x89, 0x48, 0x08, 0x55}) &&
           Matches(module + kMatrixToAnglesRva,
                   std::array<std::uint8_t, 10>{0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x6C,
                                                0x24, 0x10}) &&
           Matches(module + kBaseAnglesReturnRvaA - 5,
                   std::array<std::uint8_t, 5>{0xE8, 0x97, 0xA0, 0x07, 0x01}) &&
           Matches(module + kBaseAnglesReturnRvaB - 5,
                   std::array<std::uint8_t, 5>{0xE8, 0x67, 0xA0, 0x07, 0x01}) &&
           Matches(module + kDirectionToAnglesRva,
                   std::array<std::uint8_t, 10>{0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x74,
                                                0x24, 0x10}) &&
           Matches(module + kHudUpdateRva,
                   std::array<std::uint8_t, 16>{0x48, 0x8B, 0xC4, 0x48, 0x89, 0x58, 0x08, 0x55,
                                                0x56, 0x57, 0x41, 0x54, 0x41, 0x55, 0x41, 0x56}) &&
           Matches(module + kHudTargetSolverRva,
                   std::array<std::uint8_t, 10>{0x48, 0x89, 0x5C, 0x24, 0x08, 0x57, 0x48, 0x83,
                                                0xEC, 0x70}) &&
           Matches(module + kHudTargetSolverCallRva,
                    std::array<std::uint8_t, 5>{0xE8, 0x11, 0x4F, 0xBB, 0x00}) &&
           Matches(module + kGetActiveCameraRva,
                   std::array<std::uint8_t, 12>{0x80, 0x3D, 0xBF, 0x55, 0xBE, 0x04, 0x00, 0x74,
                                                0x0B, 0x48, 0x8B, 0x49}) &&
           Matches(module + kProjectWorldToHudRva,
                   std::array<std::uint8_t, 12>{0x48, 0x83, 0xEC, 0x38, 0xC5, 0xFA, 0x10, 0x22,
                                                0xC5, 0xDA, 0x59, 0x89}) &&
           Matches(module + kHudGetActiveCameraCallRva,
                   std::array<std::uint8_t, 5>{0xE8, 0x9C, 0x6B, 0xD9, 0xFF}) &&
           Matches(module + kHudWorldToScreenCallRva,
                   std::array<std::uint8_t, 5>{0xE8, 0xEC, 0x9F, 0x68, 0x01}) &&
           Matches(module + kHudModelRegistrationRva,
                   std::array<std::uint8_t, 10>{0x48, 0x89, 0x5C, 0x24, 0x08, 0x57, 0x48, 0x83,
                                                0xEC, 0x20}) &&
           Matches(module + kBaseAngleWriterRva,
                   std::array<std::uint8_t, 10>{0xC5, 0xFA, 0x11, 0x47, 0x24, 0xC5, 0xFA, 0x11,
                                                0x4F, 0x2C}) &&
           Matches(module + kTargetAngleWriterRva,
                   std::array<std::uint8_t, 8>{0xC5, 0xFB, 0x11, 0x47, 0x24, 0x8B, 0x45, 0x18});
}

bool CreateHook(std::size_t index, std::uintptr_t target, void* detour, void** original) noexcept {
    void* const candidate = reinterpret_cast<void*>(target);
    if (MH_CreateHook(candidate, detour, original) != MH_OK) return false;
    g_hookTargets[index] = candidate;
    return true;
}

void RollBackHooks() noexcept {
    for (void*& target : g_hookTargets) {
        if (!target) continue;
        MH_DisableHook(target);
        MH_RemoveHook(target);
        target = nullptr;
    }
    g_projectileConsumerOriginal = nullptr;
    g_matrixToAnglesOriginal = nullptr;
    g_directionToAnglesOriginal = nullptr;
    g_hudUpdateOriginal = nullptr;
    g_hudTargetSolverOriginal = nullptr;
    g_getActiveCamera = nullptr;
    g_projectWorldToHud = nullptr;
    g_activeCameraManagerPointer = 0;
}
} // namespace

namespace AimRestriction {

bool Install() {
    if (g_installed.load(std::memory_order_acquire)) return true;
    g_loggedLaunchBasis.store(false, std::memory_order_release);
    g_loggedTargetTag.store(false, std::memory_order_release);
    g_loggedTargetMatch.store(false, std::memory_order_release);
    g_loggedHudProjection.store(false, std::memory_order_release);
    g_baseReticleOwned.store(false, std::memory_order_release);
    const auto module = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    if (!ValidateBuild(module)) {
        RuntimePaths::Log(
            "[AimRestriction]",
            "Exact gunnery/HUD signature gate failed; off-axis tracking remains disabled.", true);
        return false;
    }
    g_getActiveCamera =
        reinterpret_cast<GetActiveCameraFunction>(module + kGetActiveCameraRva);
    g_projectWorldToHud =
        reinterpret_cast<ProjectWorldToHudFunction>(module + kProjectWorldToHudRva);
    g_activeCameraManagerPointer = module + kActiveCameraManagerPointerRva;

    const MH_STATUS initialize = MH_Initialize();
    if (initialize != MH_OK && initialize != MH_ERROR_ALREADY_INITIALIZED) {
        RuntimePaths::Log("[AimRestriction]", "MinHook initialization failed.", true);
        return false;
    }
    if (!CreateHook(0, module + kProjectileConsumerRva,
                    reinterpret_cast<void*>(&HookedProjectileConsumer),
                    reinterpret_cast<void**>(&g_projectileConsumerOriginal)) ||
        !CreateHook(1, module + kMatrixToAnglesRva, reinterpret_cast<void*>(&HookedMatrixToAngles),
                    reinterpret_cast<void**>(&g_matrixToAnglesOriginal)) ||
        !CreateHook(2, module + kDirectionToAnglesRva,
                    reinterpret_cast<void*>(&HookedDirectionToAngles),
                    reinterpret_cast<void**>(&g_directionToAnglesOriginal)) ||
        !CreateHook(3, module + kHudUpdateRva, reinterpret_cast<void*>(&HookedHudUpdate),
                    reinterpret_cast<void**>(&g_hudUpdateOriginal)) ||
        !CreateHook(4, module + kHudTargetSolverRva,
                    reinterpret_cast<void*>(&HookedHudTargetSolver),
                    reinterpret_cast<void**>(&g_hudTargetSolverOriginal))) {
        RollBackHooks();
        RuntimePaths::Log(
            "[AimRestriction]",
            "Could not create the complete gunnery/HUD hook set; all owned hooks rolled back.",
            true);
        return false;
    }
    for (void* target : g_hookTargets) {
        if (MH_EnableHook(target) != MH_OK) {
            RollBackHooks();
            RuntimePaths::Log(
                "[AimRestriction]",
                "Could not enable the complete gunnery/HUD hook set; all owned hooks rolled back.",
                true);
            return false;
        }
    }
    g_installed.store(true, std::memory_order_release);
    RuntimePaths::Log(
        "[AimRestriction]",
        "Validated ballistic, target-solution, base-reticle, and aim-assist hooks installed.",
        true);
    return true;
}

void Shutdown() {
    g_installed.store(false, std::memory_order_release);
    ClearCameraFrame();
    RollBackHooks();
}

bool Ready() noexcept {
    return g_installed.load(std::memory_order_acquire);
}

void PublishCameraFrame(const HeadMath::Quaternion& headRotation) noexcept {
    const Vector3 right = Right(headRotation);
    const Vector3 camera = Forward(headRotation);
    const Vector3 up = Up(headRotation);
    Vector3 normalizedRight;
    Vector3 normalizedCamera;
    Vector3 normalizedUp;
    if (!AimRestrictionPolicy::Normalize(right, normalizedRight) ||
        !AimRestrictionPolicy::Normalize(camera, normalizedCamera) ||
        !AimRestrictionPolicy::Normalize(up, normalizedUp)) {
        ClearCameraFrame();
        return;
    }
    std::lock_guard lock(g_stateMutex);
    g_frame.localRight = normalizedRight;
    g_frame.localCameraForward = normalizedCamera;
    g_frame.localUp = normalizedUp;
    g_frame.shipForward = {};
    g_frame.cameraForward = {};
    g_frame.timestampMilliseconds = SteadyMilliseconds();
    ++g_frame.generation;
    g_frame.valid = g_installed.load(std::memory_order_acquire);
}

void ClearCameraFrame() noexcept {
    std::lock_guard lock(g_stateMutex);
    g_frame.valid = false;
    g_frame.timestampMilliseconds = 0;
    ++g_frame.generation;
    g_launchBase = {};
    g_targetTag = {};
    g_hudTargetEligible = false;
    g_ballisticTargetEligible = false;
    g_lastDecision.mode = Mode::Native;
    g_lastDecision.reason = Reason::Disabled;
    g_lastDecision.generation = g_frame.generation;
    g_lastDecision.fresh = false;
    g_lastDecision.buildValidated = g_installed.load(std::memory_order_acquire);
}

void ResetSpatialState() noexcept {
    std::lock_guard lock(g_stateMutex);
    g_localRimHistory = {};
    g_launchBase = {};
    g_targetTag = {};
    g_hudTargetEligible = false;
    g_ballisticTargetEligible = false;
}

Status GetStatus() {
    std::lock_guard lock(g_stateMutex);
    const auto now = SteadyMilliseconds();
    const bool fresh = g_frame.valid && g_frame.timestampMilliseconds > 0 &&
                       now >= g_frame.timestampMilliseconds &&
                       now - g_frame.timestampMilliseconds <= kFrameFreshMilliseconds;
    return {
        g_installed.load(std::memory_order_acquire),
        fresh,
        g_lastDecision,
    };
}

} // namespace AimRestriction
