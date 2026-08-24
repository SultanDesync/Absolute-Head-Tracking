#include "PCH.h"

#include "ControlPanelSubscriber.h"

#include "BindingCodec.h"
#include "Config.h"
#include "ControlPanelDiscovery.h"
#include "HeadTracking.h"
#include "InputBusClient.h"
#include "LiveComponentsExperimentalAPI.h"
#include "RuntimePaths.h"

namespace {
using namespace AbsoluteControlPanelApi;

std::atomic<const ApiV1*> g_hostApi{};
std::atomic<const AbsoluteControlPanelExperimental::ExperimentalApiV1*>
    g_liveHostApi{};
std::atomic<std::uint64_t> g_headPoseSequence{};
std::mutex g_editMutex;
Config g_rollback{};
bool g_editing{};

template <std::size_t N>
void Copy(char (&target)[N], std::string_view source) noexcept {
    const auto count = (std::min)(source.size(), N - 1);
    std::memcpy(target, source.data(), count);
}

ValueV1 Boolean(bool value) noexcept {
    ValueV1 result;
    result.kind = ValueKind::Boolean;
    result.booleanValue = value ? 1U : 0U;
    return result;
}
ValueV1 Integer(int value) noexcept {
    ValueV1 result;
    result.kind = ValueKind::Integer;
    result.integerValue = value;
    return result;
}
ValueV1 Float(float value) noexcept {
    ValueV1 result;
    result.kind = ValueKind::Float;
    result.floatValue = value;
    return result;
}
ValueV1 String(std::string_view value) noexcept {
    ValueV1 result;
    result.kind = ValueKind::String;
    Copy(result.stringValue, value);
    return result;
}

ControlDescriptorV1 Control(ControlKind kind, std::uint32_t flags,
    std::string_view id, std::string_view label, std::string_view description,
    double minimum = 0.0, double maximum = 0.0, double step = 0.0) {
    ControlDescriptorV1 control;
    control.kind = kind;
    control.flags = flags;
    Copy(control.controlId, id);
    Copy(control.label, label);
    Copy(control.description, description);
    control.minimumValue = minimum;
    control.maximumValue = maximum;
    control.stepValue = step;
    return control;
}

constexpr std::uint32_t kKeyboardChord = kBindingKeyboard | kBindingModifiers |
    kBindingClearable;

const std::array g_generalControls{
    Control(ControlKind::Toggle, kControlNone, "master-enabled", "Enable camera look",
        "Enable OpenTrack camera look when the cockpit and runtime gates are ready."),
    Control(ControlKind::IntegerSlider, kControlAdvanced, "poll-rate", "Polling rate",
        "OpenTrack polling frequency in hertz.", 30, 500, 10),
    Control(ControlKind::IntegerSlider, kControlAdvanced, "stale-timeout", "Stale timeout",
        "Milliseconds before an old OpenTrack frame stops affecting the camera.", 50, 5000, 50),
    Control(ControlKind::FloatSlider, kControlNone, "smoothing", "Smoothing",
        "Output smoothing from immediate response to strongly filtered motion.", 0, 0.99, 0.01),
    Control(ControlKind::Toggle, kControlRequiresRestart | kControlAdvanced,
        "diagnostic-log", "Diagnostic logging",
        "Write detailed Absolute Head Tracking diagnostics after restart.")
};

const std::array g_axesControls{
    Control(ControlKind::FloatSlider, kControlNone, "deadzone", "Deadzone",
        "Sensitivity-scaled angular movement ignored around the captured center, in degrees.", 0, 20, 0.1),
    Control(ControlKind::Toggle, kControlNone, "yaw-enabled", "Enable yaw", "Apply horizontal head rotation."),
    Control(ControlKind::Toggle, kControlNone, "yaw-inverted", "Invert yaw", "Reverse horizontal head rotation."),
    Control(ControlKind::FloatSlider, kControlNone, "yaw-sensitivity", "Yaw sensitivity", "Scale horizontal tracker rotation.", 0.05, 10, 0.05),
    Control(ControlKind::FloatSlider, kControlNone, "yaw-minimum", "Left limit", "Minimum horizontal camera angle after calibration.", -180, 180, 1),
    Control(ControlKind::FloatSlider, kControlNone, "yaw-center", "Yaw neutral angle", "Camera yaw at the physically recentered tracker pose.", -180, 180, 1),
    Control(ControlKind::FloatSlider, kControlNone, "yaw-maximum", "Right limit", "Maximum horizontal camera angle after calibration.", -180, 180, 1),
    Control(ControlKind::Toggle, kControlNone, "pitch-enabled", "Enable pitch", "Apply vertical head rotation."),
    Control(ControlKind::Toggle, kControlNone, "pitch-inverted", "Invert pitch", "Reverse vertical head rotation."),
    Control(ControlKind::FloatSlider, kControlNone, "pitch-sensitivity", "Pitch sensitivity", "Scale vertical tracker rotation.", 0.05, 10, 0.05),
    Control(ControlKind::FloatSlider, kControlNone, "pitch-minimum", "Down limit", "Minimum vertical camera angle after calibration.", -180, 180, 1),
    Control(ControlKind::FloatSlider, kControlNone, "pitch-center", "Pitch neutral angle", "Camera pitch at the physically recentered tracker pose.", -180, 180, 1),
    Control(ControlKind::FloatSlider, kControlNone, "pitch-maximum", "Up limit", "Maximum vertical camera angle after calibration.", -180, 180, 1),
    Control(ControlKind::Toggle, kControlNone, "roll-enabled", "Enable roll", "Apply head roll rotation."),
    Control(ControlKind::Toggle, kControlNone, "roll-inverted", "Invert roll", "Reverse head roll rotation."),
    Control(ControlKind::FloatSlider, kControlNone, "roll-sensitivity", "Roll sensitivity", "Scale tracker roll rotation.", 0.05, 10, 0.05),
    Control(ControlKind::FloatSlider, kControlNone, "roll-minimum", "Left-bank limit", "Minimum camera roll angle after calibration.", -180, 180, 1),
    Control(ControlKind::FloatSlider, kControlNone, "roll-center", "Level offset", "Camera roll at the physically recentered tracker pose.", -180, 180, 1),
    Control(ControlKind::FloatSlider, kControlNone, "roll-maximum", "Right-bank limit", "Maximum camera roll angle after calibration.", -180, 180, 1),
    Control(ControlKind::Action, kControlNone, "recenter-now", "Recenter now",
        "Capture the current OpenTrack pose as the new neutral center.")
};
static_assert(std::tuple_size_v<decltype(g_axesControls)> == 20);

void ConfigurePoseAxis(
    AbsoluteControlPanelExperimental::HeadPoseAxisDescriptorV1& axis,
    AbsoluteControlPanelExperimental::HeadPoseView view,
    std::string_view id, std::string_view label,
    std::string_view sensitivityId, std::string_view minimumId,
    std::string_view centerId, std::string_view maximumId,
    std::string_view enabledId, std::string_view invertedId) noexcept {
    axis.view = view;
    Copy(axis.axisId, id);
    Copy(axis.label, label);
    Copy(axis.sensitivityControlId, sensitivityId);
    Copy(axis.minimumControlId, minimumId);
    Copy(axis.centerControlId, centerId);
    Copy(axis.maximumControlId, maximumId);
    Copy(axis.enabledControlId, enabledId);
    Copy(axis.invertedControlId, invertedId);
}

AbsoluteControlPanelExperimental::Result __cdecl ReadHeadPoseFrame(
    void*, AbsoluteControlPanelExperimental::LiveFrameV1* frame) noexcept {
    namespace Live = AbsoluteControlPanelExperimental;
    if (!frame || frame->structSize < Live::kLiveFrameV1HeadPoseSize)
        return Live::Result::InvalidArgument;
    try {
        const auto state = HeadTracking::GetLiveState();
        const bool available = state.poseAvailable;
        *frame = {};
        frame->kind = Live::ComponentKind::HeadPose;
        frame->sequence = g_headPoseSequence.fetch_add(
            1, std::memory_order_relaxed) + 1;
        frame->monotonicTimestampUs = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count());
        if (!available) frame->flags |= Live::kFrameUnavailable;
        frame->headPose.axisCount = 3;
        // The visual order follows the design sketch: side-profile pitch,
        // top-view yaw, then the roll artificial horizon.
        constexpr std::array<std::size_t, 3> sourceIndices{ 1, 0, 2 };
        for (std::size_t index = 0; index < sourceIndices.size(); ++index) {
            auto& axis = frame->headPose.axes[index];
            const auto source = sourceIndices[index];
            axis.available = available ? 1U : 0U;
            axis.trackerDegrees = state.trackerDegrees[source];
            axis.outputDegrees = state.outputDegrees[source];
        }
        return Live::Result::Ok;
    } catch (...) {
        return Live::Result::Rejected;
    }
}

