#include "PCH.h"

#include "HeadTracking.h"
#include "CameraHook.h"
#include "Config.h"
#include "HeadMath.h"
#include "RuntimePaths.h"
#include "UIHook.h"
#include "Workbench.h"

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

bool KeyDown(int key) {
    return key > 0 && (GetAsyncKeyState(key) & 0x8000) != 0;
}

bool HotkeyDown(const HotkeySettings& hotkey) {
    if (!KeyDown(hotkey.virtualKey)) return false;
    return KeyDown(VK_CONTROL) == hotkey.control &&
           KeyDown(VK_MENU) == hotkey.alt &&
           KeyDown(VK_SHIFT) == hotkey.shift;
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
    bool haveCenter = false;
    bool previousWorkbench = false;
    bool previousRecenter = false;
    bool previousToggle = false;
    auto previousTime = std::chrono::steady_clock::now();

    while (g_running.load(std::memory_order_acquire)) {
        const Config config = Configuration::Get();
        const auto now = std::chrono::steady_clock::now();
        const float dt = std::chrono::duration<float>(now - previousTime).count();
        previousTime = now;

        const bool workbenchDown = HotkeyDown(config.workbench);
        const bool recenterDown = HotkeyDown(config.recenter);
        const bool toggleDown = HotkeyDown(config.toggle);
        if (!Workbench::IsCapturingHotkey()) {
            if (workbenchDown && !previousWorkbench && config.enableWorkbench &&
                UIHook::IsAvailable()) UIHook::Toggle();
            if (recenterDown && !previousRecenter) g_recenterRequested.store(true);
            if (toggleDown && !previousToggle) HeadTracking::ToggleRuntime();
        }
        previousWorkbench = workbenchDown;
        previousRecenter = recenterDown;
        previousToggle = toggleDown;

        OpenTrack::Sample sample{};
        const bool haveSample = OpenTrack::Read(sample, config.staleMilliseconds);
        const bool recenter = g_recenterRequested.exchange(false, std::memory_order_acq_rel);
        if (haveSample && (!haveCenter || recenter)) {
            center = sample.radians;
            filtered = {};
            haveCenter = true;
            g_centered.store(true, std::memory_order_release);
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

        const bool active = config.enabled &&
            g_runtimeEnabled.load(std::memory_order_acquire) && haveSample && haveCenter;
        if (active) {
            const std::array<AxisSettings, 3> axes{ config.yaw, config.pitch, config.roll };
            for (std::size_t index = 0; index < axes.size(); ++index) {
                if (!axes[index].enabled) {
                    filtered[index] = 0.0F;
                    continue;
                }
                const float direction = axes[index].inverted ? -1.0F : 1.0F;
                const float target = HeadMath::ShapeAngle(
                    trackerDegrees[index] * axes[index].sensitivity * direction,
                    config.deadzoneDegrees, axes[index].maximumDegrees);
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
    OpenTrack::Shutdown();
}
} // namespace

namespace HeadTracking {

bool Start() {
    bool expected = false;
    if (!g_running.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
        return true;
    g_thread = std::thread(PollLoop);
    RuntimePaths::Log("[HeadTracking]", "OpenTrack polling service started.");
    return true;
}

void Stop() {
    g_running.store(false, std::memory_order_release);
    if (g_thread.joinable()) g_thread.join();
}

void RequestRecenter() {
    g_recenterRequested.store(true, std::memory_order_release);
}

void ToggleRuntime() {
    const bool enabled = !g_runtimeEnabled.load(std::memory_order_relaxed);
    SetRuntimeEnabled(enabled);
}

void SetRuntimeEnabled(bool enabled) {
    g_runtimeEnabled.store(enabled, std::memory_order_release);
    if (enabled) g_recenterRequested.store(true, std::memory_order_release);
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
    return state;
}

} // namespace HeadTracking
