#pragma once

#include "AbsoluteInputBusAPI.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace InputBusClient {

enum class ConnectResult : std::uint8_t {
    Connected,
    NotInstalled,
    Incompatible,
};

struct ActionEdges {
    std::uint32_t togglePresses{};
    std::uint32_t recenterPresses{};
};

[[nodiscard]] ConnectResult Connect() noexcept;
[[nodiscard]] bool Connected() noexcept;
[[nodiscard]] bool IsDigitalBinding(std::string_view binding) noexcept;
[[nodiscard]] std::string DisplayBinding(std::string_view binding);

// Reads monotonic press counters. First observation, producer restarts, binding
// changes, and suppressed menu-capture intervals are baselines rather than
// synthetic actions.
[[nodiscard]] ActionEdges PollActions(std::string_view toggleBinding,
    std::string_view recenterBinding, bool suppressed) noexcept;
[[nodiscard]] std::int64_t __cdecl PilotSignalAgeMilliseconds() noexcept;

[[nodiscard]] AbsoluteInputBusApi::Result BeginDigitalCapture() noexcept;
[[nodiscard]] AbsoluteInputBusApi::Result PollCapture(
    AbsoluteInputBusApi::CaptureResultV1& result) noexcept;
[[nodiscard]] AbsoluteInputBusApi::Result CancelCapture() noexcept;

} // namespace InputBusClient