AbsoluteControlPanelExperimental::LiveChannelDescriptorV1 HeadPoseChannel() {
    namespace Live = AbsoluteControlPanelExperimental;
    Live::LiveChannelDescriptorV1 descriptor;
    descriptor.structSize = Live::kLiveChannelDescriptorV1HeadPoseSize;
    Copy(descriptor.moduleId, "absolute.head_tracking");
    Copy(descriptor.pageId, "axes");
    Copy(descriptor.channelId, "head-pose");
    Copy(descriptor.title, "Live head pose and asymmetric axis calibration");
    descriptor.kind = Live::ComponentKind::HeadPose;
    descriptor.flags = Live::kLivePresentationPinned;
    descriptor.readLiveFrame = &ReadHeadPoseFrame;
    descriptor.headPose.axisCount = 3;
    Copy(descriptor.headPose.recenterControlId, "recenter-now");
    Copy(descriptor.headPose.deadzoneControlId, "deadzone");
    ConfigurePoseAxis(descriptor.headPose.axes[0],
        Live::HeadPoseView::Profile, "pitch", "Pitch / vertical",
        "pitch-sensitivity", "pitch-minimum", "pitch-center",
        "pitch-maximum", "pitch-enabled", "pitch-inverted");
    ConfigurePoseAxis(descriptor.headPose.axes[1],
        Live::HeadPoseView::Top, "yaw", "Yaw / horizontal",
        "yaw-sensitivity", "yaw-minimum", "yaw-center", "yaw-maximum",
        "yaw-enabled", "yaw-inverted");
    ConfigurePoseAxis(descriptor.headPose.axes[2],
        Live::HeadPoseView::ArtificialHorizon, "roll", "Roll / horizon",
        "roll-sensitivity", "roll-minimum", "roll-center", "roll-maximum",
        "roll-enabled", "roll-inverted");
    return descriptor;
}

