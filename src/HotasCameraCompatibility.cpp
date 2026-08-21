#include "PCH.h"

#include "AbsoluteCameraOwnershipAPI.h"
#include "HotasCameraCompatibility.h"
#include "RuntimePaths.h"

namespace {
std::atomic<const AbsoluteCameraOwnershipApi::ApiV1*> g_api{};
}

namespace HotasCameraCompatibility {

Result Connect() noexcept
{
    try {
        const HMODULE module = GetModuleHandleW(L"AbsoluteHOTAS.dll");
        if (!module) return Result::NotLoaded;

        const FARPROC address = GetProcAddress(
            module, "AbsoluteHOTAS_QueryCameraOwnershipApi");
        if (!address) return Result::Incompatible;

        using Query = const AbsoluteCameraOwnershipApi::ApiV1*(__cdecl*)(
            std::uint32_t) noexcept;
        const auto* api = reinterpret_cast<Query>(address)(
            AbsoluteCameraOwnershipApi::kAbiVersion);
        constexpr std::size_t minimum =
            offsetof(AbsoluteCameraOwnershipApi::ApiV1, cockpitSignalAgeMilliseconds) +
            sizeof(api->cockpitSignalAgeMilliseconds);
        if (!api || api->structSize < minimum ||
            api->abiVersion != AbsoluteCameraOwnershipApi::kAbiVersion ||
            !api->providerId || std::string_view(api->providerId) != "absolute.hotas" ||
            !api->declareAbsoluteHeadTrackingOwner || !api->cameraHookReleased ||
            !api->flightObserverInstalled || !api->cockpitSignalAgeMilliseconds) {
            return Result::Incompatible;
        }

        if (api->declareAbsoluteHeadTrackingOwner() !=
                AbsoluteCameraOwnershipApi::Result::Ok ||
            api->cameraHookReleased() == 0 || api->flightObserverInstalled() == 0) {
            return Result::Incompatible;
        }

        g_api.store(api, std::memory_order_release);
        RuntimePaths::Log(
            "[Compatibility]",
            "AbsoluteHOTAS released the camera seam; its selected-flight observer remains the suite context owner.",
            true);
        return Result::Connected;
    } catch (...) {
        return Result::Incompatible;
    }
}

bool Connected() noexcept
{
    return g_api.load(std::memory_order_acquire) != nullptr;
}

std::int64_t __cdecl CockpitSignalAgeMilliseconds() noexcept
{
    try {
        const auto* api = g_api.load(std::memory_order_acquire);
        return api && api->cockpitSignalAgeMilliseconds
            ? api->cockpitSignalAgeMilliseconds()
            : -1;
    } catch (...) {
        return -1;
    }
}

} // namespace HotasCameraCompatibility
