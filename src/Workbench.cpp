#include "PCH.h"

#include "AimRestriction.h"
#include "Workbench.h"
#include "CameraHook.h"
#include "Config.h"
#include "HeadTracking.h"
#include "HotkeyPolicy.h"
#include "OpenTrack.h"
#include "Plugin.h"
#include "UIHook.h"

#include <imgui.h>

namespace {
constexpr ImVec4 kAccent{ 0.20F, 0.72F, 0.95F, 1.0F };
constexpr ImVec4 kBrand{ 1.0F, 0.64F, 0.16F, 1.0F };
constexpr ImVec4 kGood{ 0.25F, 0.86F, 0.52F, 1.0F };
constexpr ImVec4 kWarn{ 1.0F, 0.78F, 0.24F, 1.0F };
constexpr ImVec4 kBad{ 1.0F, 0.30F, 0.30F, 1.0F };

enum class BindingTarget : std::uint8_t {
    None,
    Workbench,
    Activation,
    Recenter,
};

struct CaptureState {
    BindingTarget target = BindingTarget::None;
    std::array<bool, 256> previous{};
    bool waitingForRelease = false;
};

CaptureState g_capture;
std::atomic<bool> g_hotkeyCaptureActive{ false };
bool g_capturePopupRequested = false;

bool KeyDown(int virtualKey) {
    return virtualKey > 0 && virtualKey < 256 &&
        (GetAsyncKeyState(virtualKey) & 0x8000) != 0;
}

bool IsMouseKey(int virtualKey) {
    return virtualKey == VK_LBUTTON || virtualKey == VK_RBUTTON ||
        virtualKey == VK_MBUTTON || virtualKey == VK_XBUTTON1 ||
        virtualKey == VK_XBUTTON2;
}

bool IsModifierKey(int virtualKey) {
    return virtualKey == VK_SHIFT || virtualKey == VK_LSHIFT ||
        virtualKey == VK_RSHIFT || virtualKey == VK_CONTROL ||
        virtualKey == VK_LCONTROL || virtualKey == VK_RCONTROL ||
        virtualKey == VK_MENU || virtualKey == VK_LMENU ||
        virtualKey == VK_RMENU || virtualKey == VK_LWIN ||
        virtualKey == VK_RWIN;
}

bool AnyKeyboardKeyDown() {
    for (int key = 1; key < 256; ++key) {
        if (!IsMouseKey(key) && KeyDown(key)) return true;
    }
    return false;
}

std::string KeyName(int virtualKey) {
    if (virtualKey <= 0) return "Unbound";
    if (virtualKey >= 'A' && virtualKey <= 'Z')
        return std::string(1, static_cast<char>(virtualKey));
    if (virtualKey >= '0' && virtualKey <= '9')
        return std::string(1, static_cast<char>(virtualKey));
    if (virtualKey >= VK_F1 && virtualKey <= VK_F24)
        return std::format("F{}", virtualKey - VK_F1 + 1);
    if (virtualKey >= VK_NUMPAD0 && virtualKey <= VK_NUMPAD9)
        return std::format("Numpad {}", virtualKey - VK_NUMPAD0);

    switch (virtualKey) {
    case VK_SPACE: return "Space";
    case VK_TAB: return "Tab";
    case VK_RETURN: return "Enter";
    case VK_BACK: return "Backspace";
    case VK_DELETE: return "Delete";
    case VK_INSERT: return "Insert";
    case VK_HOME: return "Home";
    case VK_END: return "End";
    case VK_PRIOR: return "Page Up";
    case VK_NEXT: return "Page Down";
    case VK_LEFT: return "Left";
    case VK_RIGHT: return "Right";
    case VK_UP: return "Up";
    case VK_DOWN: return "Down";
    case VK_ADD: return "Numpad +";
    case VK_SUBTRACT: return "Numpad -";
    case VK_MULTIPLY: return "Numpad *";
    case VK_DIVIDE: return "Numpad /";
    case VK_DECIMAL: return "Numpad .";
    case VK_PAUSE: return "Pause";
    case VK_SNAPSHOT: return "Print Screen";
    case VK_SCROLL: return "Scroll Lock";
    case VK_CAPITAL: return "Caps Lock";
    default: break;
    }

    const UINT scanCode = MapVirtualKeyA(static_cast<UINT>(virtualKey), MAPVK_VK_TO_VSC_EX);
    LONG keyNameParameter = static_cast<LONG>((scanCode & 0xFFU) << 16U);
    if ((scanCode & 0xFF00U) != 0) keyNameParameter |= 1 << 24;
    std::array<char, 64> name{};
    if (GetKeyNameTextA(keyNameParameter, name.data(), static_cast<int>(name.size())) > 0)
        return name.data();
    return std::format("Key 0x{:02X}", virtualKey);
}

std::string HotkeyName(const HotkeySettings& hotkey) {
    if (!HotkeyPolicy::IsBound(hotkey)) return "Unbound";
    std::string result;
    auto append = [&result](std::string_view part) {
        if (!result.empty()) result += " + ";
        result += part;
    };
    if (hotkey.control) append("Ctrl");
    if (hotkey.alt) append("Alt");
    if (hotkey.shift) append("Shift");
    append(KeyName(hotkey.virtualKey));
    return result;
}

const char* CaptureLabel(BindingTarget target) {
    switch (target) {
    case BindingTarget::Workbench: return "Workbench menu";
    case BindingTarget::Activation: return "Tracking activation toggle";
    case BindingTarget::Recenter: return "Recenter";
    case BindingTarget::None: break;
    }
    return "Hotkey";
}

HotkeySettings* TargetHotkey(Config& config, BindingTarget target) {
    switch (target) {
    case BindingTarget::Workbench: return &config.workbench;
    case BindingTarget::Activation: return &config.toggle;
    case BindingTarget::Recenter: return &config.recenter;
    case BindingTarget::None: break;
    }
    return nullptr;
}

void BeginCapture(BindingTarget target) {
    g_capture.target = target;
    g_capture.waitingForRelease = false;
    for (int key = 0; key < 256; ++key) g_capture.previous[key] = KeyDown(key);
    g_hotkeyCaptureActive.store(true, std::memory_order_release);
    // HotkeyEditor runs under its row's PushID scope. Defer OpenPopup until the
    // root window scope where DrawCaptureModal resolves the same popup ID.
    g_capturePopupRequested = true;
}

void FinishCapture(bool waitForRelease) {
    g_capture.target = BindingTarget::None;
    g_capture.waitingForRelease = waitForRelease;
    if (!waitForRelease)
        g_hotkeyCaptureActive.store(false, std::memory_order_release);
    ImGui::CloseCurrentPopup();
}

void AbandonCapture() {
    g_capture.target = BindingTarget::None;
    g_capture.waitingForRelease = false;
    g_capturePopupRequested = false;
    g_hotkeyCaptureActive.store(false, std::memory_order_release);
}

bool CapturedKeyEdge(int virtualKey) {
    return KeyDown(virtualKey) && !g_capture.previous[virtualKey];
}

void RefreshCaptureKeyState() {
    for (int key = 0; key < 256; ++key) g_capture.previous[key] = KeyDown(key);
}

bool DrawCaptureModal(Config& editing) {
    bool changed = false;
    if (!ImGui::BeginPopupModal("Capture hotkey", nullptr,
            ImGuiWindowFlags_AlwaysAutoResize)) return false;

    ImGui::TextColored(kAccent, "%s", CaptureLabel(g_capture.target));
    ImGui::Spacing();
    ImGui::TextWrapped("Hold Ctrl, Alt, and/or Shift, then press the main key. "
        "Pressing a key without modifiers creates a single-key binding.");
    ImGui::Spacing();
    ImGui::TextColored(kBrand, "Waiting for keyboard input...");
    ImGui::TextDisabled("Escape cancels. Backspace clears the binding.");
    if (KeyDown(VK_LWIN) || KeyDown(VK_RWIN))
        ImGui::TextColored(kWarn, "Windows-key chords are not supported.");

    if (CapturedKeyEdge(VK_ESCAPE)) {
        FinishCapture(true);
    } else if (CapturedKeyEdge(VK_BACK)) {
        if (auto* hotkey = TargetHotkey(editing, g_capture.target)) {
            *hotkey = { 0, false, false, false };
            changed = true;
        }
        FinishCapture(true);
    } else if (!KeyDown(VK_LWIN) && !KeyDown(VK_RWIN)) {
        for (int key = 1; key < 256; ++key) {
            if (IsMouseKey(key) || IsModifierKey(key) || !CapturedKeyEdge(key)) continue;
            if (auto* hotkey = TargetHotkey(editing, g_capture.target)) {
                *hotkey = {
                    key, KeyDown(VK_CONTROL), KeyDown(VK_MENU), KeyDown(VK_SHIFT)
                };
                changed = true;
            }
            FinishCapture(true);
            break;
        }
    }

    if (g_capture.target != BindingTarget::None) RefreshCaptureKeyState();
    ImGui::Spacing();
    if (ImGui::Button("Cancel", ImVec2(100.0F, 0.0F))) FinishCapture(false);
    ImGui::EndPopup();
    return changed;
}

void ApplyAbsoluteStyle() {
    auto& style = ImGui::GetStyle();
    style.WindowRounding = 5.0F;
    style.ChildRounding = 4.0F;
    style.FrameRounding = 3.0F;
    style.GrabRounding = 3.0F;
    style.TabRounding = 3.0F;
    style.WindowPadding = { 14.0F, 12.0F };
    style.ItemSpacing = { 8.0F, 7.0F };
    auto* colors = style.Colors;
    colors[ImGuiCol_WindowBg] = { 0.045F, 0.055F, 0.070F, 0.98F };
    colors[ImGuiCol_ChildBg] = { 0.065F, 0.078F, 0.095F, 0.92F };
    colors[ImGuiCol_FrameBg] = { 0.10F, 0.12F, 0.145F, 1.0F };
    colors[ImGuiCol_FrameBgHovered] = { 0.14F, 0.19F, 0.23F, 1.0F };
    colors[ImGuiCol_FrameBgActive] = { 0.16F, 0.24F, 0.29F, 1.0F };
    colors[ImGuiCol_Button] = { 0.10F, 0.28F, 0.40F, 1.0F };
    colors[ImGuiCol_ButtonHovered] = { 0.14F, 0.40F, 0.56F, 1.0F };
    colors[ImGuiCol_ButtonActive] = { 0.12F, 0.34F, 0.48F, 1.0F };
    colors[ImGuiCol_Header] = { 0.10F, 0.30F, 0.43F, 1.0F };
    colors[ImGuiCol_HeaderHovered] = { 0.14F, 0.40F, 0.56F, 1.0F };
    colors[ImGuiCol_SliderGrab] = kAccent;
    colors[ImGuiCol_CheckMark] = kBrand;
    colors[ImGuiCol_Separator] = { 0.22F, 0.31F, 0.38F, 1.0F };
}

void StatusLabel(const char* label, bool good, const char* detail) {
    ImGui::TextColored(good ? kGood : kBad, good ? "●" : "●");
    ImGui::SameLine();
    ImGui::TextUnformatted(label);
    ImGui::SameLine();
    ImGui::TextDisabled("%s", detail);
}

bool AxisEditor(const char* id, const char* label, AxisSettings& axis,
                float tracker, float output) {
    bool changed = false;
    ImGui::PushID(id);
    ImGui::BeginChild("AxisCard", ImVec2(0.0F, 230.0F), true);
    ImGui::TextColored(kAccent, "%s", label);
    ImGui::SameLine();
    ImGui::TextDisabled("tracker %+.1f deg  output %+.1f deg", tracker, output);
    changed |= ImGui::Checkbox("Enabled", &axis.enabled);
    ImGui::SameLine();
    changed |= ImGui::Checkbox("Invert", &axis.inverted);
    ImGui::BeginDisabled(!axis.enabled);
    ImGui::SetNextItemWidth(-1.0F);
    changed |= ImGui::SliderFloat("Sensitivity", &axis.sensitivity,
        0.05F, 10.0F, "%.2fx");
    ImGui::SetNextItemWidth(-1.0F);
    changed |= ImGui::SliderFloat("Minimum", &axis.minimumDegrees,
        -180.0F, 180.0F, "%.0f deg");
    ImGui::SetNextItemWidth(-1.0F);
    changed |= ImGui::SliderFloat("Center", &axis.centerDegrees,
        -180.0F, 180.0F, "%.0f deg");
    ImGui::SetNextItemWidth(-1.0F);
    changed |= ImGui::SliderFloat("Maximum", &axis.maximumDegrees,
        -180.0F, 180.0F, "%.0f deg");
    ImGui::EndDisabled();
    ImGui::EndChild();
    ImGui::PopID();
    return changed;
}

bool HotkeyEditor(const char* id, const char* label, const char* description,
                  BindingTarget target, HotkeySettings& hotkey) {
    bool changed = false;
    ImGui::PushID(id);
    ImGui::TextUnformatted(label);
    ImGui::TextDisabled("%s", description);
    ImGui::SameLine(365.0F);
    const std::string binding = HotkeyName(hotkey);
    if (ImGui::Button(binding.c_str(), ImVec2(185.0F, 0.0F))) BeginCapture(target);
    ImGui::SameLine();
    if (ImGui::SmallButton("Clear")) {
        hotkey = { 0, false, false, false };
        changed = true;
    }
    ImGui::PopID();
    return changed;
}
} // namespace

