#include "AbsoluteControlPanelAPI.h"
#include "ControlPanelDiscovery.h"

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
