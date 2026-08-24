#include "InputBusBinding.h"

#include <charconv>
#include <format>

namespace {
bool ParseUnsigned(std::string_view text, std::uint32_t& output) noexcept {
    if (text.empty()) return false;
    const auto [end, error] = std::from_chars(
        text.data(), text.data() + text.size(), output);
    return error == std::errc{} && end == text.data() + text.size();
}

bool ParseHex(std::string_view text, std::uint32_t& output) noexcept {
    if (!text.starts_with("0x") || text.size() <= 2) return false;
    text.remove_prefix(2);
    const auto [end, error] = std::from_chars(
        text.data(), text.data() + text.size(), output, 16);
    return error == std::errc{} && end == text.data() + text.size();
}
} // namespace

namespace InputBusBinding {

bool Parse(std::string_view binding, Parsed& output) noexcept {
    try {
        const auto separator = binding.find('@');
        if (separator == std::string_view::npos || separator == 0 ||
            separator >= 48 || binding.find('@', separator + 1) !=
                std::string_view::npos) {
            return false;
        }
        Parsed parsed;
        parsed.persistentId.assign(binding.substr(0, separator));
        auto control = binding.substr(separator + 1);
        constexpr std::string_view buttonPrefix = "button:";
        constexpr std::string_view povPrefix = "pov:";
        constexpr std::string_view axisPrefix = "axis:";
        if (control.starts_with(buttonPrefix)) {
            control.remove_prefix(buttonPrefix.size());
            if (!ParseUnsigned(control, parsed.controlId) ||
                parsed.controlId == 0 || parsed.controlId > 128) {
                return false;
            }
            parsed.kind = Kind::Button;
            parsed.digitalChannel = parsed.controlId - 1;
        } else if (control.starts_with(povPrefix)) {
            control.remove_prefix(povPrefix.size());
            const auto directionSeparator = control.find(':');
            if (directionSeparator == std::string_view::npos ||
                !ParseUnsigned(control.substr(0, directionSeparator),
                    parsed.povIndex) || parsed.povIndex > 3) {
                return false;
            }
            const auto direction = control.substr(directionSeparator + 1);
            if (direction == "up") parsed.povDirection = 0;
            else if (direction == "right") parsed.povDirection = 1;
            else if (direction == "down") parsed.povDirection = 2;
            else if (direction == "left") parsed.povDirection = 3;
            else return false;
            parsed.kind = Kind::PovDirection;
            parsed.digitalChannel = 128 + parsed.povIndex * 4 +
                parsed.povDirection;
            parsed.controlId = parsed.digitalChannel + 1;
        } else if (control.starts_with(axisPrefix)) {
            control.remove_prefix(axisPrefix.size());
            if (!ParseHex(control, parsed.axisUsage) ||
                parsed.axisUsage < 0x30 || parsed.axisUsage > 0x37) {
                return false;
            }
            parsed.kind = Kind::Axis;
            parsed.axisIndex = parsed.axisUsage - 0x30;
            parsed.controlId = parsed.axisUsage;
        } else {
            return false;
        }
        output = std::move(parsed);
        return true;
    } catch (...) {
        return false;
    }
}

bool IsDigital(const Parsed& binding) noexcept {
    return binding.kind == Kind::Button ||
        binding.kind == Kind::PovDirection;
}

std::string ControlLabel(const Parsed& binding) {
    if (binding.kind == Kind::Button) {
        return std::format("Button {}", binding.controlId);
    }
    if (binding.kind == Kind::PovDirection) {
        constexpr const char* directions[]{"Up", "Right", "Down", "Left"};
        return std::format("POV {} {}", binding.povIndex + 1,
            directions[binding.povDirection]);
    }
    constexpr const char* axes[]{"X", "Y", "Z", "Rx", "Ry", "Rz",
        "Slider 1", "Slider 2"};
    return std::format("{} axis", axes[binding.axisIndex]);
}

std::uint32_t ObservePresses(PressCounter& state,
    std::uint64_t producerGeneration, std::uint32_t current,
    bool suppressed) noexcept {
    if (!state.initialized ||
        state.producerGeneration != producerGeneration) {
        state.producerGeneration = producerGeneration;
        state.value = current;
        state.initialized = true;
        return 0;
    }
    const std::uint32_t presses = current - state.value;
    state.value = current;
    return suppressed ? 0U : presses;
}

} // namespace InputBusBinding
