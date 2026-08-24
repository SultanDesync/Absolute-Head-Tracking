#include "Config.h"
#include "HotkeyPolicy.h"

#include <cstdio>
#include <cstdlib>

#define CHECK(expression) do { if (!(expression)) { \
    std::fprintf(stderr, "CHECK failed at line %d: %s\n", __LINE__, #expression); \
    std::exit(1); } } while (false)

int main() {
    CHECK(ConfigPolicy::Defaults().pitch.inverted);
    CHECK(ConfigPolicy::Defaults().yaw.minimumDegrees == -85.0F);
    CHECK(ConfigPolicy::Defaults().yaw.centerDegrees == 0.0F);
    CHECK(ConfigPolicy::Defaults().yaw.maximumDegrees == 85.0F);
    CHECK(ConfigPolicy::Defaults().pitch.minimumDegrees == -60.0F);
    CHECK(ConfigPolicy::Defaults().pitch.maximumDegrees == 60.0F);
    CHECK(ConfigPolicy::Defaults().manualLook.mouseModifier ==
        MouseLookModifier::Disabled);
    CHECK(ConfigPolicy::Defaults().manualLook.invertControllerPitch);
    Config config;
    config.pollRateHz = 1;
    config.staleMilliseconds = 99999;
    config.smoothing = 4.0F;
    config.deadzoneDegrees = -10.0F;
    config.firingConeHalfAngleDegrees = 200.0F;
    config.yaw.sensitivity = 0.0F;
    config.pitch.minimumDegrees = 90.0F;
    config.pitch.centerDegrees = 200.0F;
    config.pitch.maximumDegrees = -500.0F;
    config.workbench.virtualKey = 999;
    config.recenterControllerBinding = "invalid";
    config.toggleControllerBinding =
        "{12345678-1234-1234-1234-123456789ABC}@button:33";
    config.manualLook.digitalRateDegreesPerSecond = 1000.0F;
    config.manualLook.mouseDegreesPerCount = -2.0F;
    config.manualLook.controllerAxisDeadzone = 2.0F;
    config.manualLook.mouseModifier = static_cast<MouseLookModifier>(99);
    config.manualLook.yawControllerAxisBinding = "device@button:1";
    config.manualLook.pitchControllerAxisBinding = "device@axis:0x31";
    config.manualLook.lookLeftControllerBinding = "device@axis:0x30";
    config.manualLook.lookRightControllerBinding = "device@pov:0:right";
    ConfigPolicy::Sanitize(config);
    CHECK(config.pollRateHz == 30);
    CHECK(config.staleMilliseconds == 5000);
    CHECK(config.smoothing == 0.99F);
    CHECK(config.deadzoneDegrees == 0.0F);
    CHECK(config.firingConeHalfAngleDegrees == 90.0F);
    CHECK(config.yaw.sensitivity == 0.05F);
    CHECK(config.pitch.minimumDegrees == -180.0F);
    CHECK(config.pitch.centerDegrees == 90.0F);
    CHECK(config.pitch.maximumDegrees == 90.0F);
    CHECK(config.workbench.virtualKey == 0xFF);
    CHECK(config.recenterControllerBinding.empty());
    CHECK(!config.toggleControllerBinding.empty());
    CHECK(config.manualLook.digitalRateDegreesPerSecond == 360.0F);
    CHECK(config.manualLook.mouseDegreesPerCount == 0.001F);
    CHECK(config.manualLook.controllerAxisDeadzone == 0.95F);
    CHECK(config.manualLook.mouseModifier == MouseLookModifier::Shift);
    CHECK(config.manualLook.yawControllerAxisBinding.empty());
    CHECK(!config.manualLook.pitchControllerAxisBinding.empty());
    CHECK(config.manualLook.lookLeftControllerBinding.empty());
    CHECK(!config.manualLook.lookRightControllerBinding.empty());

    CHECK(!ConfigPolicy::ShouldBlockForAbsoluteHOTAS(false, false));
    CHECK(ConfigPolicy::ShouldBlockForAbsoluteHOTAS(true, false));
    CHECK(ConfigPolicy::ShouldBlockForAbsoluteHOTAS(false, true));

    const HotkeySettings ctrlOne{ 0x31, true, false, false };
    const HotkeySettings anotherCtrlOne{ 0x31, true, false, false };
    const HotkeySettings one{ 0x31, false, false, false };
    const HotkeySettings unbound{ 0, false, false, false };
    CHECK(HotkeyPolicy::IsBound(ctrlOne));
    CHECK(!HotkeyPolicy::IsBound(unbound));
    CHECK(HotkeyPolicy::Same(ctrlOne, anotherCtrlOne));
    CHECK(!HotkeyPolicy::Same(ctrlOne, one));
    CHECK(!HotkeyPolicy::Same(unbound, unbound));
    return 0;
}
