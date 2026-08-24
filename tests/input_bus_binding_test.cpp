#include "InputBusBinding.h"
#include "InputBusConsumerPolicy.h"

#include <cassert>
#include <cstdint>

int main() {
    using namespace InputBusBinding;
    static_assert(sizeof(AbsoluteInputBusApi::DeviceInfoV1) == 360);
    static_assert(sizeof(AbsoluteInputBusApi::DeviceSnapshotV1) == 1352);
    static_assert(sizeof(AbsoluteInputBusApi::CaptureResultV1) == 672);
    static_assert(sizeof(AbsoluteInputBusApi::RuntimeContextV1) == 56);
    static_assert(offsetof(AbsoluteInputBusApi::ApiV1, cancelCapture) == 96);
    static_assert(sizeof(AbsoluteInputBusApi::ApiV1) == 104);
    Parsed binding;
    assert(Parse("{12345678-1234-1234-1234-123456789ABC}@button:33", binding));
    assert(binding.kind == Kind::Button && binding.controlId == 33 &&
        binding.digitalChannel == 32);
    assert(ControlLabel(binding) == "Button 33");
    assert(Parse("{12345678-1234-1234-1234-123456789ABC}@pov:2:left", binding));
    assert(binding.kind == Kind::PovDirection && binding.controlId == 140 &&
        binding.digitalChannel == 139 && ControlLabel(binding) == "POV 3 Left");
    assert(!Parse("@button:1", binding));
    assert(!Parse("device@button:0", binding));
    assert(!Parse("device@button:129", binding));
    assert(!Parse("device@pov:4:up", binding));
    assert(Parse("device@axis:0x30", binding));
    assert(binding.kind == Kind::Axis && binding.axisIndex == 0 &&
        binding.axisUsage == 0x30 && ControlLabel(binding) == "X axis");
    assert(Parse("device@axis:0x37", binding));
    assert(binding.axisIndex == 7 && ControlLabel(binding) == "Slider 2 axis");
    assert(!Parse("device@axis:0x2F", binding));
    assert(!Parse("device@axis:0x38", binding));

    PressCounter counter;
    assert(ObservePresses(counter, 1, 10, false) == 0);
    assert(ObservePresses(counter, 1, 12, false) == 2);
    assert(ObservePresses(counter, 1, 13, true) == 0);
    assert(ObservePresses(counter, 1, 13, false) == 0);
    assert(ObservePresses(counter, 2, 50, false) == 0);
    counter.value = UINT32_MAX;
    assert(ObservePresses(counter, 2, 0, false) == 1);

    AbsoluteInputBusApi::RuntimeContextV1 context;
    context.context = AbsoluteInputBusApi::RuntimeContext::Piloting;
    context.validSignals = AbsoluteInputBusApi::kContextSignalIsPilot |
        AbsoluteInputBusApi::kContextSignalGameplayActive;
    context.activeSignals = context.validSignals;
    context.selectedOutputAgeMilliseconds = 24;
    assert(InputBusConsumerPolicy::PilotSignalAge(context) == 24);
    context.activeSignals &=
        ~AbsoluteInputBusApi::kContextSignalGameplayActive;
    assert(InputBusConsumerPolicy::PilotSignalAge(context) == -1);
    context.context = AbsoluteInputBusApi::RuntimeContext::Suspended;
    assert(InputBusConsumerPolicy::PilotSignalAge(context) == -1);
    return 0;
}
