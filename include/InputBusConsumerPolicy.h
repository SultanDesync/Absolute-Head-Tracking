#pragma once

#include "AbsoluteInputBusAPI.h"

#include <cstdint>

namespace InputBusConsumerPolicy {

// Head Tracking remains fail-closed unless HOTAS publishes an authoritative
// Piloting state. The selected-output age preserves the existing strict camera
// freshness gate while the bus owns classification and gameplay suspension.
inline std::int64_t PilotSignalAge(
    const AbsoluteInputBusApi::RuntimeContextV1& context) noexcept {
    if (context.context != AbsoluteInputBusApi::RuntimeContext::Piloting ||
        (context.validSignals &
            AbsoluteInputBusApi::kContextSignalIsPilot) == 0 ||
        (context.activeSignals &
            AbsoluteInputBusApi::kContextSignalIsPilot) == 0) {
        return -1;
    }
    if ((context.validSignals &
            AbsoluteInputBusApi::kContextSignalGameplayActive) != 0 &&
        (context.activeSignals &
            AbsoluteInputBusApi::kContextSignalGameplayActive) == 0) {
        return -1;
    }
    return context.selectedOutputAgeMilliseconds;
}

} // namespace InputBusConsumerPolicy
