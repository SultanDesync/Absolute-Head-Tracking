#include "PCH.h"

#include "AimRestriction.h"
#include "HeadTracking.h"
#include "CameraHook.h"
#include "Config.h"
#include "HeadMath.h"
#include "InputBusClient.h"
#include "ManualLookPolicy.h"
#include "ManualMouseInput.h"
#include "RuntimePaths.h"
#include "WorkbenchHost.h"

namespace {
std::atomic<bool> g_running{ false };
std::thread g_thread;
std::atomic<bool> g_runtimeEnabled{ true };
std::atomic<bool> g_recenterRequested{ false };
std::array<std::atomic<std::uint32_t>, 3> g_trackerBits{
    std::bit_cast<std::uint32_t>(0.0F), std::bit_cast<std::uint32_t>(0.0F),
    std::bit_cast<std::uint32_t>(0.0F)
};
std::array<std::atomic<std::uint32_t>, 3> g_outputBits{
    std::bit_cast<std::uint32_t>(0.0F), std::bit_cast<std::uint32_t>(0.0F),
    std::bit_cast<std::uint32_t>(0.0F)
};
std::atomic<bool> g_centered{ false };
std::atomic<bool> g_poseAvailable{ false };

bool KeyDown(int key) {
    return key > 0 && (GetAsyncKeyState(key) & 0x8000) != 0;
}

bool HotkeyDown(const HotkeySettings& hotkey) {
    if (!KeyDown(hotkey.virtualKey)) return false;
    return KeyDown(VK_CONTROL) == hotkey.control &&
           KeyDown(VK_MENU) == hotkey.alt &&
           KeyDown(VK_SHIFT) == hotkey.shift;
}

bool MouseModifierDown(MouseLookModifier modifier) {
    switch (modifier) {
    case MouseLookModifier::Alt: return KeyDown(VK_MENU);
    case MouseLookModifier::Control: return KeyDown(VK_CONTROL);
    case MouseLookModifier::Shift: return KeyDown(VK_SHIFT);
    case MouseLookModifier::Disabled: return false;
    }
    return false;
}

bool ManualKeyboardConfigured(const ManualLookSettings& manual) noexcept {
    return manual.lookLeft.virtualKey > 0 || manual.lookRight.virtualKey > 0 ||
        manual.lookDown.virtualKey > 0 || manual.lookUp.virtualKey > 0 ||
        manual.bankLeft.virtualKey > 0 || manual.bankRight.virtualKey > 0;
}

void PublishAngles(const std::array<float, 3>& tracker,
                   const std::array<float, 3>& output) {
    for (std::size_t index = 0; index < tracker.size(); ++index) {
        g_trackerBits[index].store(std::bit_cast<std::uint32_t>(tracker[index]),
            std::memory_order_relaxed);
        g_outputBits[index].store(std::bit_cast<std::uint32_t>(output[index]),
            std::memory_order_relaxed);
    }
}

void PollLoop() {
    constexpr float degreesPerRadian = 57.295779513082320876F;
    std::array<float, 3> center{};
    std::array<float, 3> filtered{};
    std::array<float, 3> manualTrim{};
    bool haveCenter = false;
    bool previousWorkbench = false;
    bool previousRecenter = false;
    bool previousToggle = false;
    auto previousTime = std::chrono::steady_clock::now();

    while (g_running.load(std::memory_order_acquire)) {
        const Config config = Configuration::Get();
        const auto now = std::chrono::steady_clock::now();
        const float dt = std::clamp(
            std::chrono::duration<float>(now - previousTime).count(),
            0.0F, 0.1F);
        previousTime = now;

        const bool workbenchDown = HotkeyDown(config.workbench);
        const bool recenterDown = HotkeyDown(config.recenter);
        const bool toggleDown = HotkeyDown(config.toggle);
        const bool inputCaptureActive = WorkbenchHost::IsInputCaptureActive();
        const auto cameraStatus = CameraHook::GetStatus();
        const bool pilotActive = cameraStatus.cockpitSignalAgeMilliseconds >= 0 &&
            cameraStatus.cockpitSignalAgeMilliseconds <= 400;
        const bool manualInputSuppressed = inputCaptureActive ||
            WorkbenchHost::IsMenuOpen() || !pilotActive;
        if (!inputCaptureActive) {
            if (workbenchDown && !previousWorkbench)
                WorkbenchHost::HandleLegacyMenuShortcut();
            if (recenterDown && !previousRecenter) g_recenterRequested.store(true);
            if (toggleDown && !previousToggle) HeadTracking::ToggleRuntime();
        }
        previousWorkbench = workbenchDown;
        previousRecenter = recenterDown;
        previousToggle = toggleDown;

        const auto controllerActions = InputBusClient::PollActions(
            config.toggleControllerBinding,
            config.recenterControllerBinding, inputCaptureActive);
        if (controllerActions.recenterPresses != 0) {
            g_recenterRequested.store(true, std::memory_order_release);
        }
        // Preserve the semantics of every captured toggle edge without emitting
        // redundant state transitions when an even number accumulated.
        if ((controllerActions.togglePresses & 1U) != 0) {
            HeadTracking::ToggleRuntime();
        }

        OpenTrack::Sample sample{};
        const bool haveSample = OpenTrack::Read(sample, config.staleMilliseconds);
        const bool recenter = g_recenterRequested.exchange(false, std::memory_order_acq_rel);
        if (recenter) {
            manualTrim = {};
            filtered = {};
            AimRestriction::ResetSpatialState();
            if (!haveSample) {
                haveCenter = false;
                g_centered.store(false, std::memory_order_release);
                RuntimePaths::Log("[HeadTracking]",
                    "Manual look reset; OpenTrack center will be captured from its next fresh frame.");
            }
        }
        if (haveSample && (!haveCenter || recenter)) {
            center = sample.radians;
            filtered = {};
            haveCenter = true;
            g_centered.store(true, std::memory_order_release);
            AimRestriction::ResetSpatialState();
            RuntimePaths::Log("[HeadTracking]", recenter
                ? "OpenTrack center captured by request." : "Initial OpenTrack center captured.");
        }

        std::array<float, 3> trackerDegrees{};
        std::array<float, 3> outputDegrees{};
        if (haveSample && haveCenter) {
            for (std::size_t index = 0; index < trackerDegrees.size(); ++index) {
                trackerDegrees[index] = (sample.radians[index] - center[index]) * degreesPerRadian;
            }
        }


        const std::array<std::string_view, 3> controllerAxes{
            config.manualLook.yawControllerAxisBinding,
            config.manualLook.pitchControllerAxisBinding,
            config.manualLook.rollControllerAxisBinding,
        };
        const std::array<std::string_view, 3> controllerNegative{
            config.manualLook.lookRightControllerBinding,
            config.manualLook.lookDownControllerBinding,
            config.manualLook.bankRightControllerBinding,
        };
        const std::array<std::string_view, 3> controllerPositive{
            config.manualLook.lookLeftControllerBinding,
            config.manualLook.lookUpControllerBinding,
            config.manualLook.bankLeftControllerBinding,
        };
        const auto controllerLook = InputBusClient::PollLook(
            controllerAxes, controllerNegative, controllerPositive,
            manualInputSuppressed);
        const bool manualConfigured = ManualKeyboardConfigured(config.manualLook) ||
            controllerLook.configured ||
            (config.manualLook.mouseModifier != MouseLookModifier::Disabled &&
                ManualMouseInput::Available());

        const bool runtimeEnabled =
            g_runtimeEnabled.load(std::memory_order_acquire);
        const bool mouseLooking = !manualInputSuppressed && config.enabled &&
            runtimeEnabled &&
            (config.yaw.enabled || config.pitch.enabled) &&
            MouseModifierDown(config.manualLook.mouseModifier);
        ManualMouseInput::SetExclusive(mouseLooking);
        const auto mouse = ManualMouseInput::Consume();
        if (!manualInputSuppressed) {
            const std::array<float, 3> keyboardDirection{
                static_cast<float>(HotkeyDown(config.manualLook.lookLeft)) -
                    static_cast<float>(HotkeyDown(config.manualLook.lookRight)),
                static_cast<float>(HotkeyDown(config.manualLook.lookUp)) -
                    static_cast<float>(HotkeyDown(config.manualLook.lookDown)),
                static_cast<float>(HotkeyDown(config.manualLook.bankLeft)) -
                    static_cast<float>(HotkeyDown(config.manualLook.bankRight)),
            };
            const std::array<AxisSettings, 3> axes{
                config.yaw, config.pitch, config.roll };
            for (std::size_t index = 0; index < manualTrim.size(); ++index) {
                const float direction = std::clamp(
                    keyboardDirection[index] + controllerLook.digital[index],
                    -1.0F, 1.0F);
                manualTrim[index] += direction *
                    config.manualLook.digitalRateDegreesPerSecond * dt;
                manualTrim[index] = std::clamp(manualTrim[index],
                    axes[index].minimumDegrees - axes[index].centerDegrees,
                    axes[index].maximumDegrees - axes[index].centerDegrees);
            }
            if (mouseLooking) {
                const float horizontalDirection =
                    config.manualLook.invertMouseHorizontal ? 1.0F : -1.0F;
                const float verticalDirection =
                    config.manualLook.invertMouseVertical ? 1.0F : -1.0F;
                manualTrim[0] += static_cast<float>(mouse.horizontal) *
                    config.manualLook.mouseDegreesPerCount * horizontalDirection;
                manualTrim[1] += static_cast<float>(mouse.vertical) *
                    config.manualLook.mouseDegreesPerCount * verticalDirection;
                manualTrim[0] = std::clamp(manualTrim[0],
                    config.yaw.minimumDegrees - config.yaw.centerDegrees,
                    config.yaw.maximumDegrees - config.yaw.centerDegrees);
                manualTrim[1] = std::clamp(manualTrim[1],
                    config.pitch.minimumDegrees - config.pitch.centerDegrees,
                    config.pitch.maximumDegrees - config.pitch.centerDegrees);
            }
        }

        const bool active = config.enabled &&
            runtimeEnabled &&
            ((haveSample && haveCenter) || manualConfigured);
        g_poseAvailable.store(active, std::memory_order_release);
        if (active) {
            const std::array<AxisSettings, 3> axes{ config.yaw, config.pitch, config.roll };
            const std::array<bool, 3> invertController{
                config.manualLook.invertControllerYaw,
                config.manualLook.invertControllerPitch,
                config.manualLook.invertControllerRoll,
            };
            // Starfield's composed yaw and roll are positive-left. Manual
            // controls are labeled by physical direction, so conventional
            // positive-right controller axes need a negative base before user
            // inversion.
            constexpr std::array<float, 3> controllerBaseDirection{
                -1.0F, 1.0F, -1.0F };
            for (std::size_t index = 0; index < axes.size(); ++index) {
                if (!axes[index].enabled) {
                    filtered[index] = 0.0F;
                    continue;
                }
                const float direction = axes[index].inverted ? -1.0F : 1.0F;
                const float trackerBase = haveSample && haveCenter
                    ? HeadMath::ShapeAngle(
                        trackerDegrees[index] * axes[index].sensitivity * direction,
                        config.deadzoneDegrees, axes[index].minimumDegrees,
                        axes[index].centerDegrees, axes[index].maximumDegrees)
                    : axes[index].centerDegrees;
                const float controllerDirection = controllerBaseDirection[index] *
                    (invertController[index] ? -1.0F : 1.0F);
                const float analogOffset = ManualLookPolicy::AnalogOffset(
                    controllerLook.analog[index] * controllerDirection,
                    config.manualLook.controllerAxisDeadzone,
                    axes[index].minimumDegrees, axes[index].centerDegrees,
                    axes[index].maximumDegrees);
                const float target = ManualLookPolicy::Composite(
                    trackerBase, manualTrim[index], analogOffset,
                    axes[index].minimumDegrees, axes[index].maximumDegrees);
                filtered[index] = HeadMath::Smooth(
                    filtered[index], target, config.smoothing, dt);
                outputDegrees[index] = filtered[index];
            }
            CameraHook::Publish(HeadMath::FromDegrees(
                outputDegrees[0], outputDegrees[1], outputDegrees[2]), true);
        } else {
            filtered = {};
            CameraHook::Clear();
        }
        PublishAngles(trackerDegrees, outputDegrees);

        const int sleepMilliseconds = std::max(1, 1000 / config.pollRateHz);
        std::this_thread::sleep_for(std::chrono::milliseconds(sleepMilliseconds));
    }

    CameraHook::Clear();
    g_poseAvailable.store(false, std::memory_order_release);
    OpenTrack::Shutdown();
}
} // namespace

