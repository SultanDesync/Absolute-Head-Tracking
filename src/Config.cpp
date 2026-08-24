#include "PCH.h"

#include "Config.h"
#include "RuntimePaths.h"

#include <SimpleIni.h>

namespace {
std::mutex g_mutex;
Config g_config = ConfigPolicy::Defaults();
bool g_dirty = false;
std::uint64_t g_generation = 0;

int ReadInteger(CSimpleIniA& ini, const char* section, const char* key, int fallback) {
    const char* text = ini.GetValue(section, key, nullptr);
    if (!text || !*text) return fallback;
    try {
        return std::stoi(text, nullptr, 0);
    } catch (...) {
        return fallback;
    }
}

float ReadFloat(CSimpleIniA& ini, const char* section, const std::string& key,
                float fallback) {
    return static_cast<float>(ini.GetDoubleValue(section, key.c_str(), fallback));
}

void ReadAxisRange(CSimpleIniA& ini, const char* axisName, AxisSettings& axis) {
    const std::string prefix = std::string("f") + axisName;
    const std::string minimumKey = prefix + "MinimumDegrees";
    const std::string centerKey = prefix + "CenterDegrees";
    const std::string maximumKey = prefix + "MaximumDegrees";
    const std::string legacyKey = std::string("fMax") + axisName + "Degrees";

    const bool hasMinimum = ini.GetValue("Tracking", minimumKey.c_str(), nullptr) != nullptr;
    const bool hasMaximum = ini.GetValue("Tracking", maximumKey.c_str(), nullptr) != nullptr;
    if (!hasMinimum && !hasMaximum) {
        const float legacyMaximum = std::abs(ReadFloat(
            ini, "Tracking", legacyKey, axis.maximumDegrees));
        axis.minimumDegrees = -legacyMaximum;
        axis.maximumDegrees = legacyMaximum;
    } else {
        axis.minimumDegrees = ReadFloat(
            ini, "Tracking", minimumKey, axis.minimumDegrees);
        axis.maximumDegrees = ReadFloat(
            ini, "Tracking", maximumKey, axis.maximumDegrees);
    }
    axis.centerDegrees = ReadFloat(
        ini, "Tracking", centerKey, axis.centerDegrees);
}

void WriteAxisRange(CSimpleIniA& ini, const char* axisName,
                    const AxisSettings& axis) {
    const std::string prefix = std::string("f") + axisName;
    ini.SetDoubleValue("Tracking", (prefix + "MinimumDegrees").c_str(),
        axis.minimumDegrees, nullptr, true);
    ini.SetDoubleValue("Tracking", (prefix + "CenterDegrees").c_str(),
        axis.centerDegrees, nullptr, true);
    ini.SetDoubleValue("Tracking", (prefix + "MaximumDegrees").c_str(),
        axis.maximumDegrees, nullptr, true);
}

HotkeySettings ReadHotkey(CSimpleIniA& ini, const char* prefix,
                           const HotkeySettings& fallback) {
    HotkeySettings result = fallback;
    const std::string key = std::string("i") + prefix + "Key";
    const std::string control = std::string("b") + prefix + "Ctrl";
    const std::string alt = std::string("b") + prefix + "Alt";
    const std::string shift = std::string("b") + prefix + "Shift";
    result.virtualKey = ReadInteger(ini, "Hotkeys", key.c_str(), fallback.virtualKey);
    result.control = ini.GetBoolValue("Hotkeys", control.c_str(), fallback.control);
    result.alt = ini.GetBoolValue("Hotkeys", alt.c_str(), fallback.alt);
    result.shift = ini.GetBoolValue("Hotkeys", shift.c_str(), fallback.shift);
    return result;
}

void WriteHotkey(CSimpleIniA& ini, const char* prefix, const HotkeySettings& hotkey) {
    const std::string key = std::string("i") + prefix + "Key";
    const std::string control = std::string("b") + prefix + "Ctrl";
    const std::string alt = std::string("b") + prefix + "Alt";
    const std::string shift = std::string("b") + prefix + "Shift";
    ini.SetLongValue("Hotkeys", key.c_str(), hotkey.virtualKey, nullptr, true);
    ini.SetBoolValue("Hotkeys", control.c_str(), hotkey.control, nullptr, true);
    ini.SetBoolValue("Hotkeys", alt.c_str(), hotkey.alt, nullptr, true);
    ini.SetBoolValue("Hotkeys", shift.c_str(), hotkey.shift, nullptr, true);
}
} // namespace

