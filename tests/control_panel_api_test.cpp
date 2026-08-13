#include "AbsoluteControlPanelAPI.h"

#include <cassert>
#include <cstring>

int main() {
    using namespace AbsoluteControlPanelApi;
    static_assert(static_cast<std::uint32_t>(ControlKind::InputBinding) == 5);
    static_assert(offsetof(ValueV1, structSize) == 0);
    static_assert(offsetof(ApiV1, isInputCaptureActive) >
        offsetof(ApiV1, registerModule));
    ControlDescriptorV1 binding;
    binding.kind = ControlKind::InputBinding;
    binding.flags = kBindingKeyboard | kBindingModifiers | kBindingClearable;
    strcpy_s(binding.controlId, "recenter-binding");
    assert((binding.flags & kBindingKeyboard) != 0);
    assert(std::strcmp(kModuleId.data(), "absolute.control_panel") == 0);
    return 0;
}
