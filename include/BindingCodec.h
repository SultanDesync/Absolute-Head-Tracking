#pragma once

#include "Config.h"

#include <string>
#include <string_view>

namespace BindingCodec {
[[nodiscard]] bool ParseKeyboard(std::string_view encoded, HotkeySettings& output) noexcept;
[[nodiscard]] std::string EncodeKeyboard(const HotkeySettings& hotkey);
[[nodiscard]] std::string Display(const HotkeySettings& hotkey);
} // namespace BindingCodec