namespace Configuration {

bool Load() {
    Config loaded = ConfigPolicy::Defaults();
    CSimpleIniA ini;
    ini.SetUnicode();
    const auto path = RuntimePaths::IniPath().string();
    const SI_Error result = ini.LoadFile(path.c_str());
    if (result < 0) {
        RuntimePaths::Log("[Config]", "INI missing or unreadable; using safe defaults.", true);
    } else {
        loaded.enabled = ini.GetBoolValue("General", "bEnabled", loaded.enabled);
        loaded.enableLog = ini.GetBoolValue("General", "bEnableLog", loaded.enableLog);
        loaded.pollRateHz = ReadInteger(ini, "General", "iPollRateHz", loaded.pollRateHz);
        loaded.enableWorkbench = ini.GetBoolValue("UI", "bEnableWorkbench", loaded.enableWorkbench);

        loaded.yaw.enabled = ini.GetBoolValue("Tracking", "bYawEnabled", loaded.yaw.enabled);
        loaded.pitch.enabled = ini.GetBoolValue("Tracking", "bPitchEnabled", loaded.pitch.enabled);
        loaded.roll.enabled = ini.GetBoolValue("Tracking", "bRollEnabled", loaded.roll.enabled);
        loaded.yaw.inverted = ini.GetBoolValue("Tracking", "bInvertYaw", loaded.yaw.inverted);
        loaded.pitch.inverted = ini.GetBoolValue("Tracking", "bInvertPitch", loaded.pitch.inverted);
        loaded.roll.inverted = ini.GetBoolValue("Tracking", "bInvertRoll", loaded.roll.inverted);
        loaded.yaw.sensitivity = static_cast<float>(ini.GetDoubleValue(
            "Tracking", "fYawSensitivity", loaded.yaw.sensitivity));
        loaded.pitch.sensitivity = static_cast<float>(ini.GetDoubleValue(
            "Tracking", "fPitchSensitivity", loaded.pitch.sensitivity));
        loaded.roll.sensitivity = static_cast<float>(ini.GetDoubleValue(
            "Tracking", "fRollSensitivity", loaded.roll.sensitivity));
        ReadAxisRange(ini, "Yaw", loaded.yaw);
        ReadAxisRange(ini, "Pitch", loaded.pitch);
        ReadAxisRange(ini, "Roll", loaded.roll);
        loaded.deadzoneDegrees = static_cast<float>(ini.GetDoubleValue(
            "Tracking", "fDeadzoneDegrees", loaded.deadzoneDegrees));
        loaded.smoothing = static_cast<float>(ini.GetDoubleValue(
            "Tracking", "fSmoothing", loaded.smoothing));
        loaded.staleMilliseconds = ReadInteger(
            ini, "Tracking", "iStaleMilliseconds", loaded.staleMilliseconds);
        loaded.limitAimToShipForwardCone = ini.GetBoolValue(
            "AimRestriction", "bLimitAimToShipForwardCone", loaded.limitAimToShipForwardCone);
        loaded.firingConeHalfAngleDegrees = static_cast<float>(ini.GetDoubleValue(
            "AimRestriction", "fFiringConeHalfAngleDegrees", loaded.firingConeHalfAngleDegrees));
        loaded.workbench = ReadHotkey(ini, "Workbench", loaded.workbench);
        loaded.recenter = ReadHotkey(ini, "Recenter", loaded.recenter);
        loaded.toggle = ReadHotkey(ini, "Toggle", loaded.toggle);
        loaded.manualLook.lookLeft = ReadHotkey(
            ini, "LookLeft", loaded.manualLook.lookLeft);
        loaded.manualLook.lookRight = ReadHotkey(
            ini, "LookRight", loaded.manualLook.lookRight);
        loaded.manualLook.lookDown = ReadHotkey(
            ini, "LookDown", loaded.manualLook.lookDown);
        loaded.manualLook.lookUp = ReadHotkey(
            ini, "LookUp", loaded.manualLook.lookUp);
        loaded.manualLook.bankLeft = ReadHotkey(
            ini, "BankLeft", loaded.manualLook.bankLeft);
        loaded.manualLook.bankRight = ReadHotkey(
            ini, "BankRight", loaded.manualLook.bankRight);
        loaded.recenterControllerBinding = ini.GetValue(
            "Hotkeys", "sRecenterControllerBinding", "");
        loaded.toggleControllerBinding = ini.GetValue(
            "Hotkeys", "sToggleControllerBinding", "");

        loaded.manualLook.digitalRateDegreesPerSecond = ReadFloat(
            ini, "ManualLook", "fDigitalRateDegreesPerSecond",
            loaded.manualLook.digitalRateDegreesPerSecond);
        loaded.manualLook.mouseDegreesPerCount = ReadFloat(
            ini, "ManualLook", "fMouseDegreesPerCount",
            loaded.manualLook.mouseDegreesPerCount);
        loaded.manualLook.mouseModifier = static_cast<MouseLookModifier>(
            ReadInteger(ini, "ManualLook", "iMouseModifier",
                static_cast<int>(loaded.manualLook.mouseModifier)));
        loaded.manualLook.invertMouseHorizontal = ini.GetBoolValue(
            "ManualLook", "bInvertMouseHorizontal",
            loaded.manualLook.invertMouseHorizontal);
        loaded.manualLook.invertMouseVertical = ini.GetBoolValue(
            "ManualLook", "bInvertMouseVertical",
            loaded.manualLook.invertMouseVertical);
        loaded.manualLook.controllerAxisDeadzone = ReadFloat(
            ini, "ManualLook", "fControllerAxisDeadzone",
            loaded.manualLook.controllerAxisDeadzone);
        loaded.manualLook.invertControllerYaw = ini.GetBoolValue(
            "ManualLook", "bInvertControllerYaw",
            loaded.manualLook.invertControllerYaw);
        loaded.manualLook.invertControllerPitch = ini.GetBoolValue(
            "ManualLook", "bInvertControllerPitch",
            loaded.manualLook.invertControllerPitch);
        loaded.manualLook.invertControllerRoll = ini.GetBoolValue(
            "ManualLook", "bInvertControllerRoll",
            loaded.manualLook.invertControllerRoll);
        loaded.manualLook.yawControllerAxisBinding = ini.GetValue(
            "ManualLook", "sYawControllerAxisBinding", "");
        loaded.manualLook.pitchControllerAxisBinding = ini.GetValue(
            "ManualLook", "sPitchControllerAxisBinding", "");
        loaded.manualLook.rollControllerAxisBinding = ini.GetValue(
            "ManualLook", "sRollControllerAxisBinding", "");
        loaded.manualLook.lookLeftControllerBinding = ini.GetValue(
            "ManualLook", "sLookLeftControllerBinding", "");
        loaded.manualLook.lookRightControllerBinding = ini.GetValue(
            "ManualLook", "sLookRightControllerBinding", "");
        loaded.manualLook.lookDownControllerBinding = ini.GetValue(
            "ManualLook", "sLookDownControllerBinding", "");
        loaded.manualLook.lookUpControllerBinding = ini.GetValue(
            "ManualLook", "sLookUpControllerBinding", "");
        loaded.manualLook.bankLeftControllerBinding = ini.GetValue(
            "ManualLook", "sBankLeftControllerBinding", "");
        loaded.manualLook.bankRightControllerBinding = ini.GetValue(
            "ManualLook", "sBankRightControllerBinding", "");
    }

    ConfigPolicy::Sanitize(loaded);
    {
        std::lock_guard lock(g_mutex);
        g_config = loaded;
        g_dirty = false;
        ++g_generation;
    }
    RuntimePaths::Log("[Config]", std::format("Loaded {} (poll={} Hz, stale={} ms).",
        path, loaded.pollRateHz, loaded.staleMilliseconds));
    return result >= 0;
}

bool Save() {
    const Config config = Get();
    CSimpleIniA ini;
    ini.SetUnicode();
    ini.SetBoolValue("General", "bEnabled", config.enabled, nullptr, true);
    ini.SetBoolValue("General", "bEnableLog", config.enableLog, nullptr, true);
    ini.SetLongValue("General", "iPollRateHz", config.pollRateHz, nullptr, true);
    ini.SetBoolValue("UI", "bEnableWorkbench", config.enableWorkbench, nullptr, true);

    ini.SetBoolValue("Tracking", "bYawEnabled", config.yaw.enabled, nullptr, true);
    ini.SetBoolValue("Tracking", "bPitchEnabled", config.pitch.enabled, nullptr, true);
    ini.SetBoolValue("Tracking", "bRollEnabled", config.roll.enabled, nullptr, true);
    ini.SetBoolValue("Tracking", "bInvertYaw", config.yaw.inverted, nullptr, true);
    ini.SetBoolValue("Tracking", "bInvertPitch", config.pitch.inverted, nullptr, true);
    ini.SetBoolValue("Tracking", "bInvertRoll", config.roll.inverted, nullptr, true);
    ini.SetDoubleValue("Tracking", "fYawSensitivity", config.yaw.sensitivity, nullptr, true);
    ini.SetDoubleValue("Tracking", "fPitchSensitivity", config.pitch.sensitivity, nullptr, true);
    ini.SetDoubleValue("Tracking", "fRollSensitivity", config.roll.sensitivity, nullptr, true);
    WriteAxisRange(ini, "Yaw", config.yaw);
    WriteAxisRange(ini, "Pitch", config.pitch);
    WriteAxisRange(ini, "Roll", config.roll);
    ini.SetDoubleValue("Tracking", "fDeadzoneDegrees", config.deadzoneDegrees, nullptr, true);
    ini.SetDoubleValue("Tracking", "fSmoothing", config.smoothing, nullptr, true);
    ini.SetLongValue("Tracking", "iStaleMilliseconds", config.staleMilliseconds, nullptr, true);
    ini.SetBoolValue("AimRestriction", "bLimitAimToShipForwardCone",
        config.limitAimToShipForwardCone, nullptr, true);
    ini.SetDoubleValue("AimRestriction", "fFiringConeHalfAngleDegrees",
        config.firingConeHalfAngleDegrees, nullptr, true);
    WriteHotkey(ini, "Workbench", config.workbench);
    WriteHotkey(ini, "Recenter", config.recenter);
    WriteHotkey(ini, "Toggle", config.toggle);
    WriteHotkey(ini, "LookLeft", config.manualLook.lookLeft);
    WriteHotkey(ini, "LookRight", config.manualLook.lookRight);
    WriteHotkey(ini, "LookDown", config.manualLook.lookDown);
    WriteHotkey(ini, "LookUp", config.manualLook.lookUp);
    WriteHotkey(ini, "BankLeft", config.manualLook.bankLeft);
    WriteHotkey(ini, "BankRight", config.manualLook.bankRight);
    ini.SetValue("Hotkeys", "sRecenterControllerBinding",
        config.recenterControllerBinding.c_str(), nullptr, true);
    ini.SetValue("Hotkeys", "sToggleControllerBinding",
        config.toggleControllerBinding.c_str(), nullptr, true);
    ini.SetDoubleValue("ManualLook", "fDigitalRateDegreesPerSecond",
        config.manualLook.digitalRateDegreesPerSecond, nullptr, true);
    ini.SetDoubleValue("ManualLook", "fMouseDegreesPerCount",
        config.manualLook.mouseDegreesPerCount, nullptr, true);
    ini.SetLongValue("ManualLook", "iMouseModifier",
        static_cast<int>(config.manualLook.mouseModifier), nullptr, true);
    ini.SetBoolValue("ManualLook", "bInvertMouseHorizontal",
        config.manualLook.invertMouseHorizontal, nullptr, true);
    ini.SetBoolValue("ManualLook", "bInvertMouseVertical",
        config.manualLook.invertMouseVertical, nullptr, true);
    ini.SetDoubleValue("ManualLook", "fControllerAxisDeadzone",
        config.manualLook.controllerAxisDeadzone, nullptr, true);
    ini.SetBoolValue("ManualLook", "bInvertControllerYaw",
        config.manualLook.invertControllerYaw, nullptr, true);
    ini.SetBoolValue("ManualLook", "bInvertControllerPitch",
        config.manualLook.invertControllerPitch, nullptr, true);
    ini.SetBoolValue("ManualLook", "bInvertControllerRoll",
        config.manualLook.invertControllerRoll, nullptr, true);
    ini.SetValue("ManualLook", "sYawControllerAxisBinding",
        config.manualLook.yawControllerAxisBinding.c_str(), nullptr, true);
    ini.SetValue("ManualLook", "sPitchControllerAxisBinding",
        config.manualLook.pitchControllerAxisBinding.c_str(), nullptr, true);
    ini.SetValue("ManualLook", "sRollControllerAxisBinding",
        config.manualLook.rollControllerAxisBinding.c_str(), nullptr, true);
    ini.SetValue("ManualLook", "sLookLeftControllerBinding",
        config.manualLook.lookLeftControllerBinding.c_str(), nullptr, true);
    ini.SetValue("ManualLook", "sLookRightControllerBinding",
        config.manualLook.lookRightControllerBinding.c_str(), nullptr, true);
    ini.SetValue("ManualLook", "sLookDownControllerBinding",
        config.manualLook.lookDownControllerBinding.c_str(), nullptr, true);
    ini.SetValue("ManualLook", "sLookUpControllerBinding",
        config.manualLook.lookUpControllerBinding.c_str(), nullptr, true);
    ini.SetValue("ManualLook", "sBankLeftControllerBinding",
        config.manualLook.bankLeftControllerBinding.c_str(), nullptr, true);
    ini.SetValue("ManualLook", "sBankRightControllerBinding",
        config.manualLook.bankRightControllerBinding.c_str(), nullptr, true);

    const auto path = RuntimePaths::IniPath().string();
    if (ini.SaveFile(path.c_str()) < 0) {
        RuntimePaths::Log("[Config]", "Failed to save the INI.", true);
        return false;
    }
    {
        std::lock_guard lock(g_mutex);
        g_dirty = false;
    }
    RuntimePaths::Log("[Config]", "Saved the single configuration file.", true);
    return true;
}

Config Get() {
    std::lock_guard lock(g_mutex);
    return g_config;
}

void Apply(const Config& value) {
    Config sanitized = value;
    ConfigPolicy::Sanitize(sanitized);
    std::lock_guard lock(g_mutex);
    g_config = sanitized;
    g_dirty = true;
    ++g_generation;
}

bool Dirty() {
    std::lock_guard lock(g_mutex);
    return g_dirty;
}

std::uint64_t Generation() {
    std::lock_guard lock(g_mutex);
    return g_generation;
}

} // namespace Configuration
