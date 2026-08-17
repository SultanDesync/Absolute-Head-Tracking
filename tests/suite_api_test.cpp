#include "AbsoluteHeadTrackingAPI.h"
#include "AbsoluteCameraOwnershipAPI.h"

#include <cassert>
#include <cstddef>
#include <type_traits>

int main() {
    using namespace AbsoluteHeadTrackingApi;
    static_assert(std::is_trivially_copyable_v<ConfigurationV1>);
    static_assert(std::is_trivially_copyable_v<StatusV1>);
    static_assert(std::is_trivially_copyable_v<PoseV1>);
    static_assert(offsetof(ApiV1, getStatus) > offsetof(ApiV1, version));
    static_assert(sizeof(AbsoluteCameraOwnershipApi::ApiV1) == 48);
    static_assert(offsetof(AbsoluteCameraOwnershipApi::ApiV1,
                           declareAbsoluteHeadTrackingOwner) == 16);
    static_assert(offsetof(AbsoluteCameraOwnershipApi::ApiV1,
                           cockpitSignalAgeMilliseconds) == 40);

    ApiV1 api{};
    assert(api.structSize == sizeof(ApiV1));
    assert(api.abiVersion == kAbiVersion);
    ConfigurationV1 config{};
    assert(config.structSize == sizeof(ConfigurationV1));
    return 0;
}
