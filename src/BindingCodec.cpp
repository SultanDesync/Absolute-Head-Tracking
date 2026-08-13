#include "BindingCodec.h"

#include <charconv>
#include <format>

namespace {
bool ParseBoolean(std::string_view text, bool& output) noexcept {
    if (text == "0") { output = false; return true; }
    if (text == "1") { output = true; return true; }
    return false;
}
}

namespace BindingCodec {
bool ParseKeyboard(std::string_view encoded, HotkeySettings& output) noexcept {
    if (encoded.empty()) {
        output = {0, false, false, false};
        return true;
    }
    constexpr std::string_view prefix = "keyboard:0x";
    if (!encoded.starts_with(prefix)) return false;
    encoded.remove_prefix(prefix.size());
    const auto first = encoded.find(';');
    if (first == std::string_view::npos) return false;
    int key{};
    const auto keyText = encoded.substr(0, first);
    const auto [keyEnd, keyError] = std::from_chars(
        keyText.data(), keyText.data() + keyText.size(), key, 16);
    if (keyError != std::errc{} || keyEnd != keyText.data() + keyText.size() ||
        key <= 0 || key > 0xFF) return false;

    encoded.remove_prefix(first + 1);
    bool control{}, alt{}, shift{};
    constexpr std::string_view ctrlPrefix = "ctrl=";
    constexpr std::string_view altPrefix = "alt=";
    constexpr std::string_view shiftPrefix = "shift=";
    if (!encoded.starts_with(ctrlPrefix)) return false;
    encoded.remove_prefix(ctrlPrefix.size());
    const auto ctrlEnd = encoded.find(';');
    if (ctrlEnd == std::string_view::npos ||
        !ParseBoolean(encoded.substr(0, ctrlEnd), control)) return false;
    encoded.remove_prefix(ctrlEnd + 1);
    if (!encoded.starts_with(altPrefix)) return false;
    encoded.remove_prefix(altPrefix.size());
    const auto altEnd = encoded.find(';');
    if (altEnd == std::string_view::npos ||
        !ParseBoolean(encoded.substr(0, altEnd), alt)) return false;
    encoded.remove_prefix(altEnd + 1);
    if (!encoded.starts_with(shiftPrefix)) return false;
    encoded.remove_prefix(shiftPrefix.size());
    if (!ParseBoolean(encoded, shift)) return false;
    output = {key, control, alt, shift};
    return true;
}

std::string EncodeKeyboard(const HotkeySettings& hotkey) {
    if (hotkey.virtualKey <= 0) return {};
    return std::format("keyboard:0x{:02X};ctrl={};alt={};shift={}",
        hotkey.virtualKey, hotkey.control ? 1 : 0, hotkey.alt ? 1 : 0,
        hotkey.shift ? 1 : 0);
}

std::string Display(const HotkeySettings& hotkey) {
    if (hotkey.virtualKey <= 0) return "Unbound";
    std::string result;
    const auto append = [&](std::string_view part) {
        if (!result.empty()) result += " + ";
        result += part;
    };
    if (hotkey.control) append("Ctrl");
    if (hotkey.alt) append("Alt");
    if (hotkey.shift) append("Shift");
    if (hotkey.virtualKey >= 'A' && hotkey.virtualKey <= 'Z') {
        append(std::string(1, static_cast<char>(hotkey.virtualKey)));
    } else if (hotkey.virtualKey >= '0' && hotkey.virtualKey <= '9') {
        append(std::string(1, static_cast<char>(hotkey.virtualKey)));
    } else if (hotkey.virtualKey >= 0x70 && hotkey.virtualKey <= 0x87) {
        append(std::format("F{}", hotkey.virtualKey - 0x70 + 1));
    } else {
        append(std::format("Key 0x{:02X}", hotkey.virtualKey));
    }
    return result;
}
} // namespace BindingCodec
