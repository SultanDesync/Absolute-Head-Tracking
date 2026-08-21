#include "PCH.h"

#include "ControlPanelSubscriber.h"

#include "BindingCodec.h"
#include "Config.h"
#include "ControlPanelDiscovery.h"
#include "HeadTracking.h"
#include "InputBusClient.h"
#include "RuntimePaths.h"

namespace {
using namespace AbsoluteControlPanelApi;

std::atomic<const ApiV1*> g_hostApi{};
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
    Control(ControlKind::FloatSlider, kControlNone, "deadzone", "Deadzone",
        "Tracker movement ignored around the captured center, in degrees.", 0, 20, 0.1),
    Control(ControlKind::FloatSlider, kControlNone, "smoothing", "Smoothing",
        "Output smoothing from immediate response to strongly filtered motion.", 0, 0.99, 0.01),
    Control(ControlKind::Toggle, kControlRequiresRestart | kControlAdvanced,
        "diagnostic-log", "Diagnostic logging",
        "Write detailed Absolute Head Tracking diagnostics after restart."),
    Control(ControlKind::Action, kControlNone, "recenter-now", "Recenter now",
        "Capture the current OpenTrack pose as the new neutral center.")
};

const std::array g_axesControls{
    Control(ControlKind::Toggle, kControlNone, "yaw-enabled", "Enable yaw", "Apply horizontal head rotation."),
    Control(ControlKind::Toggle, kControlNone, "yaw-inverted", "Invert yaw", "Reverse horizontal head rotation."),
    Control(ControlKind::FloatSlider, kControlNone, "yaw-sensitivity", "Yaw sensitivity", "Scale horizontal tracker rotation.", 0.05, 10, 0.05),
    Control(ControlKind::FloatSlider, kControlNone, "yaw-maximum", "Maximum yaw", "Clamp horizontal camera rotation in degrees.", 1, 180, 1),
    Control(ControlKind::Toggle, kControlNone, "pitch-enabled", "Enable pitch", "Apply vertical head rotation."),
    Control(ControlKind::Toggle, kControlNone, "pitch-inverted", "Invert pitch", "Reverse vertical head rotation."),
    Control(ControlKind::FloatSlider, kControlNone, "pitch-sensitivity", "Pitch sensitivity", "Scale vertical tracker rotation.", 0.05, 10, 0.05),
    Control(ControlKind::FloatSlider, kControlNone, "pitch-maximum", "Maximum pitch", "Clamp vertical camera rotation in degrees.", 1, 180, 1),
    Control(ControlKind::Toggle, kControlNone, "roll-enabled", "Enable roll", "Apply head roll rotation."),
    Control(ControlKind::Toggle, kControlNone, "roll-inverted", "Invert roll", "Reverse head roll rotation."),
    Control(ControlKind::FloatSlider, kControlNone, "roll-sensitivity", "Roll sensitivity", "Scale tracker roll rotation.", 0.05, 10, 0.05),
    Control(ControlKind::FloatSlider, kControlNone, "roll-maximum", "Maximum roll", "Clamp camera roll in degrees.", 1, 180, 1)
};
static_assert(std::tuple_size_v<decltype(g_axesControls)> == 12);
const std::array g_bindingControls{
    Control(ControlKind::InputBinding, kKeyboardChord, "panel-binding", "Legacy menu shortcut (keyboard)",
        "Legacy standalone menu chord. Absolute Control Panel itself remains available through its own entry."),
    Control(ControlKind::InputBinding, kKeyboardChord, "toggle-binding", "Tracking toggle (keyboard)",
        "Enable or suspend tracking without changing the saved master switch."),
    Control(ControlKind::InputBinding, kKeyboardChord, "recenter-binding", "Recenter (keyboard)",
        "Capture the current OpenTrack pose as neutral.")
};

