#include "PCH.h"

#include "CameraHook.h"
#include "Config.h"
#include "HeadTracking.h"
#include "Plugin.h"
#include "RuntimePaths.h"
#include "SFSEInterface.h"
#include "SuiteState.h"
#include "UIHook.h"
#include "Workbench.h"
#include "WorkbenchHost.h"

namespace {
std::atomic<bool> g_uiOwnershipSelected{};

bool AbsoluteHOTASPresent() {
    std::error_code error;
    const bool moduleLoaded = GetModuleHandleW(L"AbsoluteHOTAS.dll") != nullptr;
    const bool dllPresent =
        std::filesystem::exists(RuntimePaths::PluginDirectory() / "AbsoluteHOTAS.dll", error);
    return ConfigPolicy::ShouldBlockForAbsoluteHOTAS(moduleLoaded, dllPresent);
}

void SelectUiOwnership() {
    if (g_uiOwnershipSelected.exchange(true, std::memory_order_acq_rel)) {
        return;
    }
    const Config config = Configuration::Get();
    const auto selection = WorkbenchHost::Select();
    if (!WorkbenchHost::ShouldInstallStandaloneRenderer(config.enableWorkbench)) {
        return;
    }
    if (UIHook::Install()) {
        Workbench::Initialize();
        RuntimePaths::Log("[Main]",
                          "Standalone Dear ImGui workbench armed; press Ctrl+Alt+H.");
    } else {
        RuntimePaths::Log("[Main]",
                          "Standalone workbench graphics hook unavailable; the INI and suite API "
                          "remain usable.",
                          true);
    }
}

void OnSfseMessage(SFSE::MessagingInterface::Message* message) {
    if (message && message->type == SFSE::MessagingInterface::kPostPostLoad) {
        SelectUiOwnership();
    }
}
} // namespace

SFSE_PLUGIN_LOAD(const SFSE::LoadInterface* loadInterface) {
    RuntimePaths::InitializeLogging();
    RuntimePaths::Log("[Main]",
                      std::format("{} {} startup.", Plugin::FriendlyName, Plugin::VersionString),
                      true);

    Configuration::Load();
    const bool inactiveByHOTAS = AbsoluteHOTASPresent();
    SuiteState::SetInactiveByHOTAS(inactiveByHOTAS);
    if (inactiveByHOTAS) {
        RuntimePaths::Log("[Compatibility]",
                          "AbsoluteHOTAS.dll detected. Absolute Head Tracking remains queryable "
                          "for diagnostics but installs no gameplay or renderer hooks.",
                          true);
    } else {
        const bool cameraReady = CameraHook::Install();
        if (!cameraReady) {
            RuntimePaths::Log(
                "[Main]",
                "Native camera seams did not pass their exact runtime gates. Tracking is "
                "fail-closed.",
                true);
        }
        HeadTracking::Start();
    }

    if (!loadInterface) {
        RuntimePaths::Log("[Main]",
                          "SFSE load interface is null; local renderer ownership cannot be "
                          "selected safely.",
                          true);
        return true;
    }
    const auto* messaging = loadInterface->GetMessagingInterface();
    if (!messaging || messaging->Version() < 1 ||
        !messaging->RegisterListener(loadInterface->GetPluginHandle(), &OnSfseMessage)) {
        RuntimePaths::Log(
            "[Main]",
            "Could not subscribe to SFSE post-post-load messaging; local renderer suppressed "
            "to avoid load-order-dependent graphics ownership.",
            true);
        return true;
    }

    if (GetModuleHandleW(L"AbsoluteZero.dll")) {
        RuntimePaths::Log("[Compatibility]",
                          "AbsoluteZero detected. Its flight writer hook and this camera hook are "
                          "independent.");
    }
    RuntimePaths::Log("[Main]", "Plugin load complete; renderer selection deferred.");
    return true;
}
