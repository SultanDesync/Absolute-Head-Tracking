#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace AbsoluteInputBusApi {

inline constexpr std::uint32_t kAbiVersion = 1;
inline constexpr std::size_t kAxisCount = 8;
inline constexpr std::size_t kButtonCount = 128;
inline constexpr std::size_t kPovCount = 4;
inline constexpr std::size_t kPovDirectionCount = 16;
inline constexpr std::size_t kDigitalControlCount =
    kButtonCount + kPovDirectionCount;
inline constexpr std::size_t kDigitalWordCount = 3;
inline constexpr std::size_t kConsumerIdCapacity = 64;
inline constexpr std::size_t kDeviceIdCapacity = 48;
inline constexpr std::size_t kDeviceNameCapacity = 128;
inline constexpr std::size_t kBindingTextCapacity = 256;
inline constexpr std::size_t kProfileIdCapacity = 96;
inline constexpr std::size_t kDetailCapacity = 192;

inline constexpr std::uint64_t kCapabilityDeviceEnumeration = 1ULL << 0;
inline constexpr std::uint64_t kCapabilitySnapshots = 1ULL << 1;
inline constexpr std::uint64_t kCapabilityButtonCapture = 1ULL << 2;
inline constexpr std::uint64_t kCapabilityAxisCapture = 1ULL << 3;
inline constexpr std::uint64_t kCapabilityProfileIdentity = 1ULL << 4;
inline constexpr std::uint64_t kCapabilityRuntimeContext = 1ULL << 5;
inline constexpr std::uint64_t kCapabilitiesV1 =
    kCapabilityDeviceEnumeration | kCapabilitySnapshots |
    kCapabilityButtonCapture | kCapabilityAxisCapture |
    kCapabilityProfileIdentity | kCapabilityRuntimeContext;

inline constexpr std::uint32_t kCaptureButtons = 1U << 0;
inline constexpr std::uint32_t kCapturePovDirections = 1U << 1;
inline constexpr std::uint32_t kCaptureAxes = 1U << 2;
inline constexpr std::uint32_t kCaptureDigital =
    kCaptureButtons | kCapturePovDirections;
inline constexpr std::uint32_t kCaptureAll = kCaptureDigital | kCaptureAxes;

enum class Result : std::uint32_t {
    Ok, NotReady, InvalidArgument, NotFound, Busy, StaleSession,
};
enum class ControlKind : std::uint32_t {
    None, Button, PovDirection, Axis,
};
enum class CaptureState : std::uint32_t {
    Idle, Capturing, Captured, Cancelled, TimedOut, Error,
};
enum class RuntimeContext : std::uint32_t {
    Unknown, Suspended, OnFoot, Piloting,
};

inline constexpr std::uint32_t kContextSignalIsPilot = 1U << 0;
inline constexpr std::uint32_t kContextSignalGameplayActive = 1U << 1;
inline constexpr std::uint32_t kContextSignalTargetingMode = 1U << 2;
inline constexpr std::uint32_t kContextSourceAutomaticPilot = 1U << 0;

struct DeviceInfoV1 {
    std::uint32_t structSize{sizeof(DeviceInfoV1)};
    std::uint32_t deviceIndex{};
    std::uint8_t instanceGuid[16]{};
    std::uint8_t productGuid[16]{};
    std::uint16_t vendorId{};
    std::uint16_t productId{};
    std::uint32_t axisCount{};
    std::uint32_t buttonCount{};
    std::uint32_t povCount{kPovCount};
    char persistentId[kDeviceIdCapacity]{};
    char instanceName[kDeviceNameCapacity]{};
    char productName[kDeviceNameCapacity]{};
};

