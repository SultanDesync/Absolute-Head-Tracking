#include "PCH.h"

#include "CameraHook.h"
#include "Config.h"
#include "HeadTracking.h"
#include "Plugin.h"
#include "RuntimePaths.h"
#include "SFSEInterface.h"
#include "UIHook.h"
#include "Workbench.h"

namespace {
bool AbsoluteHOTASPresent() {
    std::error_code error;
    const bool moduleLoaded = GetModuleHandleW(L"AbsoluteHOTAS.dll") != nullptr;
    const bool dllPresent = std::filesystem::exists(
        RuntimePaths::PluginDirectory() / "AbsoluteHOTAS.dll", error);
    return ConfigPolicy::ShouldBlockForAbsoluteHOTAS(moduleLoaded, dllPresent);
}
} // namespace

SFSE_PLUGIN_LOAD(const SFSE::LoadInterface*) {
    RuntimePaths::InitializeLogging();
    RuntimePaths::Log("[Main]", std::format("{} {} startup.",
        Plugin::FriendlyName, Plugin::VersionString), true);

    // Check the installed file as well as the loaded module. SFSE plugin load
    // order must never decide which of two camera-hook owners wins.
    if (AbsoluteHOTASPresent()) {
        RuntimePaths::Log("[Compatibility]",
            "AbsoluteHOTAS.dll detected. Absolute Head Tracking will not install hooks; "
            "use AbsoluteHOTAS's built-in camera look instead.", true);
        return true;
    }

    Configuration::Load();
    const bool cameraReady = CameraHook::Install();
    if (!cameraReady) {
        RuntimePaths::Log("[Main]",
            "Native camera seams did not pass their exact runtime gates. Tracking is fail-closed.", true);
    }

    HeadTracking::Start();
    const Config config = Configuration::Get();
    if (config.enableWorkbench && UIHook::Install()) {
        Workbench::Initialize();
        RuntimePaths::Log("[Main]", "Dear ImGui workbench armed; press Ctrl+Alt+H.");
    } else if (config.enableWorkbench) {
        RuntimePaths::Log("[Main]",
            "Workbench graphics hook unavailable; the INI remains fully usable.", true);
    }

    if (GetModuleHandleW(L"AbsoluteZero.dll")) {
        RuntimePaths::Log("[Compatibility]",
            "AbsoluteZero detected. Its flight writer hook and this camera hook are independent.");
    }
    RuntimePaths::Log("[Main]", "Plugin load complete.");
    return true;
}
