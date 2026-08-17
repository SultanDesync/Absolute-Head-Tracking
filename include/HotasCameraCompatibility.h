#pragma once

#include <cstdint>

namespace HotasCameraCompatibility {

enum class Result {
    Connected,
    NotLoaded,
    Incompatible,
};

[[nodiscard]] Result Connect() noexcept;
[[nodiscard]] bool Connected() noexcept;
[[nodiscard]] std::int64_t __cdecl CockpitSignalAgeMilliseconds() noexcept;

} // namespace HotasCameraCompatibility
