#include "PCH.h"

#include "CameraHook.h"
#include "CameraPolicy.h"
#include "RuntimePaths.h"

namespace {
using FlightOutputFunction = void (*)(void*, const float*, float*);
using CameraRotationFunction = void (*)(void*, float*);

// Exact-build seams recovered and validated in AbsoluteHOTAS 5.0.1.
constexpr std::uintptr_t kFlightOutputSlotRva = 0x4C41BB0;
constexpr std::uintptr_t kFlightOutputRva = 0x12BA5E0;
constexpr std::uintptr_t kFirstPersonStateVtableRva = 0x4D04360;
constexpr std::size_t kCameraRotationSlot = 13;
constexpr std::uintptr_t kCameraRotationRva = 0x1E84A20;
constexpr std::uintptr_t kPlayerCameraSingletonRva = 0x61DD460;
constexpr std::int64_t kCockpitFreshMilliseconds = 400;

FlightOutputFunction g_flightOutputOriginal = nullptr;
CameraRotationFunction g_cameraRotationOriginal = nullptr;
std::atomic<bool> g_flightHookInstalled{ false };
std::atomic<bool> g_cameraHookInstalled{ false };
std::atomic<std::int64_t> g_lastFlightOutputMilliseconds{ 0 };
std::array<std::atomic<std::uint32_t>, 4> g_poseBits{
    std::bit_cast<std::uint32_t>(1.0F), std::bit_cast<std::uint32_t>(0.0F),
    std::bit_cast<std::uint32_t>(0.0F), std::bit_cast<std::uint32_t>(0.0F)
};
std::atomic<bool> g_poseActive{ false };

std::int64_t SteadyMilliseconds() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

std::int64_t FlightOutputAge() {
    const auto observed = g_lastFlightOutputMilliseconds.load(std::memory_order_acquire);
    const auto now = SteadyMilliseconds();
    if (observed <= 0 || now < observed) return -1;
    return now - observed;
}

#pragma warning(push)
#pragma warning(disable: 4733)
template <class T>
bool SafeRead(std::uintptr_t address, T& value) {
    if (!address) return false;
    __try {
        value = *reinterpret_cast<volatile T*>(address);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool SafeCopy(void* destination, const void* source, std::size_t size) {
    if (!destination || !source || size == 0) return false;
    __try {
        std::memcpy(destination, source, size);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}
#pragma warning(pop)

bool InstallPointerHook(std::uintptr_t* slot, std::uintptr_t expected,
                        std::uintptr_t detour, std::uintptr_t& original) {
    std::uintptr_t current = 0;
    if (!SafeRead(reinterpret_cast<std::uintptr_t>(slot), current) || current != expected)
        return false;
    DWORD oldProtection = 0;
    if (!VirtualProtect(slot, sizeof(*slot), PAGE_READWRITE, &oldProtection)) return false;
    original = current;
    InterlockedExchangePointer(reinterpret_cast<void* volatile*>(slot),
        reinterpret_cast<void*>(detour));
    FlushInstructionCache(GetCurrentProcess(), slot, sizeof(*slot));
    DWORD ignored = 0;
    VirtualProtect(slot, sizeof(*slot), oldProtection, &ignored);
    return true;
}

void RestorePointerHook(std::uintptr_t* slot, std::uintptr_t detour,
                        std::uintptr_t original) {
    if (!slot || !original) return;
    std::uintptr_t current = 0;
    if (!SafeRead(reinterpret_cast<std::uintptr_t>(slot), current) || current != detour) return;
    DWORD oldProtection = 0;
    if (!VirtualProtect(slot, sizeof(*slot), PAGE_READWRITE, &oldProtection)) return;
    InterlockedExchangePointer(reinterpret_cast<void* volatile*>(slot),
        reinterpret_cast<void*>(original));
    FlushInstructionCache(GetCurrentProcess(), slot, sizeof(*slot));
    DWORD ignored = 0;
    VirtualProtect(slot, sizeof(*slot), oldProtection, &ignored);
}

void HookedFlightOutput(void* handler, const float* input, float* output) {
    const auto original = g_flightOutputOriginal;
    if (!original) return;
    original(handler, input, output);
    if (handler && input && output) {
        g_lastFlightOutputMilliseconds.store(SteadyMilliseconds(), std::memory_order_release);
    }
}

void HookedCameraRotation(void* state, float* quaternion) {
    const auto original = g_cameraRotationOriginal;
    if (!original) return;
    original(state, quaternion);
    if (!state || !quaternion || !g_poseActive.load(std::memory_order_acquire)) return;
    if (!CameraPolicy::CockpitSignalFresh(
            FlightOutputAge(), kCockpitFreshMilliseconds)) return;

    const auto module = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    std::uintptr_t playerCamera = 0;
    std::uintptr_t currentState = 0;
    if (!module || !SafeRead(module + kPlayerCameraSingletonRva, playerCamera) ||
        !playerCamera || !SafeRead(playerCamera + 0x10, currentState) ||
        currentState != reinterpret_cast<std::uintptr_t>(state)) return;

    std::array<float, 4> native{};
    std::array<float, 4> head{};
    if (!SafeCopy(native.data(), quaternion, sizeof(native))) return;
    for (std::size_t index = 0; index < head.size(); ++index) {
        head[index] = std::bit_cast<float>(
            g_poseBits[index].load(std::memory_order_relaxed));
    }
    const std::array<float, 4> result{
        native[0] * head[0] - native[1] * head[1] - native[2] * head[2] - native[3] * head[3],
        native[0] * head[1] + native[1] * head[0] - native[2] * head[3] + native[3] * head[2],
        native[0] * head[2] + native[1] * head[3] + native[2] * head[0] - native[3] * head[1],
        native[0] * head[3] - native[1] * head[2] + native[2] * head[1] + native[3] * head[0],
    };
    SafeCopy(quaternion, result.data(), sizeof(result));
}
} // namespace

namespace CameraHook {

bool Install() {
    const auto module = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    if (!module) return false;

    auto* flightSlot = reinterpret_cast<std::uintptr_t*>(module + kFlightOutputSlotRva);
    std::uintptr_t flightOriginal = module + kFlightOutputRva;
    g_flightOutputOriginal = reinterpret_cast<FlightOutputFunction>(flightOriginal);
    const bool flightInstalled = InstallPointerHook(flightSlot, module + kFlightOutputRva,
        reinterpret_cast<std::uintptr_t>(&HookedFlightOutput), flightOriginal);
    if (!flightInstalled) {
        g_flightOutputOriginal = nullptr;
        RuntimePaths::Log("[CameraHook]",
            "Cockpit observer exact gate failed; camera injection remains disabled.", true);
        return false;
    }
    g_flightOutputOriginal = reinterpret_cast<FlightOutputFunction>(flightOriginal);
    g_flightHookInstalled.store(true, std::memory_order_release);

    auto* cameraSlot = reinterpret_cast<std::uintptr_t*>(module +
        kFirstPersonStateVtableRva + kCameraRotationSlot * sizeof(std::uintptr_t));
    std::uintptr_t cameraOriginal = module + kCameraRotationRva;
    g_cameraRotationOriginal = reinterpret_cast<CameraRotationFunction>(cameraOriginal);
    const bool cameraInstalled = InstallPointerHook(cameraSlot, module + kCameraRotationRva,
        reinterpret_cast<std::uintptr_t>(&HookedCameraRotation), cameraOriginal);
    if (!cameraInstalled) {
        g_cameraRotationOriginal = nullptr;
        RestorePointerHook(flightSlot, reinterpret_cast<std::uintptr_t>(&HookedFlightOutput),
            reinterpret_cast<std::uintptr_t>(g_flightOutputOriginal));
        g_flightHookInstalled.store(false, std::memory_order_release);
        RuntimePaths::Log("[CameraHook]",
            "FirstPersonState exact gate failed; all native hooks were rolled back.", true);
        return false;
    }
    g_cameraRotationOriginal = reinterpret_cast<CameraRotationFunction>(cameraOriginal);
    g_cameraHookInstalled.store(true, std::memory_order_release);
    RuntimePaths::Log("[CameraHook]",
        "Validated cockpit observer and FirstPersonState rotation hooks installed.");
    return true;
}

void Shutdown() {
    Clear();
    g_lastFlightOutputMilliseconds.store(0, std::memory_order_release);
    const auto module = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    if (!module) return;
    if (g_cameraHookInstalled.exchange(false, std::memory_order_acq_rel)) {
        auto* slot = reinterpret_cast<std::uintptr_t*>(module +
            kFirstPersonStateVtableRva + kCameraRotationSlot * sizeof(std::uintptr_t));
        RestorePointerHook(slot, reinterpret_cast<std::uintptr_t>(&HookedCameraRotation),
            reinterpret_cast<std::uintptr_t>(g_cameraRotationOriginal));
    }
    if (g_flightHookInstalled.exchange(false, std::memory_order_acq_rel)) {
        RestorePointerHook(reinterpret_cast<std::uintptr_t*>(module + kFlightOutputSlotRva),
            reinterpret_cast<std::uintptr_t>(&HookedFlightOutput),
            reinterpret_cast<std::uintptr_t>(g_flightOutputOriginal));
    }
}

void Publish(const HeadMath::Quaternion& pose, bool active) {
    const float norm = std::sqrt(pose.w * pose.w + pose.x * pose.x +
        pose.y * pose.y + pose.z * pose.z);
    if (!active || !std::isfinite(norm) || norm < 1e-6F) {
        Clear();
        return;
    }
    const std::array<float, 4> normalized{
        pose.w / norm, pose.x / norm, pose.y / norm, pose.z / norm
    };
    for (std::size_t index = 0; index < normalized.size(); ++index) {
        g_poseBits[index].store(std::bit_cast<std::uint32_t>(normalized[index]),
            std::memory_order_relaxed);
    }
    g_poseActive.store(true, std::memory_order_release);
}

void Clear() {
    g_poseActive.store(false, std::memory_order_release);
    constexpr std::array<float, 4> identity{ 1.0F, 0.0F, 0.0F, 0.0F };
    for (std::size_t index = 0; index < identity.size(); ++index) {
        g_poseBits[index].store(std::bit_cast<std::uint32_t>(identity[index]),
            std::memory_order_relaxed);
    }
}

Status GetStatus() {
    return {
        g_cameraHookInstalled.load(std::memory_order_acquire),
        g_flightHookInstalled.load(std::memory_order_acquire),
        g_poseActive.load(std::memory_order_acquire),
        FlightOutputAge(),
    };
}

} // namespace CameraHook
