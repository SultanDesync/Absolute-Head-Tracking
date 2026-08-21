#include "Config.h"

#include "InputBusBinding.h"

#include <algorithm>

namespace ConfigPolicy {

Config Defaults() {
    return {};
}

void Sanitize(Config& config) {
    config.pollRateHz = std::clamp(config.pollRateHz, 30, 500);
    config.staleMilliseconds = std::clamp(config.staleMilliseconds, 50, 5000);
    config.deadzoneDegrees = std::clamp(config.deadzoneDegrees, 0.0F, 20.0F);
    config.smoothing = std::clamp(config.smoothing, 0.0F, 0.99F);

    auto sanitizeAxis = [](AxisSettings& axis) {
        axis.sensitivity = std::clamp(axis.sensitivity, 0.05F, 10.0F);
        axis.maximumDegrees = std::clamp(axis.maximumDegrees, 1.0F, 180.0F);
    };
    sanitizeAxis(config.yaw);
    sanitizeAxis(config.pitch);
    sanitizeAxis(config.roll);

    auto sanitizeHotkey = [](HotkeySettings& hotkey) {
        hotkey.virtualKey = std::clamp(hotkey.virtualKey, 0, 0xFF);
    };
    sanitizeHotkey(config.workbench);
    sanitizeHotkey(config.recenter);
    sanitizeHotkey(config.toggle);

    InputBusBinding::Parsed binding;
    if (!config.recenterControllerBinding.empty() &&
        !InputBusBinding::Parse(config.recenterControllerBinding, binding)) {
        config.recenterControllerBinding.clear();
    }
    if (!config.toggleControllerBinding.empty() &&
        !InputBusBinding::Parse(config.toggleControllerBinding, binding)) {
        config.toggleControllerBinding.clear();
    }
}

bool ShouldBlockForAbsoluteHOTAS(bool moduleLoaded, bool dllPresent) noexcept {
    return moduleLoaded || dllPresent;
}

} // namespace ConfigPolicy
