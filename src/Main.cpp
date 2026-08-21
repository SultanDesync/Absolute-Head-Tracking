#include "PCH.h"

#include "CameraHook.h"
#include "Config.h"
#include "ControlPanelDiscovery.h"
#include "ControlPanelSubscriber.h"
#include "HeadTracking.h"
#include "HotasCameraCompatibility.h"
#include "InputBusClient.h"
#include "Plugin.h"
#include "RuntimePaths.h"
#include "SFSEInterface.h"
#include "SuiteState.h"
#include "UIHook.h"
#include "Workbench.h"
#include "WorkbenchHost.h"

namespace {
std::atomic<bool> g_uiOwnershipSelected{};
std::atomic<bool> g_runtimeInitialized{};

bool AbsoluteHOTASPresent() {
    std::error_code error;
    const bool moduleLoaded = GetModuleHandleW(L"AbsoluteHOTAS.dll") != nullptr;
    const bool dllPresent =
        std::filesystem::exists(RuntimePaths::PluginDirectory() / "AbsoluteHOTAS.dll", error);
    return ConfigPolicy::ShouldBlockForAbsoluteHOTAS(moduleLoaded, dllPresent);
}

void SelectFallbackUiOwnership() {
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

void InitializeRuntime() {
    if (g_runtimeInitialized.exchange(true, std::memory_order_acq_rel)) return;

    bool cameraReady = false;
    if (AbsoluteHOTASPresent()) {
        const auto result = HotasCameraCompatibility::Connect();
        if (result != HotasCameraCompatibility::Result::Connected) {
            SuiteState::SetInactiveByHOTAS(true);
            RuntimePaths::Log(
                "[Compatibility]",
                "AbsoluteHOTAS is present but its camera-ownership ABI is unavailable. "
                "Absolute Head Tracking remains fail-closed and installs no gameplay hooks.",
                true);
            return;
        }
        const auto busResult = InputBusClient::Connect();
        if (busResult == InputBusClient::ConnectResult::Connected) {
            CameraHook::SetExternalCockpitSignalProvider(
                &InputBusClient::PilotSignalAgeMilliseconds);
            RuntimePaths::Log(
                "[Compatibility]",
                "Absolute Input Bus selected as the authoritative pilot-context source; "
                "the local Head Tracking flight observer will not be installed.",
                true);
        } else {
            // Older compatible HOTAS builds still expose the same observer through
            // the narrow camera-ownership ABI. This remains external and never
            // re-enables Head Tracking's local observer.
            CameraHook::SetExternalCockpitSignalProvider(
                &HotasCameraCompatibility::CockpitSignalAgeMilliseconds);
            RuntimePaths::Log(
                "[InputBus]",
                "AbsoluteHOTAS camera arbitration is available, but Input Bus ABI 1 is not. "
                "Using its legacy external cockpit signal; controller bindings remain disabled.",
                true);
        }
        SuiteState::SetInactiveByHOTAS(false);
        cameraReady = CameraHook::Install();
    } else {
        SuiteState::SetInactiveByHOTAS(false);
        CameraHook::SetExternalCockpitSignalProvider(nullptr);
        cameraReady = CameraHook::Install();
    }

    if (!cameraReady) {
        RuntimePaths::Log(
            "[Main]",
            "Native camera seams did not pass their exact runtime gates. Tracking is "
            "fail-closed.",
            true);
        return;
    }
    HeadTracking::Start();
    RuntimePaths::Log("[Main]", HotasCameraCompatibility::Connected() ?
        "Head tracking runtime started as the suite camera owner." :
        "Head tracking runtime started with standalone camera ownership.", true);
}

void SelectUiOwnership(bool finalAttempt) {
    if (g_uiOwnershipSelected.load(std::memory_order_acquire)) {
        return;
    }

    const auto panelResult = ControlPanelSubscriber::RegisterDiscoveredHost();
    using ControlPanelSubscriber::Discovery::RegistrationDecision;
    const auto decision = ControlPanelSubscriber::Discovery::DecideRegistration(
        panelResult, finalAttempt);
    if (decision == RegistrationDecision::UseControlPanel) {
        g_uiOwnershipSelected.store(true, std::memory_order_release);
        RuntimePaths::Log("[Main]",
            "Absolute Control Panel selected as configuration renderer.", true);
        return;
    }
    if (decision == RegistrationDecision::RetryAtPostPostDataLoad) {
        RuntimePaths::Log("[Main]",
            "Absolute Control Panel is not ready; registration will retry at "
            "post-post-data-load.", true);
        return;
    }

    if (panelResult == AbsoluteControlPanelApi::Result::NotFound) {
        RuntimePaths::Log("[Main]",
            "Absolute Control Panel is not installed; selecting a fallback frontend.", true);
    } else {
        RuntimePaths::Log("[Main]",
            std::format("Absolute Control Panel registration was unavailable (result {}); "
                        "selecting a fallback frontend.",
                static_cast<std::uint32_t>(panelResult)), true);
    }
    SelectFallbackUiOwnership();
}

void OnSfseMessage(SFSE::MessagingInterface::Message* message) {
    if (!message) return;
    if (message->type == SFSE::MessagingInterface::kPostPostLoad) {
        InitializeRuntime();
    }
    if (message->type == SFSE::MessagingInterface::kPostDataLoad ||
        message->type == SFSE::MessagingInterface::kPostPostDataLoad) {
        SelectUiOwnership(message->type == SFSE::MessagingInterface::kPostPostDataLoad);
    }
}
} // namespace

SFSE_PLUGIN_LOAD(const SFSE::LoadInterface* loadInterface) {
    RuntimePaths::InitializeLogging();
    RuntimePaths::Log("[Main]",
                      std::format("{} {} startup.", Plugin::FriendlyName, Plugin::VersionString),
                      true);

    Configuration::Load();
    SuiteState::SetInactiveByHOTAS(false);

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
            "Could not subscribe to SFSE lifecycle messaging; gameplay and local renderer "
            "hooks are suppressed to avoid load-order-dependent ownership.",
            true);
        return true;
    }

    if (GetModuleHandleW(L"AbsoluteZero.dll")) {
        RuntimePaths::Log("[Compatibility]",
                          "AbsoluteZero detected. Its flight writer hook and this camera hook are "
                          "independent.");
    }
    RuntimePaths::Log(
        "[Main]",
        "Plugin load complete; camera arbitration deferred to post-post-load and renderer "
        "selection deferred to data load.");
    return true;
}
