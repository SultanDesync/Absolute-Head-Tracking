#pragma once

#include "Config.h"

namespace HotkeyPolicy {

constexpr bool IsBound(const HotkeySettings& hotkey) noexcept {
    return hotkey.virtualKey > 0;
}

constexpr bool Same(const HotkeySettings& left, const HotkeySettings& right) noexcept {
    return IsBound(left) && IsBound(right) &&
        left.virtualKey == right.virtualKey &&
        left.control == right.control &&
        left.alt == right.alt &&
        left.shift == right.shift;
}

} // namespace HotkeyPolicy