bool RegisterHeadPoseChannel(HMODULE module) noexcept {
    namespace Live = AbsoluteControlPanelExperimental;
    try {
        const auto address = GetProcAddress(module,
            "AbsoluteControlPanel_QueryLiveComponentsExperimental");
        if (!address) return false;
        using Query = const Live::ExperimentalApiV1*(__cdecl*)(
            std::uint32_t) noexcept;
        const auto query = reinterpret_cast<Query>(address);
        const auto* api = query(Live::kAbiVersion);
        constexpr auto required = offsetof(Live::ExperimentalApiV1,
            capabilities) + sizeof(api->capabilities);
        if (!api || api->structSize < required ||
            api->abiVersion != Live::kAbiVersion ||
            !api->registerLiveChannel ||
            (api->capabilities & Live::kLiveCapabilityHeadPose) == 0) {
            return false;
        }
        const auto descriptor = HeadPoseChannel();
        const auto result = api->registerLiveChannel(&descriptor);
        if (result != Live::Result::Ok && result != Live::Result::Duplicate)
            return false;
        g_liveHostApi.store(api, std::memory_order_release);
        return true;
    } catch (...) {
        return false;
    }
}

const std::array g_aimControls{
    Control(ControlKind::Toggle, kControlNone, "limit-aim-cone",
        "Limit aim to ship-forward cone",
        "Keep camera-directed fire and native target assistance inside a ship-forward cone."),
    Control(ControlKind::FloatSlider, kControlNone, "firing-cone-half-angle",
        "Half-angle",
        "Angular distance from ship-forward; the full included cone is twice this value.",
        1, 90, 1)
};

const std::array g_bindingControls{
    Control(ControlKind::InputBinding, kKeyboardChord, "panel-binding", "Legacy menu shortcut (keyboard)",
        "Legacy standalone menu chord. Absolute Control Panel itself remains available through its own entry."),
    Control(ControlKind::InputBinding, kKeyboardChord, "toggle-binding", "Tracking toggle (keyboard)",
        "Enable or suspend tracking without changing the saved master switch."),
    Control(ControlKind::InputBinding, kKeyboardChord, "recenter-binding", "Recenter (keyboard)",
        "Capture the current OpenTrack pose as neutral and clear retained manual look."),
    Control(ControlKind::FloatSlider, kControlNone, "manual-look-rate", "Key / hat trim rate",
        "Degrees per second added while a keyboard or digital flight control is held.", 1, 360, 1),
    Control(ControlKind::InputBinding, kKeyboardChord, "look-left-binding", "Look left (keyboard)",
        "Hold to trim the retained view left."),
    Control(ControlKind::InputBinding, kKeyboardChord, "look-right-binding", "Look right (keyboard)",
        "Hold to trim the retained view right."),
    Control(ControlKind::InputBinding, kKeyboardChord, "look-down-binding", "Look down (keyboard)",
        "Hold to trim the retained view down."),
    Control(ControlKind::InputBinding, kKeyboardChord, "look-up-binding", "Look up (keyboard)",
        "Hold to trim the retained view up."),
    Control(ControlKind::InputBinding, kKeyboardChord, "bank-left-binding", "Bank left (keyboard)",
        "Hold to trim the retained head roll left."),
    Control(ControlKind::InputBinding, kKeyboardChord, "bank-right-binding", "Bank right (keyboard)",
        "Hold to trim the retained head roll right."),
    Control(ControlKind::Choice, kControlNone, "mouse-look-modifier", "Mouse-look hold modifier",
        "Move the retained view with raw mouse deltas only while this modifier is held.", 0, 3, 1),
    Control(ControlKind::FloatSlider, kControlNone, "mouse-look-sensitivity", "Mouse-look sensitivity",
        "Degrees of retained view trim per raw mouse count.", 0.001, 2, 0.001),
    Control(ControlKind::Toggle, kControlNone, "mouse-look-invert-horizontal", "Invert mouse horizontal",
        "Reverse modifier-held horizontal mouse look."),
    Control(ControlKind::Toggle, kControlNone, "mouse-look-invert-vertical", "Invert mouse vertical",
        "Reverse modifier-held vertical mouse look.")
};

constexpr std::uint32_t kControllerBinding = kBindingController |
    kBindingClearable;
constexpr std::uint32_t kControllerAxisBinding = kBindingController |
    kBindingClearable;
