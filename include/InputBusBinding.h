#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace InputBusBinding {

enum class Kind : std::uint8_t {
    Button,
    PovDirection,
    Axis,
};

struct Parsed {
    Kind kind{Kind::Button};
    std::string persistentId;
    std::uint32_t controlId{};
    std::uint32_t digitalChannel{};
    std::uint32_t povIndex{};
    std::uint32_t povDirection{};
    std::uint32_t axisIndex{};
    std::uint32_t axisUsage{};
};

struct PressCounter {
    std::uint64_t producerGeneration{};
    std::uint32_t value{};
    bool initialized{};
};

[[nodiscard]] bool Parse(std::string_view binding, Parsed& output) noexcept;
[[nodiscard]] bool IsDigital(const Parsed& binding) noexcept;
[[nodiscard]] std::string ControlLabel(const Parsed& binding);

// Rebase on first observation and producer restart. Unsigned subtraction keeps
// the counter correct across uint32_t wraparound. Suppression consumes edges so
// a menu capture cannot fire an action after the menu closes.
[[nodiscard]] std::uint32_t ObservePresses(PressCounter& state,
    std::uint64_t producerGeneration, std::uint32_t current,
    bool suppressed) noexcept;

} // namespace InputBusBinding
