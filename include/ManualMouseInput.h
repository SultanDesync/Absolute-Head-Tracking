#pragma once

#include <cstdint>

namespace ManualMouseInput {

struct Deltas {
    std::int64_t horizontal{};
    std::int64_t vertical{};
};

[[nodiscard]] bool Install() noexcept;
[[nodiscard]] bool Available() noexcept;
void Shutdown() noexcept;
void SetExclusive(bool exclusive) noexcept;
[[nodiscard]] Deltas Consume() noexcept;

} // namespace ManualMouseInput