const std::array g_busBindingControls{
    Control(ControlKind::InputBinding, kKeyboardChord, "panel-binding", "Legacy menu shortcut (keyboard)",
        "Legacy standalone menu chord. Absolute Control Panel itself remains available through its own entry."),
    Control(ControlKind::InputBinding, kKeyboardChord, "toggle-binding", "Tracking toggle (keyboard)",
        "Keyboard fallback for enabling or suspending head tracking."),
    Control(ControlKind::InputBinding, kControllerBinding,
        "toggle-controller-binding", "Tracking toggle (flight control)",
        "Record a joystick, throttle, button-box, or POV control through the Absolute Input Bus."),
    Control(ControlKind::InputBinding, kKeyboardChord, "recenter-binding", "Recenter (keyboard)",
        "Keyboard fallback for capturing the current OpenTrack pose as neutral."),
    Control(ControlKind::InputBinding, kControllerBinding,
        "recenter-controller-binding", "Recenter (flight control)",
        "Record a joystick, throttle, button-box, or POV control through the Absolute Input Bus."),
    Control(ControlKind::FloatSlider, kControlNone, "manual-look-rate", "Key / hat trim rate",
        "Degrees per second added while a keyboard or digital flight control is held.", 1, 360, 1),
    Control(ControlKind::InputBinding, kKeyboardChord, "look-left-binding", "Look left (keyboard)",
        "Hold to trim the retained view left."),
    Control(ControlKind::InputBinding, kKeyboardChord, "look-right-binding", "Look right (keyboard)",
        "Hold to trim the retained view right."),
    Control(ControlKind::InputBinding, kKeyboardChord, "look-down-binding", "Look down (keyboard)",
        "Hold to trim the retained view down."),
    Control(ControlKind::InputBinding, kKeyboardChord, "look-up-binding", "Look up (keyboard)",
        "Hold to trim the retained view up."),
    Control(ControlKind::InputBinding, kKeyboardChord, "bank-left-binding", "Bank left (keyboard)",
        "Hold to trim the retained head roll left."),
    Control(ControlKind::InputBinding, kKeyboardChord, "bank-right-binding", "Bank right (keyboard)",
        "Hold to trim the retained head roll right."),
    Control(ControlKind::Choice, kControlNone, "mouse-look-modifier", "Mouse-look hold modifier",
        "Move the retained view with raw mouse deltas only while this modifier is held.", 0, 3, 1),
    Control(ControlKind::FloatSlider, kControlNone, "mouse-look-sensitivity", "Mouse-look sensitivity",
        "Degrees of retained view trim per raw mouse count.", 0.001, 2, 0.001),
    Control(ControlKind::Toggle, kControlNone, "mouse-look-invert-horizontal", "Invert mouse horizontal",
        "Reverse modifier-held horizontal mouse look."),
    Control(ControlKind::Toggle, kControlNone, "mouse-look-invert-vertical", "Invert mouse vertical",
        "Reverse modifier-held vertical mouse look."),
    Control(ControlKind::FloatSlider, kControlNone, "controller-axis-deadzone", "Manual-look axis deadzone",
        "Normalized center region ignored on captured Absolute Input Bus axes.", 0, 0.95, 0.01),
    Control(ControlKind::InputBinding, kControllerAxisBinding,
        "controller-yaw-axis-binding", "Yaw look axis (flight control)",
        "Capture a proportional axis; center contributes zero and endpoints map to the asymmetric yaw limits."),
    Control(ControlKind::Toggle, kControlNone, "controller-yaw-inverted", "Invert yaw look axis",
        "Reverse the captured proportional yaw axis."),
    Control(ControlKind::InputBinding, kControllerAxisBinding,
        "controller-pitch-axis-binding", "Pitch look axis (flight control)",
        "Capture a proportional axis; center contributes zero and endpoints map to the asymmetric pitch limits."),
    Control(ControlKind::Toggle, kControlNone, "controller-pitch-inverted", "Invert pitch look axis",
        "Reverse the captured proportional pitch axis."),
    Control(ControlKind::InputBinding, kControllerAxisBinding,
        "controller-roll-axis-binding", "Roll look axis (flight control)",
        "Capture a proportional axis; center contributes zero and endpoints map to the asymmetric roll limits."),
    Control(ControlKind::Toggle, kControlNone, "controller-roll-inverted", "Invert roll look axis",
        "Reverse the captured proportional roll axis."),
    Control(ControlKind::InputBinding, kControllerBinding,
        "look-left-controller-binding", "Look left (flight control)",
        "Hold a button or POV direction to trim the retained view left."),
    Control(ControlKind::InputBinding, kControllerBinding,
        "look-right-controller-binding", "Look right (flight control)",
        "Hold a button or POV direction to trim the retained view right."),
    Control(ControlKind::InputBinding, kControllerBinding,
        "look-down-controller-binding", "Look down (flight control)",
        "Hold a button or POV direction to trim the retained view down."),
    Control(ControlKind::InputBinding, kControllerBinding,
        "look-up-controller-binding", "Look up (flight control)",
        "Hold a button or POV direction to trim the retained view up."),
    Control(ControlKind::InputBinding, kControllerBinding,
        "bank-left-controller-binding", "Bank left (flight control)",
        "Hold a button or POV direction to trim retained head roll left."),
    Control(ControlKind::InputBinding, kControllerBinding,
        "bank-right-controller-binding", "Bank right (flight control)",
        "Hold a button or POV direction to trim retained head roll right.")
};

HotkeySettings* KeyboardBinding(Config& config, std::string_view id) noexcept {
    if (id == "panel-binding") return &config.workbench;
    if (id == "toggle-binding") return &config.toggle;
    if (id == "recenter-binding") return &config.recenter;
    if (id == "look-left-binding") return &config.manualLook.lookLeft;
    if (id == "look-right-binding") return &config.manualLook.lookRight;
    if (id == "look-down-binding") return &config.manualLook.lookDown;
    if (id == "look-up-binding") return &config.manualLook.lookUp;
    if (id == "bank-left-binding") return &config.manualLook.bankLeft;
    if (id == "bank-right-binding") return &config.manualLook.bankRight;
    return nullptr;
}