constexpr std::uint32_t kControllerBinding = kBindingController |
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
        "Record a joystick, throttle, button-box, or POV control through the Absolute Input Bus.")
};

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
        else if (id == "diagnostic-log") *output = Boolean(config.enableLog);
        else if (id == "panel-binding") *output = String(BindingCodec::Display(config.workbench));
        else if (id == "toggle-binding") *output = String(BindingCodec::Display(config.toggle));
        else if (id == "recenter-binding") *output = String(BindingCodec::Display(config.recenter));
        else if (id == "toggle-controller-binding")
            *output = String(InputBusClient::DisplayBinding(
                config.toggleControllerBinding));
        else if (id == "recenter-controller-binding")
            *output = String(InputBusClient::DisplayBinding(
                config.recenterControllerBinding));
        else if (const auto* axis = Axis(const_cast<Config&>(config), id)) {
            if (id.ends_with("-enabled")) *output = Boolean(axis->enabled);
            else if (id.ends_with("-inverted")) *output = Boolean(axis->inverted);
            else if (id.ends_with("-sensitivity")) *output = Float(axis->sensitivity);
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
        else if (id == "diagnostic-log") accepted = BooleanValue(*value, proposed.enableLog);
        else if (id == "panel-binding" || id == "toggle-binding" || id == "recenter-binding") {
            if (value->kind == ValueKind::String &&
                std::memchr(value->stringValue, '\0', kStringValueCapacity)) {
                HotkeySettings parsed;
                accepted = BindingCodec::ParseKeyboard(value->stringValue, parsed);
                if (accepted) {
                    if (id == "panel-binding") proposed.workbench = parsed;
                    else if (id == "toggle-binding") proposed.toggle = parsed;
                    else proposed.recenter = parsed;
                }
            }
        } else if (id == "toggle-controller-binding" ||
            id == "recenter-controller-binding") {
            if (value->kind == ValueKind::String &&
                std::memchr(value->stringValue, '\0', kStringValueCapacity)) {
                const std::string_view binding{value->stringValue};
                accepted = InputBusClient::IsDigitalBinding(binding);
                if (accepted) {
                    if (id == "toggle-controller-binding")
                        proposed.toggleControllerBinding = binding;
                    else proposed.recenterControllerBinding = binding;
                }
            }
        } else if (auto* axis = Axis(proposed, id)) {
            if (id.ends_with("-enabled")) accepted = BooleanValue(*value, axis->enabled);
            else if (id.ends_with("-inverted")) accepted = BooleanValue(*value, axis->inverted);
            else if (id.ends_with("-sensitivity")) accepted = FloatValue(*value, 0.05F, 10, axis->sensitivity);
            else if (id.ends_with("-maximum")) accepted = FloatValue(*value, 1, 180, axis->maximumDegrees);
        } else return Result::NotFound;
        if (!accepted) return Result::InvalidArgument;
        std::lock_guard lock(g_editMutex);
        if (!g_editing) { g_rollback = current; g_editing = true; }
        Configuration::Apply(proposed);
        return Result::Ok;
    } catch (...) { return Result::Rejected; }
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
    return id == "toggle-controller-binding" ||
        id == "recenter-controller-binding";
}

Result __cdecl BeginBindingCapture(void*, const char* rawId) noexcept {
    if (!rawId || !ControllerBindingControl(rawId)) return Result::NotFound;
    switch (InputBusClient::BeginDigitalCapture()) {
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
        !InputBusClient::IsDigitalBinding(output->binding)) {
        output->state = BindingCaptureState::Error;
        Copy(output->detail, "Absolute Input Bus returned an invalid digital binding.");
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
    Copy(module.description, "OpenTrack camera look configuration for Starfield.");
    return module;
}();

const std::array g_pages{
    Page("general", "General", "Runtime, response, diagnostics, and recenter controls.", g_generalControls.data(), static_cast<std::uint32_t>(g_generalControls.size())),
    Page("axes", "Axes", "Yaw, pitch, and roll response settings.", g_axesControls.data(), static_cast<std::uint32_t>(g_axesControls.size()))
};
static_assert(std::tuple_size_v<decltype(g_pages)> == 2);

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
            const bool providerCapture = api->structSize >=
                    offsetof(ApiV1, capabilities) + sizeof(api->capabilities) &&
                (api->capabilities & kCapabilityProviderBindingCapture) != 0;
            auto bindings = providerCapture
                ? Page("bindings", "Bindings",
                    "Record keyboard fallbacks and DirectInput flight controls.",
                    g_busBindingControls.data(),
                    static_cast<std::uint32_t>(g_busBindingControls.size()))
                : Page("bindings", "Bindings",
                    "Record single keys or Ctrl, Alt, and Shift chords.",
                    g_bindingControls.data(),
                    static_cast<std::uint32_t>(g_bindingControls.size()));
            if (providerCapture) {
                bindings.beginBindingCapture = &BeginBindingCapture;
                bindings.pollBindingCapture = &PollBindingCapture;
                bindings.cancelBindingCapture = &CancelBindingCapture;
            }
            const auto bindingResult = api->registerPage(&bindings);
            if (bindingResult != Result::Ok &&
                bindingResult != Result::Duplicate) return bindingResult;
            g_hostApi.store(api, std::memory_order_release);
            RuntimePaths::Log("[ControlPanel]",
                providerCapture
                    ? "Registered three dynamic pages with provider-owned flight-control capture."
                    : "Registered three dynamic pages with keyboard capture; the host does not advertise provider capture.",
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
