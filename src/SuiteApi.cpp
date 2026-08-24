#include "PCH.h"

#include "AbsoluteHeadTrackingAPI.h"

#include "CameraHook.h"
#include "Config.h"
#include "HeadTracking.h"
#include "OpenTrack.h"
#include "Plugin.h"
#include "SuiteState.h"

namespace {
using AbsoluteHeadTrackingApi::ApiV1;
using AbsoluteHeadTrackingApi::AxisConfigurationV1;
using AbsoluteHeadTrackingApi::ConfigurationV1;
using AbsoluteHeadTrackingApi::ConnectionState;
using AbsoluteHeadTrackingApi::HotkeyV1;
using AbsoluteHeadTrackingApi::PoseV1;
using AbsoluteHeadTrackingApi::Result;
using AbsoluteHeadTrackingApi::StatusV1;

HotkeyV1 ToApi(const HotkeySettings& value) {
    return {static_cast<std::uint32_t>(value.virtualKey), static_cast<std::uint8_t>(value.control),
            static_cast<std::uint8_t>(value.alt), static_cast<std::uint8_t>(value.shift), 0};
}

HotkeySettings FromApi(const HotkeyV1& value) {
    return {static_cast<int>(value.virtualKey), value.control != 0, value.alt != 0,
            value.shift != 0};
}

AxisConfigurationV1 ToApi(const AxisSettings& value) {
    AxisConfigurationV1 output;
    output.enabled = static_cast<std::uint8_t>(value.enabled);
    output.inverted = static_cast<std::uint8_t>(value.inverted);
    output.sensitivity = value.sensitivity;
    // ABI v1 exposes only a symmetric magnitude. Project an asymmetric range
    // conservatively for older suite clients without changing the record size.
    output.maximumDegrees = std::max(
        std::abs(value.minimumDegrees), std::abs(value.maximumDegrees));
    return output;
}

AxisSettings FromApi(const AxisConfigurationV1& value) {
    const float maximum = std::abs(value.maximumDegrees);
    return {value.enabled != 0, value.inverted != 0, value.sensitivity,
            -maximum, 0.0F, maximum};
}

ConnectionState ToApi(OpenTrack::ConnectionState value) {
    switch (value) {
    case OpenTrack::ConnectionState::Waiting:
        return ConnectionState::Waiting;
    case OpenTrack::ConnectionState::Connected:
        return ConnectionState::Connected;
    case OpenTrack::ConnectionState::Stale:
        return ConnectionState::Stale;
    }
    return ConnectionState::Waiting;
}

Result __cdecl GetStatus(StatusV1* output) noexcept {
    if (!output || output->structSize < sizeof(*output)) {
        return Result::InvalidArgument;
    }
    try {
        *output = {};
        const auto hooks = CameraHook::GetStatus();
        const auto live = HeadTracking::GetLiveState();
        output->cameraHookInstalled = static_cast<std::uint8_t>(hooks.cameraHookInstalled);
        output->cockpitObserverInstalled =
            static_cast<std::uint8_t>(hooks.cockpitObserverInstalled);
        output->poseActive = static_cast<std::uint8_t>(hooks.poseActive);
        output->runtimeEnabled = static_cast<std::uint8_t>(live.runtimeEnabled);
        output->configurationDirty = static_cast<std::uint8_t>(Configuration::Dirty());
        output->inactiveByHOTAS = static_cast<std::uint8_t>(SuiteState::InactiveByHOTAS());
        output->hosted = static_cast<std::uint8_t>(SuiteState::WorkbenchOpen());
        output->inputCaptureActive =
            static_cast<std::uint8_t>(SuiteState::InputCaptureActive());
        output->connection = ToApi(live.connection);
        output->cockpitSignalAgeMilliseconds = hooks.cockpitSignalAgeMilliseconds;
        output->trackerSampleAgeMilliseconds = OpenTrack::SampleAgeMilliseconds();
        output->configurationGeneration = Configuration::Generation();

        if (SuiteState::InactiveByHOTAS()) {
            return Result::InactiveByHOTAS;
        }
        if (!hooks.cameraHookInstalled || !hooks.cockpitObserverInstalled) {
            return Result::UnsupportedRuntime;
        }
        return Result::Ok;
    } catch (...) {
        return Result::Rejected;
    }
}

Result __cdecl GetPose(PoseV1* output) noexcept {
    if (!output || output->structSize < sizeof(*output)) {
        return Result::InvalidArgument;
    }
    try {
        *output = {};
        const auto live = HeadTracking::GetLiveState();
        output->centered = static_cast<std::uint8_t>(live.centered);
        for (std::size_t index = 0; index < AbsoluteHeadTrackingApi::kAxisCount; ++index) {
            output->trackerDegrees[index] = live.trackerDegrees[index];
            output->outputDegrees[index] = live.outputDegrees[index];
        }
        return live.connection == OpenTrack::ConnectionState::Connected
                   ? Result::Ok
                   : Result::TrackerUnavailable;
    } catch (...) {
        return Result::Rejected;
    }
}

Result __cdecl GetConfiguration(ConfigurationV1* output) noexcept {
    if (!output || output->structSize < sizeof(*output)) {
        return Result::InvalidArgument;
    }
    try {
        const Config config = Configuration::Get();
        *output = {};
        output->enabled = static_cast<std::uint8_t>(config.enabled);
        output->enableLog = static_cast<std::uint8_t>(config.enableLog);
        output->enableStandaloneWorkbench =
            static_cast<std::uint8_t>(config.enableWorkbench);
        output->pollRateHz = static_cast<std::uint32_t>(config.pollRateHz);
        output->staleMilliseconds =
            static_cast<std::uint32_t>(config.staleMilliseconds);
        output->deadzoneDegrees = config.deadzoneDegrees;
        output->smoothing = config.smoothing;
        output->axes[0] = ToApi(config.yaw);
        output->axes[1] = ToApi(config.pitch);
        output->axes[2] = ToApi(config.roll);
        output->legacyWorkbench = ToApi(config.workbench);
        output->recenter = ToApi(config.recenter);
        output->toggle = ToApi(config.toggle);
        return Result::Ok;
    } catch (...) {
        return Result::Rejected;
    }
}

Result __cdecl ApplyConfiguration(const ConfigurationV1* input) noexcept {
    if (!input || input->structSize < sizeof(*input)) {
        return Result::InvalidArgument;
    }
    try {
        // ABI v1 predates DirectInput bus bindings. Preserve those provider-owned
        // fields when an older suite client applies the v1 configuration record.
        Config config = Configuration::Get();
        config.enabled = input->enabled != 0;
        config.enableLog = input->enableLog != 0;
        config.enableWorkbench = input->enableStandaloneWorkbench != 0;
        config.pollRateHz = static_cast<int>(input->pollRateHz);
        config.staleMilliseconds = static_cast<int>(input->staleMilliseconds);
        config.deadzoneDegrees = input->deadzoneDegrees;
        config.smoothing = input->smoothing;
        config.yaw = FromApi(input->axes[0]);
        config.pitch = FromApi(input->axes[1]);
        config.roll = FromApi(input->axes[2]);
        config.workbench = FromApi(input->legacyWorkbench);
        config.recenter = FromApi(input->recenter);
        config.toggle = FromApi(input->toggle);
        ConfigPolicy::Sanitize(config);
        Configuration::Apply(config);
        return Result::Ok;
    } catch (...) {
        return Result::Rejected;
    }
}

Result __cdecl SaveConfiguration() noexcept {
    try {
        return Configuration::Save() ? Result::Ok : Result::WriteFailure;
    } catch (...) {
        return Result::WriteFailure;
    }
}

Result __cdecl ReloadConfiguration() noexcept {
    try {
        return Configuration::Load() ? Result::Ok : Result::Rejected;
    } catch (...) {
        return Result::Rejected;
    }
}

Result __cdecl RequestRecenter() noexcept {
    if (SuiteState::InactiveByHOTAS()) {
        return Result::InactiveByHOTAS;
    }
    try {
        HeadTracking::RequestRecenter();
        return Result::Ok;
    } catch (...) {
        return Result::Rejected;
    }
}

Result __cdecl SetRuntimeEnabled(std::uint8_t enabled) noexcept {
    if (SuiteState::InactiveByHOTAS()) {
        return Result::InactiveByHOTAS;
    }
    try {
        HeadTracking::SetRuntimeEnabled(enabled != 0);
        return Result::Ok;
    } catch (...) {
        return Result::Rejected;
    }
}

Result __cdecl SetWorkbenchSessionState(std::uint8_t open,
                                        std::uint8_t inputCaptureActive) noexcept {
    SuiteState::SetWorkbenchSession(open != 0, inputCaptureActive != 0);
    return Result::Ok;
}

const ApiV1 kApi{
    .moduleId = "absolute.head_tracking",
    .displayName = "Absolute Head Tracking",
    .version = Plugin::VersionString.data(),
    .getStatus = &GetStatus,
    .getPose = &GetPose,
    .getConfiguration = &GetConfiguration,
    .applyConfiguration = &ApplyConfiguration,
    .saveConfiguration = &SaveConfiguration,
    .reloadConfiguration = &ReloadConfiguration,
    .requestRecenter = &RequestRecenter,
    .setRuntimeEnabled = &SetRuntimeEnabled,
    .setWorkbenchSessionState = &SetWorkbenchSessionState,
};
} // namespace

extern "C" __declspec(dllexport) const AbsoluteHeadTrackingApi::ApiV1*
AbsoluteHeadTracking_QueryApi(std::uint32_t requestedAbiVersion) noexcept {
    return requestedAbiVersion == AbsoluteHeadTrackingApi::kAbiVersion ? &kApi : nullptr;
}