struct DeviceSnapshotV1 {
    std::uint32_t structSize{sizeof(DeviceSnapshotV1)};
    std::uint32_t deviceIndex{};
    std::uint64_t sequence{};
    std::uint64_t producerGeneration{};
    std::uint8_t connected{};
    std::uint8_t reserved[7]{};
    std::int32_t rawAxes[kAxisCount]{};
    std::int32_t axisMinimum[kAxisCount]{};
    std::int32_t axisMaximum[kAxisCount]{};
    float normalizedAxes[kAxisCount]{};
    std::int32_t povHundredths[kPovCount]{};
    std::uint64_t digitalDown[kDigitalWordCount]{};
    std::uint32_t pressCount[kDigitalControlCount]{};
    std::uint32_t releaseCount[kDigitalControlCount]{};
};

struct BindingV1 {
    std::uint32_t structSize{sizeof(BindingV1)};
    ControlKind kind{ControlKind::None};
    std::uint32_t deviceIndex{};
    std::uint32_t controlId{};
    std::uint8_t instanceGuid[16]{};
    char persistentId[kDeviceIdCapacity]{};
    char productName[kDeviceNameCapacity]{};
    char bindingText[kBindingTextCapacity]{};
};

struct CaptureRequestV1 {
    std::uint32_t structSize{sizeof(CaptureRequestV1)};
    std::uint32_t allowedControls{kCaptureDigital};
    std::uint32_t settleMilliseconds{50};
    std::uint32_t timeoutMilliseconds{8000};
    char consumerId[kConsumerIdCapacity]{};
};

struct CaptureResultV1 {
    std::uint32_t structSize{sizeof(CaptureResultV1)};
    CaptureState state{CaptureState::Idle};
    std::uint64_t sessionId{};
    BindingV1 binding{};
    char detail[kDetailCapacity]{};
};

struct ProfileStateV1 {
    std::uint32_t structSize{sizeof(ProfileStateV1)};
    std::uint32_t activeSlot{};
    std::uint64_t generation{};
    char profileId[kProfileIdCapacity]{};
};

struct RuntimeContextV1 {
    std::uint32_t structSize{sizeof(RuntimeContextV1)};
    RuntimeContext context{RuntimeContext::Unknown};
    std::uint64_t sequence{};
    std::uint64_t producerGeneration{};
    std::uint64_t contextGeneration{};
    std::uint32_t validSignals{};
    std::uint32_t activeSignals{};
    std::uint32_t sourceFlags{};
    std::uint32_t reserved{};
    std::int64_t selectedOutputAgeMilliseconds{-1};
};

struct ApiV1 {
    std::uint32_t structSize{sizeof(ApiV1)};
    std::uint32_t abiVersion{kAbiVersion};
    const char* providerId{};
    const char* displayName{};
    const char* version{};
    std::uint64_t capabilities{kCapabilitiesV1};
    std::uint32_t(__cdecl* getDeviceCount)() noexcept{};
    Result(__cdecl* getDevice)(std::uint32_t, DeviceInfoV1*) noexcept{};
    Result(__cdecl* getSnapshot)(std::uint32_t, DeviceSnapshotV1*) noexcept{};
    Result(__cdecl* getProfileState)(ProfileStateV1*) noexcept{};
    Result(__cdecl* getRuntimeContext)(RuntimeContextV1*) noexcept{};
    Result(__cdecl* beginCapture)(const CaptureRequestV1*, std::uint64_t*) noexcept{};
    Result(__cdecl* pollCapture)(std::uint64_t, CaptureResultV1*) noexcept{};
    Result(__cdecl* cancelCapture)(std::uint64_t) noexcept{};
};

static_assert(std::is_standard_layout_v<ApiV1>);
static_assert(std::is_trivially_copyable_v<DeviceInfoV1>);
static_assert(std::is_trivially_copyable_v<DeviceSnapshotV1>);
static_assert(std::is_trivially_copyable_v<BindingV1>);
static_assert(std::is_trivially_copyable_v<CaptureRequestV1>);
static_assert(std::is_trivially_copyable_v<CaptureResultV1>);
static_assert(std::is_trivially_copyable_v<ProfileStateV1>);
static_assert(std::is_trivially_copyable_v<RuntimeContextV1>);

} // namespace AbsoluteInputBusApi

using AbsoluteInputBusQueryApi = const AbsoluteInputBusApi::ApiV1* (__cdecl*)(
    std::uint32_t) noexcept;
