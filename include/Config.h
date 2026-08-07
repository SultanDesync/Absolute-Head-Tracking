#pragma once

#include <cstdint>

struct AxisSettings {
    bool enabled = true;
    bool inverted = false;
    float sensitivity = 1.0F;
    float maximumDegrees = 85.0F;
};

struct HotkeySettings {
    int virtualKey = 0;
    bool control = true;
    bool alt = true;
    bool shift = false;
};

struct Config {
    bool enabled = true;
    bool enableLog = false;
    bool enableWorkbench = true;
    int pollRateHz = 120;
    int staleMilliseconds = 500;
    float deadzoneDegrees = 0.0F;
    float smoothing = 0.15F;
    AxisSettings yaw{ true, false, 1.0F, 85.0F };
    AxisSettings pitch{ true, false, 1.0F, 60.0F };
    AxisSettings roll{ true, false, 1.0F, 45.0F };
    HotkeySettings workbench{ 0x48, true, true, false };
    HotkeySettings recenter{ 0x43, true, true, false };
    HotkeySettings toggle{ 0x54, true, true, false };
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