std::string* ControllerBinding(Config& config, std::string_view id) noexcept {
    if (id == "toggle-controller-binding")
        return &config.toggleControllerBinding;
    if (id == "recenter-controller-binding")
        return &config.recenterControllerBinding;
    if (id == "controller-yaw-axis-binding")
        return &config.manualLook.yawControllerAxisBinding;
    if (id == "controller-pitch-axis-binding")
        return &config.manualLook.pitchControllerAxisBinding;
    if (id == "controller-roll-axis-binding")
        return &config.manualLook.rollControllerAxisBinding;
    if (id == "look-left-controller-binding")
        return &config.manualLook.lookLeftControllerBinding;
    if (id == "look-right-controller-binding")
        return &config.manualLook.lookRightControllerBinding;
    if (id == "look-down-controller-binding")
        return &config.manualLook.lookDownControllerBinding;
    if (id == "look-up-controller-binding")
        return &config.manualLook.lookUpControllerBinding;
    if (id == "bank-left-controller-binding")
        return &config.manualLook.bankLeftControllerBinding;
    if (id == "bank-right-controller-binding")
        return &config.manualLook.bankRightControllerBinding;
    return nullptr;
}

bool ControllerAxisBindingControl(std::string_view id) noexcept {
    return id == "controller-yaw-axis-binding" ||
        id == "controller-pitch-axis-binding" ||
        id == "controller-roll-axis-binding";
}

AxisSettings* Axis(Config& config, std::string_view id) noexcept {
    if (id.starts_with("yaw-")) return &config.yaw;
    if (id.starts_with("pitch-")) return &config.pitch;
    if (id.starts_with("roll-")) return &config.roll;
    return nullptr;
}

Result __cdecl Read(void*, const char* rawId, ValueV1* output) noexcept {
    if (!rawId || !output || output->structSize < sizeof(ValueV1)) return Result::InvalidArgument;
    try {
        const std::string_view id{rawId};
        const Config config = Configuration::Get();
        if (id == "master-enabled") *output = Boolean(config.enabled);
        else if (id == "poll-rate") *output = Integer(config.pollRateHz);
        else if (id == "stale-timeout") *output = Integer(config.staleMilliseconds);
        else if (id == "deadzone") *output = Float(config.deadzoneDegrees);
        else if (id == "smoothing") *output = Float(config.smoothing);
        else if (id == "limit-aim-cone")
            *output = Boolean(config.limitAimToShipForwardCone);
        else if (id == "firing-cone-half-angle")
            *output = Float(config.firingConeHalfAngleDegrees);
        else if (id == "diagnostic-log") *output = Boolean(config.enableLog);
        else if (id == "manual-look-rate")
            *output = Float(config.manualLook.digitalRateDegreesPerSecond);
        else if (id == "mouse-look-modifier")
            *output = Integer(static_cast<int>(config.manualLook.mouseModifier));
        else if (id == "mouse-look-sensitivity")
            *output = Float(config.manualLook.mouseDegreesPerCount);
        else if (id == "mouse-look-invert-horizontal")
            *output = Boolean(config.manualLook.invertMouseHorizontal);
        else if (id == "mouse-look-invert-vertical")
            *output = Boolean(config.manualLook.invertMouseVertical);
        else if (id == "controller-axis-deadzone")
            *output = Float(config.manualLook.controllerAxisDeadzone);
        else if (id == "controller-yaw-inverted")
            *output = Boolean(config.manualLook.invertControllerYaw);
        else if (id == "controller-pitch-inverted")
            *output = Boolean(config.manualLook.invertControllerPitch);
        else if (id == "controller-roll-inverted")
            *output = Boolean(config.manualLook.invertControllerRoll);
        else if (const auto* binding = KeyboardBinding(
                const_cast<Config&>(config), id))
            *output = String(BindingCodec::Display(*binding));
        else if (const auto* binding = ControllerBinding(
                const_cast<Config&>(config), id))
            *output = String(InputBusClient::DisplayBinding(*binding));
        else if (const auto* axis = Axis(const_cast<Config&>(config), id)) {
            if (id.ends_with("-enabled")) *output = Boolean(axis->enabled);
            else if (id.ends_with("-inverted")) *output = Boolean(axis->inverted);
            else if (id.ends_with("-sensitivity")) *output = Float(axis->sensitivity);
            else if (id.ends_with("-minimum")) *output = Float(axis->minimumDegrees);
            else if (id.ends_with("-center")) *output = Float(axis->centerDegrees);
            else if (id.ends_with("-maximum")) *output = Float(axis->maximumDegrees);
            else return Result::NotFound;
        } else return Result::NotFound;
        return Result::Ok;
    } catch (...) { return Result::Rejected; }
}

bool BooleanValue(const ValueV1& value, bool& output) noexcept {
    if (value.kind != ValueKind::Boolean || value.booleanValue > 1) return false;
    output = value.booleanValue != 0;
    return true;
}
bool IntegerValue(const ValueV1& value, int minimum, int maximum, int& output) noexcept {
    if (value.kind != ValueKind::Integer || value.integerValue < minimum ||
        value.integerValue > maximum) return false;
    output = static_cast<int>(value.integerValue);
    return true;
}
bool FloatValue(const ValueV1& value, float minimum, float maximum, float& output) noexcept {
    if (value.kind != ValueKind::Float || !std::isfinite(value.floatValue) ||
        value.floatValue < minimum || value.floatValue > maximum) return false;
    output = static_cast<float>(value.floatValue);
    return true;
}

