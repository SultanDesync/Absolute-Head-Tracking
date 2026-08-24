#include "AbsoluteControlPanelAPI.h"
#include "ControlPanelDiscovery.h"
#include "LiveComponentsExperimentalAPI.h"

#include <cassert>
#include <cstring>
#include <ranges>

int main() {
    using namespace AbsoluteControlPanelApi;
    static_assert(static_cast<std::uint32_t>(ControlKind::InputBinding) == 5);
    static_assert(offsetof(ValueV1, structSize) == 0);
    static_assert(offsetof(ApiV1, isInputCaptureActive) >
        offsetof(ApiV1, registerModule));
    static_assert(offsetof(PageDescriptorV1, cancelBindingCapture) >
        offsetof(PageDescriptorV1, readChoiceOptions));
    static_assert(sizeof(BindingCaptureV1) == 456);
    ControlDescriptorV1 binding;
    binding.kind = ControlKind::InputBinding;
    binding.flags = kBindingKeyboard | kBindingModifiers | kBindingClearable;
    strcpy_s(binding.controlId, "recenter-binding");
    assert((binding.flags & kBindingKeyboard) != 0);
    assert((kCapabilityProviderBindingCapture &
        kCapabilityLabeledChoices) == 0);
    assert(std::strcmp(kModuleId.data(), "absolute.control_panel") == 0);

    namespace Live = AbsoluteControlPanelExperimental;
    static_assert(static_cast<std::uint32_t>(Live::ComponentKind::HeadPose) == 4);
    static_assert(Live::kLiveChannelDescriptorV1RadialResponseSize ==
        offsetof(Live::LiveChannelDescriptorV1, headPose));
    static_assert(Live::kLiveFrameV1RadialResponseSize ==
        offsetof(Live::LiveFrameV1, headPose));
    Live::LiveChannelDescriptorV1 pose;
    pose.kind = Live::ComponentKind::HeadPose;
    pose.headPose.axisCount = 3;
    pose.headPose.axes[0].view = Live::HeadPoseView::Profile;
    strcpy_s(pose.headPose.recenterControlId, "recenter-now");
    strcpy_s(pose.headPose.deadzoneControlId, "deadzone");
    assert(std::strcmp(pose.headPose.recenterControlId,
        "recenter-now") == 0);
    assert(std::strcmp(pose.headPose.deadzoneControlId,
        "deadzone") == 0);
    assert((Live::kLiveCapabilities & Live::kLiveCapabilityHeadPose) != 0);

    using namespace ControlPanelSubscriber::Discovery;
    assert(std::ranges::find(kHostModuleNames,
        std::wstring_view(L"AbsoluteControlPanelResearchDev.dll")) !=
        kHostModuleNames.end());
    assert(DecideRegistration(Result::Ok, false) ==
        RegistrationDecision::UseControlPanel);
    assert(DecideRegistration(Result::NotReady, false) ==
        RegistrationDecision::RetryAtPostPostDataLoad);
    assert(DecideRegistration(Result::NotReady, true) ==
        RegistrationDecision::UseFallback);
    assert(DecideRegistration(Result::NotFound, false) ==
        RegistrationDecision::UseFallback);
    return 0;
}
