#pragma once

#include <algorithm>
#include <cstdint>

namespace CameraPolicy {
inline bool CockpitSignalFresh(std::int64_t ageMilliseconds,
                               std::int64_t maximumAgeMilliseconds = 400) noexcept {
    return ageMilliseconds >= 0 &&
        ageMilliseconds <= std::max<std::int64_t>(0, maximumAgeMilliseconds);
}
} // namespace CameraPolicy
