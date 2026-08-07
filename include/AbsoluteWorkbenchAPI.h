#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace AbsoluteWorkbenchApi {

inline constexpr std::uint32_t kAbiVersion = 1;
inline constexpr const char* kModuleId = "absolute.workbench";

enum class Result : std::uint32_t {
    Ok,
    NotReady,
    InvalidArgument,
    NotFound,
    Rejected,
    InternalError,
};

enum class HostMode : std::uint32_t {
    Active,
    Suppressed,
    Unavailable,
    Incompatible,
};

struct HostApiV1 {
    std::uint32_t structSize{sizeof(HostApiV1)};
    std::uint32_t abiVersion{kAbiVersion};
    const char* moduleId{kModuleId};
    const char* version{};

    HostMode(__cdecl* getHostMode)() noexcept{};
    Result(__cdecl* requestOpen)(const char* moduleId, const char* pageId) noexcept{};
    std::uint8_t(__cdecl* isOpen)() noexcept{};
    std::uint8_t(__cdecl* isInputCaptureActive)() noexcept{};

    const void* reserved[8]{};
};

using QueryHostApi = const HostApiV1*(__cdecl*)(std::uint32_t) noexcept;

static_assert(std::is_standard_layout_v<HostApiV1>);
static_assert(std::is_trivially_copyable_v<HostApiV1>);

} // namespace AbsoluteWorkbenchApi