Result __cdecl Write(void*, const char* rawId, const ValueV1* value) noexcept {
    if (!rawId || !value || value->structSize < sizeof(ValueV1)) return Result::InvalidArgument;
    try {
        const std::string_view id{rawId};
        const Config current = Configuration::Get();
        Config proposed = current;
        bool accepted = false;
        if (id == "master-enabled") accepted = BooleanValue(*value, proposed.enabled);
        else if (id == "poll-rate") accepted = IntegerValue(*value, 30, 500, proposed.pollRateHz);
        else if (id == "stale-timeout") accepted = IntegerValue(*value, 50, 5000, proposed.staleMilliseconds);
        else if (id == "deadzone") accepted = FloatValue(*value, 0, 20, proposed.deadzoneDegrees);
        else if (id == "smoothing") accepted = FloatValue(*value, 0, 0.99F, proposed.smoothing);
        else if (id == "limit-aim-cone")
            accepted = BooleanValue(*value, proposed.limitAimToShipForwardCone);
        else if (id == "firing-cone-half-angle")
            accepted = FloatValue(*value, 1, 90, proposed.firingConeHalfAngleDegrees);
        else if (id == "diagnostic-log") accepted = BooleanValue(*value, proposed.enableLog);
        else if (id == "manual-look-rate") accepted = FloatValue(*value, 1, 360,
            proposed.manualLook.digitalRateDegreesPerSecond);
        else if (id == "mouse-look-modifier") {
            int modifier{};
            accepted = IntegerValue(*value, 0, 3, modifier);
            if (accepted) proposed.manualLook.mouseModifier =
                static_cast<MouseLookModifier>(modifier);
        }
        else if (id == "mouse-look-sensitivity") accepted = FloatValue(*value,
            0.001F, 2, proposed.manualLook.mouseDegreesPerCount);
        else if (id == "mouse-look-invert-horizontal") accepted = BooleanValue(
            *value, proposed.manualLook.invertMouseHorizontal);
        else if (id == "mouse-look-invert-vertical") accepted = BooleanValue(
            *value, proposed.manualLook.invertMouseVertical);
        else if (id == "controller-axis-deadzone") accepted = FloatValue(*value,
            0, 0.95F, proposed.manualLook.controllerAxisDeadzone);
        else if (id == "controller-yaw-inverted") accepted = BooleanValue(
            *value, proposed.manualLook.invertControllerYaw);
        else if (id == "controller-pitch-inverted") accepted = BooleanValue(
            *value, proposed.manualLook.invertControllerPitch);
        else if (id == "controller-roll-inverted") accepted = BooleanValue(
            *value, proposed.manualLook.invertControllerRoll);
        else if (auto* binding = KeyboardBinding(proposed, id)) {
            if (value->kind == ValueKind::String &&
                std::memchr(value->stringValue, '\0', kStringValueCapacity)) {
                HotkeySettings parsed;
                accepted = BindingCodec::ParseKeyboard(value->stringValue, parsed);
                if (accepted) *binding = parsed;
            }
        } else if (auto* binding = ControllerBinding(proposed, id)) {
            if (value->kind == ValueKind::String &&
                std::memchr(value->stringValue, '\0', kStringValueCapacity)) {
                const std::string_view encoded{value->stringValue};
                accepted = ControllerAxisBindingControl(id)
                    ? InputBusClient::IsAxisBinding(encoded)
                    : InputBusClient::IsDigitalBinding(encoded);
                if (accepted) *binding = encoded;
            }
        } else if (auto* axis = Axis(proposed, id)) {
            if (id.ends_with("-enabled")) accepted = BooleanValue(*value, axis->enabled);
            else if (id.ends_with("-inverted")) accepted = BooleanValue(*value, axis->inverted);
            else if (id.ends_with("-sensitivity")) accepted = FloatValue(*value, 0.05F, 10, axis->sensitivity);
            else if (id.ends_with("-minimum")) accepted = FloatValue(*value, -180, 180, axis->minimumDegrees);
            else if (id.ends_with("-center")) accepted = FloatValue(*value, -180, 180, axis->centerDegrees);
            else if (id.ends_with("-maximum")) accepted = FloatValue(*value, -180, 180, axis->maximumDegrees);
            if (accepted && (axis->minimumDegrees > axis->centerDegrees ||
                    axis->centerDegrees > axis->maximumDegrees)) accepted = false;
        } else return Result::NotFound;
        if (!accepted) return Result::InvalidArgument;
        std::lock_guard lock(g_editMutex);
        if (!g_editing) { g_rollback = current; g_editing = true; }
        Configuration::Apply(proposed);
        return Result::Ok;
    } catch (...) { return Result::Rejected; }
}

Result __cdecl ReadChoiceOptions(void*, const char* rawId,
    ChoiceOptionV1* options, std::uint32_t capacity,
    std::uint32_t* outputCount) noexcept {
    if (!rawId || !outputCount) return Result::InvalidArgument;
    if (std::string_view(rawId) != "mouse-look-modifier") {
        *outputCount = 0;
        return Result::NotFound;
    }
    constexpr std::array<std::pair<std::int64_t, std::string_view>, 4> values{{
        {static_cast<int>(MouseLookModifier::Disabled), "Disabled"},
        {static_cast<int>(MouseLookModifier::Alt), "Alt"},
        {static_cast<int>(MouseLookModifier::Control), "Ctrl"},
        {static_cast<int>(MouseLookModifier::Shift), "Shift"},
    }};
    *outputCount = static_cast<std::uint32_t>(values.size());
    if (!options || capacity < values.size()) return Result::CapacityExceeded;
    for (std::size_t index = 0; index < values.size(); ++index) {
        options[index] = {};
        options[index].value = values[index].first;
        Copy(options[index].label, values[index].second);
    }
    return Result::Ok;
}

