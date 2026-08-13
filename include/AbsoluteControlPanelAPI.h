#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <type_traits>

namespace AbsoluteControlPanelApi {
inline constexpr std::uint32_t kAbiVersion = 1;
inline constexpr std::string_view kModuleId = "absolute.control_panel";
inline constexpr std::size_t kIdentifierCapacity = 64;
inline constexpr std::size_t kLabelCapacity = 96;
inline constexpr std::size_t kDescriptionCapacity = 192;
inline constexpr std::size_t kStringValueCapacity = 256;

enum class Result : std::uint32_t {
    Ok, NotReady, InvalidArgument, Duplicate, NotFound,
    CapacityExceeded, Rejected, WriteFailure
};
enum class ControlKind : std::uint32_t {
    Toggle, IntegerSlider, FloatSlider, Choice, Action, InputBinding,
    ButtonBinding = InputBinding
};
enum class ValueKind : std::uint32_t { Boolean, Integer, Float, String };
enum ControlFlags : std::uint32_t {
    kControlNone = 0,
    kControlReadOnly = 1U << 0,
    kControlRequiresRestart = 1U << 1,
    kControlAdvanced = 1U << 2,
    kBindingKeyboard = 1U << 8,
    kBindingMouse = 1U << 9,
    kBindingController = 1U << 10,
    kBindingModifiers = 1U << 11,
    kBindingClearable = 1U << 12
};
struct ValueV1 {
    std::uint32_t structSize{sizeof(ValueV1)};
    ValueKind kind{ValueKind::String};
    std::uint32_t booleanValue{};
    std::int64_t integerValue{};
    double floatValue{};
    char stringValue[kStringValueCapacity]{};
};
struct ControlDescriptorV1 {
    std::uint32_t structSize{sizeof(ControlDescriptorV1)};
    ControlKind kind{ControlKind::Toggle};
    std::uint32_t flags{kControlNone};
    char controlId[kIdentifierCapacity]{};
    char label[kLabelCapacity]{};
    char description[kDescriptionCapacity]{};
    double minimumValue{};
    double maximumValue{};
    double stepValue{};
};
using ReadValueCallback = Result(__cdecl*)(void*, const char*, ValueV1*) noexcept;
using WriteDraftCallback = Result(__cdecl*)(void*, const char*, const ValueV1*) noexcept;
using InvokeActionCallback = Result(__cdecl*)(void*, const char*) noexcept;
using ApplyCallback = Result(__cdecl*)(void*) noexcept;
using CancelCallback = void(__cdecl*)(void*) noexcept;
struct ModuleDescriptorV1 {
    std::uint32_t structSize{sizeof(ModuleDescriptorV1)};
    char moduleId[kIdentifierCapacity]{};
    char displayName[kLabelCapacity]{};
    char description[kDescriptionCapacity]{};
};
struct PageDescriptorV1 {
    std::uint32_t structSize{sizeof(PageDescriptorV1)};
    char moduleId[kIdentifierCapacity]{};
    char pageId[kIdentifierCapacity]{};
    char displayName[kLabelCapacity]{};
    char description[kDescriptionCapacity]{};
    std::uint32_t controlCount{};
    const ControlDescriptorV1* controls{};
    void* context{};
    ReadValueCallback readValue{};
    WriteDraftCallback writeDraft{};
    InvokeActionCallback invokeAction{};
    ApplyCallback apply{};
    CancelCallback cancel{};
};
struct ApiV1 {
    std::uint32_t structSize{sizeof(ApiV1)};
    std::uint32_t abiVersion{kAbiVersion};
    const char* moduleId{};
    const char* displayName{};
    const char* version{};
    Result(__cdecl* registerPage)(const PageDescriptorV1*) noexcept{};
    Result(__cdecl* unregisterModule)(const char*) noexcept{};
    Result(__cdecl* requestRefresh)(const char*, const char*) noexcept{};
    Result(__cdecl* registerModule)(const ModuleDescriptorV1*) noexcept{};
    std::uint8_t(__cdecl* isOpen)() noexcept{};
    std::uint8_t(__cdecl* isInputCaptureActive)() noexcept{};
};
static_assert(std::is_trivially_copyable_v<ValueV1>);
static_assert(std::is_trivially_copyable_v<ControlDescriptorV1>);
static_assert(std::is_trivially_copyable_v<ModuleDescriptorV1>);
static_assert(std::is_standard_layout_v<PageDescriptorV1>);
static_assert(std::is_standard_layout_v<ApiV1>);
} // namespace AbsoluteControlPanelApi

using AbsoluteControlPanelQueryApi = const AbsoluteControlPanelApi::ApiV1* (__cdecl*)(
    std::uint32_t) noexcept;
