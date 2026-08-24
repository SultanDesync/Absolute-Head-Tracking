#pragma once

#include <cstdint>
#include <string>

struct AxisSettings {
    bool enabled = true;
    bool inverted = false;
    float sensitivity = 1.0F;
    float minimumDegrees = -85.0F;
    float centerDegrees = 0.0F;
    float maximumDegrees = 85.0F;
};

struct HotkeySettings {
    int virtualKey = 0;
    bool control = true;
    bool alt = true;
    bool shift = false;
};

enum class MouseLookModifier : int {
    Disabled = 0,
    Alt = 1,
    Control = 2,
    Shift = 3,
};

struct ManualLookSettings {
    float digitalRateDegreesPerSecond = 60.0F;
    float mouseDegreesPerCount = 0.08F;
    MouseLookModifier mouseModifier = MouseLookModifier::Disabled;
    bool invertMouseHorizontal = false;
    bool invertMouseVertical = false;
    float controllerAxisDeadzone = 0.08F;
    bool invertControllerYaw = false;
    bool invertControllerPitch = true;
    bool invertControllerRoll = false;

    HotkeySettings lookLeft{ 0, false, false, false };
    HotkeySettings lookRight{ 0, false, false, false };
    HotkeySettings lookDown{ 0, false, false, false };
    HotkeySettings lookUp{ 0, false, false, false };
    HotkeySettings bankLeft{ 0, false, false, false };
    HotkeySettings bankRight{ 0, false, false, false };

    std::string yawControllerAxisBinding;
    std::string pitchControllerAxisBinding;
    std::string rollControllerAxisBinding;
    std::string lookLeftControllerBinding;
    std::string lookRightControllerBinding;
    std::string lookDownControllerBinding;
    std::string lookUpControllerBinding;
    std::string bankLeftControllerBinding;
    std::string bankRightControllerBinding;
};

struct Config {
    bool enabled = true;
    bool enableLog = false;
    bool enableWorkbench = true;
    int pollRateHz = 120;
    int staleMilliseconds = 500;
    float deadzoneDegrees = 0.0F;
    float smoothing = 0.15F;
    bool limitAimToShipForwardCone = true;
    float firingConeHalfAngleDegrees = 30.0F;
    AxisSettings yaw{ true, false, 1.0F, -85.0F, 0.0F, 85.0F };
    // FreeTrack/OpenTrack pitch uses the opposite sign from Starfield's camera pitch.
    AxisSettings pitch{ true, true, 1.0F, -60.0F, 0.0F, 60.0F };
    AxisSettings roll{ true, false, 1.0F, -45.0F, 0.0F, 45.0F };
    HotkeySettings workbench{ 0x48, true, true, false };
    HotkeySettings recenter{ 0x43, true, true, false };
    HotkeySettings toggle{ 0x54, true, true, false };
    std::string recenterControllerBinding;
    std::string toggleControllerBinding;
    ManualLookSettings manualLook;
};

namespace ConfigPolicy {
Config Defaults();
void Sanitize(Config& config);
bool ShouldBlockForAbsoluteHOTAS(bool moduleLoaded, bool dllPresent) noexcept;
} // namespace ConfigPolicy

namespace Configuration {
bool Load();
bool Save();
Config Get();
void Apply(const Config& config);
bool Dirty();
std::uint64_t Generation();
} // namespace Configuration