Result __cdecl Invoke(void*, const char* rawId) noexcept {
    if (!rawId) return Result::InvalidArgument;
    if (std::string_view(rawId) != "recenter-now") return Result::NotFound;
    HeadTracking::RequestRecenter();
    return Result::Ok;
}

Result __cdecl Apply(void*) noexcept {
    std::lock_guard lock(g_editMutex);
    if (!Configuration::Save()) return Result::WriteFailure;
    g_editing = false;
    return Result::Ok;
}

void __cdecl Cancel(void*) noexcept {
    std::lock_guard lock(g_editMutex);
    if (g_editing) Configuration::Apply(g_rollback);
    g_editing = false;
}

bool ControllerBindingControl(std::string_view id) noexcept {
    Config config;
    return ControllerBinding(config, id) != nullptr;
}

Result __cdecl BeginBindingCapture(void*, const char* rawId) noexcept {
    if (!rawId || !ControllerBindingControl(rawId)) return Result::NotFound;
    const auto allowed = ControllerAxisBindingControl(rawId)
        ? AbsoluteInputBusApi::kCaptureAxes
        : AbsoluteInputBusApi::kCaptureDigital;
    switch (InputBusClient::BeginCapture(allowed)) {
    case AbsoluteInputBusApi::Result::Ok: return Result::Ok;
    case AbsoluteInputBusApi::Result::NotReady: return Result::NotReady;
    case AbsoluteInputBusApi::Result::InvalidArgument:
        return Result::InvalidArgument;
    default: return Result::Rejected;
    }
}

Result __cdecl PollBindingCapture(void*, const char* rawId,
    BindingCaptureV1* output) noexcept {
    if (!rawId || !ControllerBindingControl(rawId) || !output ||
        output->structSize < sizeof(*output)) return Result::InvalidArgument;
    AbsoluteInputBusApi::CaptureResultV1 capture;
    const auto result = InputBusClient::PollCapture(capture);
    if (result == AbsoluteInputBusApi::Result::NotReady) return Result::NotReady;
    if (result != AbsoluteInputBusApi::Result::Ok) return Result::Rejected;
    *output = {};
    switch (capture.state) {
    case AbsoluteInputBusApi::CaptureState::Idle:
        output->state = BindingCaptureState::Idle;
        break;
    case AbsoluteInputBusApi::CaptureState::Capturing:
        output->state = BindingCaptureState::Capturing;
        break;
    case AbsoluteInputBusApi::CaptureState::Captured:
        output->state = BindingCaptureState::Captured;
        break;
    case AbsoluteInputBusApi::CaptureState::Cancelled:
        output->state = BindingCaptureState::Cancelled;
        break;
    case AbsoluteInputBusApi::CaptureState::TimedOut:
        output->state = BindingCaptureState::TimedOut;
        break;
    case AbsoluteInputBusApi::CaptureState::Error:
        output->state = BindingCaptureState::Error;
        break;
    }
    if (std::memchr(capture.binding.bindingText, '\0',
            sizeof(capture.binding.bindingText))) {
        Copy(output->binding, capture.binding.bindingText);
    } else if (output->state == BindingCaptureState::Captured) {
        output->state = BindingCaptureState::Error;
    }
    if (std::memchr(capture.detail, '\0', sizeof(capture.detail))) {
        Copy(output->detail, capture.detail);
    }
    if (output->state == BindingCaptureState::Captured &&
        !(ControllerAxisBindingControl(rawId)
            ? InputBusClient::IsAxisBinding(output->binding)
            : InputBusClient::IsDigitalBinding(output->binding))) {
        output->state = BindingCaptureState::Error;
        Copy(output->detail, ControllerAxisBindingControl(rawId)
            ? "Absolute Input Bus returned an invalid axis binding."
            : "Absolute Input Bus returned an invalid digital binding.");
    }
    return Result::Ok;
}

Result __cdecl CancelBindingCapture(void*, const char* rawId) noexcept {
    if (!rawId || !ControllerBindingControl(rawId)) return Result::NotFound;
    const auto result = InputBusClient::CancelCapture();
    return result == AbsoluteInputBusApi::Result::Ok ||
        result == AbsoluteInputBusApi::Result::StaleSession
        ? Result::Ok : Result::Rejected;
}

PageDescriptorV1 Page(std::string_view id, std::string_view label,
    std::string_view description, const ControlDescriptorV1* controls,
    std::uint32_t count) {
    PageDescriptorV1 page;
    Copy(page.moduleId, "absolute.head_tracking");
    Copy(page.pageId, id);
    Copy(page.displayName, label);
    Copy(page.description, description);
    page.controlCount = count;
    page.controls = controls;
    page.readValue = &Read;
    page.writeDraft = &Write;
    page.invokeAction = &Invoke;
    page.apply = &Apply;
    page.cancel = &Cancel;
    return page;
}

const ModuleDescriptorV1 g_module = [] {
    ModuleDescriptorV1 module;
    Copy(module.moduleId, "absolute.head_tracking");
    Copy(module.displayName, "Absolute Head Tracking");
    Copy(module.description, "Tracked and manually trimmed cockpit camera look for Starfield.");
    return module;
}();

