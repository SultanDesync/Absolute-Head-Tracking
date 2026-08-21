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
        loaded.yaw.maximumDegrees = static_cast<float>(ini.GetDoubleValue(
            "Tracking", "fMaxYawDegrees", loaded.yaw.maximumDegrees));
        loaded.pitch.maximumDegrees = static_cast<float>(ini.GetDoubleValue(
            "Tracking", "fMaxPitchDegrees", loaded.pitch.maximumDegrees));
        loaded.roll.maximumDegrees = static_cast<float>(ini.GetDoubleValue(
            "Tracking", "fMaxRollDegrees", loaded.roll.maximumDegrees));
        loaded.deadzoneDegrees = static_cast<float>(ini.GetDoubleValue(
            "Tracking", "fDeadzoneDegrees", loaded.deadzoneDegrees));
        loaded.smoothing = static_cast<float>(ini.GetDoubleValue(
            "Tracking", "fSmoothing", loaded.smoothing));
        loaded.staleMilliseconds = ReadInteger(
            ini, "Tracking", "iStaleMilliseconds", loaded.staleMilliseconds);
        loaded.workbench = ReadHotkey(ini, "Workbench", loaded.workbench);
        loaded.recenter = ReadHotkey(ini, "Recenter", loaded.recenter);
        loaded.toggle = ReadHotkey(ini, "Toggle", loaded.toggle);
        loaded.recenterControllerBinding = ini.GetValue(
            "Hotkeys", "sRecenterControllerBinding", "");
        loaded.toggleControllerBinding = ini.GetValue(
            "Hotkeys", "sToggleControllerBinding", "");
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
    ini.SetDoubleValue("Tracking", "fMaxYawDegrees", config.yaw.maximumDegrees, nullptr, true);
    ini.SetDoubleValue("Tracking", "fMaxPitchDegrees", config.pitch.maximumDegrees, nullptr, true);
    ini.SetDoubleValue("Tracking", "fMaxRollDegrees", config.roll.maximumDegrees, nullptr, true);
    ini.SetDoubleValue("Tracking", "fDeadzoneDegrees", config.deadzoneDegrees, nullptr, true);
    ini.SetDoubleValue("Tracking", "fSmoothing", config.smoothing, nullptr, true);
    ini.SetLongValue("Tracking", "iStaleMilliseconds", config.staleMilliseconds, nullptr, true);
    WriteHotkey(ini, "Workbench", config.workbench);
    WriteHotkey(ini, "Recenter", config.recenter);
    WriteHotkey(ini, "Toggle", config.toggle);
    ini.SetValue("Hotkeys", "sRecenterControllerBinding",
        config.recenterControllerBinding.c_str(), nullptr, true);
    ini.SetValue("Hotkeys", "sToggleControllerBinding",
        config.toggleControllerBinding.c_str(), nullptr, true);

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
