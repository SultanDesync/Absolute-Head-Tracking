#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace AbsoluteHeadTrackingApi {

inline constexpr std::uint32_t kAbiVersion = 1;
inline constexpr std::size_t kAxisCount = 3;

enum class Result : std::uint32_t {
    Ok,
    NotReady,
    UnsupportedRuntime,
    InactiveByHOTAS,
    TrackerUnavailable,
    InvalidArgument,
    WriteFailure,
    Rejected,
};

enum class ConnectionState : std::uint32_t {
    Waiting,
    Connected,
    Stale,
};

struct HotkeyV1 {
    std::uint32_t virtualKey{};
    std::uint8_t control{};
    std::uint8_t alt{};
    std::uint8_t shift{};
    std::uint8_t reserved{};
};

struct AxisConfigurationV1 {
    std::uint8_t enabled{};
    std::uint8_t inverted{};
    std::uint8_t reserved[2]{};
    float sensitivity{1.0F};
    float maximumDegrees{85.0F};
};

struct ConfigurationV1 {
    std::uint32_t structSize{sizeof(ConfigurationV1)};
    std::uint8_t enabled{};
    std::uint8_t enableLog{};
    std::uint8_t enableStandaloneWorkbench{};
    std::uint8_t reserved{};
    std::uint32_t pollRateHz{};
    std::uint32_t staleMilliseconds{};
    float deadzoneDegrees{};
    float smoothing{};
    AxisConfigurationV1 axes[kAxisCount]{};
    HotkeyV1 legacyWorkbench{};
    HotkeyV1 recenter{};
    HotkeyV1 toggle{};
};

struct StatusV1 {
    std::uint32_t structSize{sizeof(StatusV1)};
    std::uint8_t cameraHookInstalled{};
    std::uint8_t cockpitObserverInstalled{};
    std::uint8_t poseActive{};
    std::uint8_t runtimeEnabled{};
    std::uint8_t configurationDirty{};
    std::uint8_t inactiveByHOTAS{};
    std::uint8_t hosted{};
    std::uint8_t inputCaptureActive{};
    ConnectionState connection{ConnectionState::Waiting};
    std::uint32_t reserved{};
    std::int64_t cockpitSignalAgeMilliseconds{-1};
    std::int64_t trackerSampleAgeMilliseconds{-1};
    std::uint64_t configurationGeneration{};
};

struct PoseV1 {
    std::uint32_t structSize{sizeof(PoseV1)};
    std::uint8_t centered{};
    std::uint8_t reserved[3]{};
    float trackerDegrees[kAxisCount]{};
    float outputDegrees[kAxisCount]{};
};

struct ApiV1 {
    std::uint32_t structSize{sizeof(ApiV1)};
    std::uint32_t abiVersion{kAbiVersion};
    const char* moduleId{};
    const char* displayName{};
    const char* version{};

    Result(__cdecl* getStatus)(StatusV1*) noexcept{};
    Result(__cdecl* getPose)(PoseV1*) noexcept{};
    Result(__cdecl* getConfiguration)(ConfigurationV1*) noexcept{};
    Result(__cdecl* applyConfiguration)(const ConfigurationV1*) noexcept{};
    Result(__cdecl* saveConfiguration)() noexcept{};
    Result(__cdecl* reloadConfiguration)() noexcept{};
    Result(__cdecl* requestRecenter)() noexcept{};
    Result(__cdecl* setRuntimeEnabled)(std::uint8_t) noexcept{};
    Result(__cdecl* setWorkbenchSessionState)(std::uint8_t open,
                                              std::uint8_t inputCaptureActive) noexcept{};
};

static_assert(std::is_standard_layout_v<ApiV1>);
static_assert(std::is_trivially_copyable_v<ConfigurationV1>);
static_assert(std::is_trivially_copyable_v<StatusV1>);
static_assert(std::is_trivially_copyable_v<PoseV1>);

} // namespace AbsoluteHeadTrackingApi

#if defined(ABSOLUTE_HEAD_TRACKING_EXPORTS)
#define ABSOLUTE_HEAD_TRACKING_API __declspec(dllexport)
#else
#define ABSOLUTE_HEAD_TRACKING_API __declspec(dllimport)
#endif
extern "C" ABSOLUTE_HEAD_TRACKING_API const AbsoluteHeadTrackingApi::ApiV1*
AbsoluteHeadTracking_QueryApi(std::uint32_t requestedAbiVersion) noexcept;