const std::array g_pages{
    Page("axes", "Axes", "Live yaw, pitch, roll, deadzone, and physical recenter controls.", g_axesControls.data(), static_cast<std::uint32_t>(g_axesControls.size())),
    Page("general", "General", "Runtime, response, and diagnostic controls.", g_generalControls.data(), static_cast<std::uint32_t>(g_generalControls.size())),
    Page("aim-restriction", "Aim Restriction",
        "Bound camera-directed fire and native target assistance to ship-forward.",
        g_aimControls.data(), static_cast<std::uint32_t>(g_aimControls.size()))
};
static_assert(std::tuple_size_v<decltype(g_pages)> == 3);

bool Valid(const ApiV1* api) noexcept {
    constexpr auto required = offsetof(ApiV1, registerModule) + sizeof(api->registerModule);
    return api && api->structSize >= required && api->abiVersion == kAbiVersion &&
        api->moduleId && std::string_view(api->moduleId) == kModuleId &&
        api->registerPage && api->unregisterModule && api->requestRefresh &&
        api->registerModule;
}
} // namespace

namespace ControlPanelSubscriber {
AbsoluteControlPanelApi::Result RegisterDiscoveredHost() noexcept {
    using AbsoluteControlPanelApi::Result;
    try {
        for (const auto moduleName : ControlPanelSubscriber::Discovery::kHostModuleNames) {
            const auto module = GetModuleHandleW(moduleName.data());
            if (!module) continue;
            const auto address = GetProcAddress(module, "AbsoluteControlPanel_QueryApi");
            if (!address) return Result::NotFound;
            const auto query = reinterpret_cast<AbsoluteControlPanelQueryApi>(address);
            const auto* api = query(AbsoluteControlPanelApi::kAbiVersion);
            if (!Valid(api)) return Result::Rejected;
            const auto moduleResult = api->registerModule(&g_module);
            if (moduleResult != Result::Ok && moduleResult != Result::Duplicate)
                return moduleResult;
            for (const auto& page : g_pages) {
                const auto result = api->registerPage(&page);
                if (result != Result::Ok && result != Result::Duplicate) return result;
            }
            // The native Axes instrument is independent of the optional
            // bindings page. Commit it first so a future provider-capture or
            // binding-descriptor incompatibility cannot suppress live pose UI.
            const bool headPoseHosted = RegisterHeadPoseChannel(module);
            const bool providerCapture = api->structSize >=
                    offsetof(ApiV1, capabilities) + sizeof(api->capabilities) &&
                (api->capabilities & kCapabilityProviderBindingCapture) != 0;
            auto bindings = providerCapture
                ? Page("bindings", "Bindings",
                    "Record retained keyboard, mouse-modifier, and DirectInput look controls.",
                    g_busBindingControls.data(),
                    static_cast<std::uint32_t>(g_busBindingControls.size()))
                : Page("bindings", "Bindings",
                    "Record retained keyboard look and modifier-held mouse controls.",
                    g_bindingControls.data(),
                    static_cast<std::uint32_t>(g_bindingControls.size()));
            bindings.readChoiceOptions = &ReadChoiceOptions;
            if (providerCapture) {
                bindings.beginBindingCapture = &BeginBindingCapture;
                bindings.pollBindingCapture = &PollBindingCapture;
                bindings.cancelBindingCapture = &CancelBindingCapture;
            }
            const auto bindingResult = api->registerPage(&bindings);
            if (bindingResult != Result::Ok &&
                bindingResult != Result::Duplicate) {
                RuntimePaths::Log("[ControlPanel]", std::format(
                    "Bindings page registration failed with result {}; core pages and live Axes remain hosted.",
                    static_cast<std::uint32_t>(bindingResult)), true);
            }
            g_hostApi.store(api, std::memory_order_release);
            RuntimePaths::Log("[ControlPanel]",
                bindingResult != Result::Ok && bindingResult != Result::Duplicate
                    ? (headPoseHosted
                        ? "Registered core pages and the native Head Pose surface; Bindings page is unavailable."
                        : "Registered core pages; Bindings and Head Pose surfaces are unavailable.")
                    : headPoseHosted
                    ? "Registered four pages and the native Head Pose calibration surface."
                    : (providerCapture
                        ? "Registered four pages with flight-control capture; Head Pose live rendering is unavailable on this host."
                        : "Registered four pages with keyboard capture; optional live rendering is unavailable on this host."),
                true);
            return Result::Ok;
        }
        return Result::NotFound;
    } catch (...) { return Result::Rejected; }
}

bool IsHosted() noexcept { return g_hostApi.load(std::memory_order_acquire) != nullptr; }

bool IsMenuOpen() noexcept {
    const auto* api = g_hostApi.load(std::memory_order_acquire);
    constexpr auto required = offsetof(AbsoluteControlPanelApi::ApiV1, isOpen) +
        sizeof(api->isOpen);
    return api && api->structSize >= required && api->isOpen && api->isOpen() != 0;
}

bool IsInputCaptureActive() noexcept {
    const auto* api = g_hostApi.load(std::memory_order_acquire);
    constexpr auto required = offsetof(AbsoluteControlPanelApi::ApiV1, isInputCaptureActive) +
        sizeof(api->isInputCaptureActive);
    return api && api->structSize >= required && api->isInputCaptureActive &&
        api->isInputCaptureActive() != 0;
}
} // namespace ControlPanelSubscriber