namespace HeadTracking {

bool Start() {
    bool expected = false;
    if (!g_running.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
        return true;
    if (!ManualMouseInput::Install()) {
        RuntimePaths::Log("[ManualLook]",
            "Raw mouse-delta observer could not be installed; keyboard and Input Bus look remain available.",
            true);
    }
    g_thread = std::thread(PollLoop);
    RuntimePaths::Log("[HeadTracking]", "OpenTrack polling service started.");
    return true;
}

void Stop() {
    g_running.store(false, std::memory_order_release);
    if (g_thread.joinable()) g_thread.join();
    ManualMouseInput::Shutdown();
}

void RequestRecenter() {
    AimRestriction::ResetSpatialState();
    g_recenterRequested.store(true, std::memory_order_release);
}

void ToggleRuntime() {
    const bool enabled = !g_runtimeEnabled.load(std::memory_order_relaxed);
    SetRuntimeEnabled(enabled);
}

void SetRuntimeEnabled(bool enabled) {
    g_runtimeEnabled.store(enabled, std::memory_order_release);
    if (enabled) {
        AimRestriction::ResetSpatialState();
        g_recenterRequested.store(true, std::memory_order_release);
    }
    else CameraHook::Clear();
    RuntimePaths::Log("[HeadTracking]",
        enabled ? "Runtime camera look enabled." : "Runtime camera look disabled.", true);
}

bool RuntimeEnabled() {
    return g_runtimeEnabled.load(std::memory_order_acquire);
}

LiveState GetLiveState() {
    LiveState state;
    for (std::size_t index = 0; index < state.trackerDegrees.size(); ++index) {
        state.trackerDegrees[index] = std::bit_cast<float>(
            g_trackerBits[index].load(std::memory_order_relaxed));
        state.outputDegrees[index] = std::bit_cast<float>(
            g_outputBits[index].load(std::memory_order_relaxed));
    }
    state.connection = OpenTrack::State();
    state.runtimeEnabled = RuntimeEnabled();
    state.centered = g_centered.load(std::memory_order_acquire);
    state.poseAvailable = g_poseAvailable.load(std::memory_order_acquire);
    return state;
}

} // namespace HeadTracking
