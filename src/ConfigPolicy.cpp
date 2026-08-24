#include "Config.h"

#include "InputBusBinding.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace ConfigPolicy {

Config Defaults() {
    return {};
}

void Sanitize(Config& config) {
    config.pollRateHz = std::clamp(config.pollRateHz, 30, 500);
    config.staleMilliseconds = std::clamp(config.staleMilliseconds, 50, 5000);
    config.deadzoneDegrees = std::clamp(config.deadzoneDegrees, 0.0F, 20.0F);
    config.smoothing = std::clamp(config.smoothing, 0.0F, 0.99F);
    config.firingConeHalfAngleDegrees = std::clamp(
        config.firingConeHalfAngleDegrees, 1.0F, 90.0F);

    auto sanitizeAxis = [](AxisSettings& axis) {
        axis.sensitivity = std::clamp(axis.sensitivity, 0.05F, 10.0F);
        if (!std::isfinite(axis.minimumDegrees)) axis.minimumDegrees = -85.0F;
        if (!std::isfinite(axis.centerDegrees)) axis.centerDegrees = 0.0F;
        if (!std::isfinite(axis.maximumDegrees)) axis.maximumDegrees = 85.0F;
        axis.minimumDegrees = std::clamp(axis.minimumDegrees, -180.0F, 180.0F);
        axis.maximumDegrees = std::clamp(axis.maximumDegrees, -180.0F, 180.0F);
        if (axis.minimumDegrees > axis.maximumDegrees)
            std::swap(axis.minimumDegrees, axis.maximumDegrees);
        axis.centerDegrees = std::clamp(
            axis.centerDegrees, axis.minimumDegrees, axis.maximumDegrees);
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
    sanitizeHotkey(config.manualLook.lookLeft);
    sanitizeHotkey(config.manualLook.lookRight);
    sanitizeHotkey(config.manualLook.lookDown);
    sanitizeHotkey(config.manualLook.lookUp);
    sanitizeHotkey(config.manualLook.bankLeft);
    sanitizeHotkey(config.manualLook.bankRight);

    if (!std::isfinite(config.manualLook.digitalRateDegreesPerSecond))
        config.manualLook.digitalRateDegreesPerSecond = 60.0F;
    if (!std::isfinite(config.manualLook.mouseDegreesPerCount))
        config.manualLook.mouseDegreesPerCount = 0.08F;
    if (!std::isfinite(config.manualLook.controllerAxisDeadzone))
        config.manualLook.controllerAxisDeadzone = 0.08F;
    config.manualLook.digitalRateDegreesPerSecond = std::clamp(
        config.manualLook.digitalRateDegreesPerSecond, 1.0F, 360.0F);
    config.manualLook.mouseDegreesPerCount = std::clamp(
        config.manualLook.mouseDegreesPerCount, 0.001F, 2.0F);
    config.manualLook.controllerAxisDeadzone = std::clamp(
        config.manualLook.controllerAxisDeadzone, 0.0F, 0.95F);
    config.manualLook.mouseModifier = static_cast<MouseLookModifier>(std::clamp(
        static_cast<int>(config.manualLook.mouseModifier),
        static_cast<int>(MouseLookModifier::Disabled),
        static_cast<int>(MouseLookModifier::Shift)));

    InputBusBinding::Parsed binding;
    if (!config.recenterControllerBinding.empty() &&
        (!InputBusBinding::Parse(config.recenterControllerBinding, binding) ||
            !InputBusBinding::IsDigital(binding))) {
        config.recenterControllerBinding.clear();
    }
    if (!config.toggleControllerBinding.empty() &&
        (!InputBusBinding::Parse(config.toggleControllerBinding, binding) ||
            !InputBusBinding::IsDigital(binding))) {
        config.toggleControllerBinding.clear();
    }

    const std::array digitalBindings{
        &config.manualLook.lookLeftControllerBinding,
        &config.manualLook.lookRightControllerBinding,
        &config.manualLook.lookDownControllerBinding,
        &config.manualLook.lookUpControllerBinding,
        &config.manualLook.bankLeftControllerBinding,
        &config.manualLook.bankRightControllerBinding,
    };
    for (auto* text : digitalBindings) {
        if (!text->empty() &&
            (!InputBusBinding::Parse(*text, binding) ||
                !InputBusBinding::IsDigital(binding))) {
            text->clear();
        }
    }
    const std::array axisBindings{
        &config.manualLook.yawControllerAxisBinding,
        &config.manualLook.pitchControllerAxisBinding,
        &config.manualLook.rollControllerAxisBinding,
    };
    for (auto* text : axisBindings) {
        if (!text->empty() &&
            (!InputBusBinding::Parse(*text, binding) ||
                binding.kind != InputBusBinding::Kind::Axis)) {
            text->clear();
        }
    }
}

bool ShouldBlockForAbsoluteHOTAS(bool moduleLoaded, bool dllPresent) noexcept {
    return moduleLoaded || dllPresent;
}

} // namespace ConfigPolicy