namespace Workbench {

void Initialize() {
    UIHook::SetDrawCallback(&Draw);
}

bool IsCapturingHotkey() {
    return UIHook::IsUIOpen() &&
        g_hotkeyCaptureActive.load(std::memory_order_acquire);
}

void Draw() {
    static bool styleApplied = false;
    if (g_capture.waitingForRelease && !AnyKeyboardKeyDown()) {
        g_capture.waitingForRelease = false;
        g_hotkeyCaptureActive.store(false, std::memory_order_release);
    }
    if (!styleApplied) {
        ApplyAbsoluteStyle();
        styleApplied = true;
    }

    bool windowOpen = true;
    ImGui::SetNextWindowSize(ImVec2(830.0F, 760.0F), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(660.0F, 560.0F), ImVec2(1500.0F, 1200.0F));
    if (!ImGui::Begin("Absolute Head Tracking", &windowOpen, ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        AbandonCapture();
        if (!windowOpen) UIHook::Close();
        return;
    }

    // Popup names are resolved through ImGui's current ID stack. Check modal
    // lifetime only after entering the same root window scope used to open it.
    if (g_capture.target != BindingTarget::None && !g_capturePopupRequested &&
        !ImGui::IsPopupOpen("Capture hotkey")) {
        AbandonCapture();
    }

    Config editing = Configuration::Get();
    const auto live = HeadTracking::GetLiveState();
    const auto hooks = CameraHook::GetStatus();
    const auto aim = AimRestriction::GetStatus();
    bool changed = false;
    bool saveRequested = false;

    ImGui::TextColored(kBrand, "ABSOLUTE");
    ImGui::SameLine();
    ImGui::Text("HEAD TRACKING  %.*s", static_cast<int>(Plugin::VersionString.size()),
        Plugin::VersionString.data());
    ImGui::TextDisabled("Standalone OpenTrack camera look for Starfield");
    ImGui::Separator();

    if (ImGui::BeginTable("Status", 2, ImGuiTableFlags_SizingStretchSame)) {
        ImGui::TableNextColumn();
        StatusLabel("Camera seam", hooks.cameraHookInstalled,
            hooks.cameraHookInstalled ? "exact gate passed" : "unavailable");
        StatusLabel("Cockpit observer", hooks.cockpitObserverInstalled,
            hooks.cockpitObserverInstalled ? "independent of AbsoluteZero" : "unavailable");
        StatusLabel("Aim restriction", aim.installed,
            aim.installed ? "atomic hook set ready" : "camera activation blocked");
        ImGui::TableNextColumn();
        const bool trackerConnected = live.connection == OpenTrack::ConnectionState::Connected;
        const char* trackerDetail = trackerConnected ? "live FreeTrack frames" :
            (live.connection == OpenTrack::ConnectionState::Stale ? "output stale" : "waiting for OpenTrack");
        StatusLabel("OpenTrack", trackerConnected, trackerDetail);
        const bool cockpitFresh = hooks.cockpitSignalAgeMilliseconds >= 0 &&
            hooks.cockpitSignalAgeMilliseconds <= 400;
        const std::string cockpitDetail = hooks.cockpitSignalAgeMilliseconds < 0
            ? "waiting for player flight output"
            : std::format("signal age {} ms", hooks.cockpitSignalAgeMilliseconds);
        StatusLabel("Cockpit gate", cockpitFresh, cockpitDetail.c_str());
        ImGui::EndTable();
    }

    ImGui::Spacing();
    changed |= ImGui::Checkbox("Enable camera look", &editing.enabled);
    ImGui::SameLine();
    bool runtimeEnabled = live.runtimeEnabled;
    if (ImGui::Checkbox("Runtime enabled", &runtimeEnabled)) {
        HeadTracking::SetRuntimeEnabled(runtimeEnabled);
    }
    ImGui::SameLine();
    if (ImGui::Button("Recenter now")) HeadTracking::RequestRecenter();

    ImGui::Spacing();
    ImGui::SeparatorText("Axes (legacy fallback)");
    const int axisColumns = ImGui::GetContentRegionAvail().x >= 960.0F ? 3 : 1;
    if (ImGui::BeginTable("AxisGrid", axisColumns, ImGuiTableFlags_SizingStretchSame)) {
        ImGui::TableNextColumn();
        changed |= AxisEditor("yaw", "YAW", editing.yaw,
            live.trackerDegrees[0], live.outputDegrees[0]);
        ImGui::TableNextColumn();
        changed |= AxisEditor("pitch", "PITCH", editing.pitch,
            live.trackerDegrees[1], live.outputDegrees[1]);
        ImGui::TableNextColumn();
        changed |= AxisEditor("roll", "ROLL", editing.roll,
            live.trackerDegrees[2], live.outputDegrees[2]);
        ImGui::EndTable();
    }

    ImGui::SeparatorText("Response");
    if (ImGui::BeginTable("Response", 3, ImGuiTableFlags_SizingStretchSame)) {
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-1.0F);
        changed |= ImGui::SliderFloat("Deadzone", &editing.deadzoneDegrees,
            0.0F, 20.0F, "%.1f°");
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-1.0F);
        changed |= ImGui::SliderFloat("Smoothing", &editing.smoothing,
            0.0F, 0.99F, "%.2f");
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-1.0F);
        changed |= ImGui::SliderInt("Stale timeout", &editing.staleMilliseconds,
            50, 2000, "%d ms");
        ImGui::EndTable();
    }

    ImGui::SeparatorText("Aim Restriction");
    changed |= ImGui::Checkbox("Limit aim to ship-forward cone",
        &editing.limitAimToShipForwardCone);
    ImGui::TextDisabled(
        "Disable only to opt into unrestricted camera-relative fire and native target assistance.");
    ImGui::BeginDisabled(!editing.limitAimToShipForwardCone);
    ImGui::SetNextItemWidth(360.0F);
    changed |= ImGui::SliderFloat("Half-angle", &editing.firingConeHalfAngleDegrees,
        1.0F, 90.0F, "%.0f°");
    ImGui::SameLine();
    ImGui::TextDisabled("%.0f° full cone", editing.firingConeHalfAngleDegrees * 2.0F);
    ImGui::EndDisabled();

    ImGui::SeparatorText("Bindings");
    changed |= HotkeyEditor("workbench", "Workbench menu",
        "Open or close this tuning window.", BindingTarget::Workbench, editing.workbench);
    changed |= HotkeyEditor("activation", "Tracking activation toggle",
        "Enable or suspend tracking without changing the saved master switch.",
        BindingTarget::Activation, editing.toggle);
    changed |= HotkeyEditor("recenter", "Recenter",
        "Capture the current OpenTrack pose as neutral.",
        BindingTarget::Recenter, editing.recenter);
    if (HotkeyPolicy::Same(editing.workbench, editing.toggle) ||
        HotkeyPolicy::Same(editing.workbench, editing.recenter) ||
        HotkeyPolicy::Same(editing.toggle, editing.recenter)) {
        ImGui::TextColored(kBad,
            "Binding conflict: each action should use a different key or chord.");
    }
    if (!HotkeyPolicy::IsBound(editing.workbench)) {
        ImGui::TextColored(kWarn,
            "The workbench is unbound. Save another binding before closing it.");
    }
    ImGui::TextDisabled("Click a binding to capture Ctrl/Alt/Shift chords or a single key.");

    if (ImGui::CollapsingHeader("Advanced")) {
        changed |= ImGui::SliderInt("Polling rate", &editing.pollRateHz, 30, 500, "%d Hz");
        changed |= ImGui::Checkbox("Enable diagnostic log after restart", &editing.enableLog);
        changed |= ImGui::Checkbox("Enable workbench after restart", &editing.enableWorkbench);
    }

    if (g_capturePopupRequested) {
        ImGui::OpenPopup("Capture hotkey");
        g_capturePopupRequested = false;
    }
    changed |= DrawCaptureModal(editing);

    ImGui::Spacing();
    ImGui::Separator();
    if (ImGui::Button("Save", ImVec2(110.0F, 0.0F))) saveRequested = true;
    ImGui::SameLine();
    if (ImGui::Button("Reload", ImVec2(110.0F, 0.0F))) {
        Configuration::Load();
        changed = false;
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset defaults", ImVec2(130.0F, 0.0F))) {
        editing = ConfigPolicy::Defaults();
        changed = true;
    }
    ImGui::SameLine();
    ImGui::TextColored(Configuration::Dirty() ? kWarn : kGood,
        Configuration::Dirty() ? "Unsaved live tuning" : "Configuration saved");

    if (changed) Configuration::Apply(editing);
    if (saveRequested) Configuration::Save();

    ImGui::End();
    if (!windowOpen) UIHook::Close();
}

} // namespace Workbench
