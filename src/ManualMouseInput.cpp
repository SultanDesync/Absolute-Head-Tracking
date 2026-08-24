#include "PCH.h"

#include "ManualMouseInput.h"

#include "RuntimePaths.h"

#include <MinHook.h>

namespace {
using GetRawInputDataFunction = UINT(WINAPI*)(
    HRAWINPUT, UINT, LPVOID, PUINT, UINT);

GetRawInputDataFunction g_original{};
void* g_target{};
std::atomic<std::int64_t> g_horizontal{};
std::atomic<std::int64_t> g_vertical{};
std::atomic<bool> g_installed{};
std::atomic<bool> g_exclusive{};

UINT WINAPI HookedGetRawInputData(HRAWINPUT input, UINT command, LPVOID data,
    PUINT size, UINT headerSize) {
    const auto original = g_original;
    if (!original) return static_cast<UINT>(-1);
    const UINT result = original(input, command, data, size, headerSize);
    constexpr auto mousePacketSize = offsetof(RAWINPUT, data) + sizeof(RAWMOUSE);
    if (result == static_cast<UINT>(-1) || command != RID_INPUT || !data ||
        result < mousePacketSize) {
        return result;
    }

    auto* raw = static_cast<RAWINPUT*>(data);
    if (raw->header.dwType == RIM_TYPEMOUSE &&
        (raw->data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE) == 0) {
        g_horizontal.fetch_add(raw->data.mouse.lLastX,
            std::memory_order_relaxed);
        g_vertical.fetch_add(raw->data.mouse.lLastY,
            std::memory_order_relaxed);
        if (g_exclusive.load(std::memory_order_acquire)) {
            raw->data.mouse.lLastX = 0;
            raw->data.mouse.lLastY = 0;
        }
    }
    return result;
}
} // namespace

namespace ManualMouseInput {

bool Install() noexcept {
    if (g_installed.load(std::memory_order_acquire)) return true;
    const auto user32 = GetModuleHandleW(L"user32.dll");
    if (!user32) return false;
    g_target = reinterpret_cast<void*>(GetProcAddress(user32, "GetRawInputData"));
    if (!g_target) return false;

    const auto initialize = MH_Initialize();
    if (initialize != MH_OK && initialize != MH_ERROR_ALREADY_INITIALIZED)
        return false;
    if (MH_CreateHook(g_target, reinterpret_cast<void*>(&HookedGetRawInputData),
            reinterpret_cast<void**>(&g_original)) != MH_OK) {
        g_target = nullptr;
        g_original = nullptr;
        return false;
    }
    if (MH_EnableHook(g_target) != MH_OK) {
        MH_RemoveHook(g_target);
        g_target = nullptr;
        g_original = nullptr;
        return false;
    }
    g_installed.store(true, std::memory_order_release);
    RuntimePaths::Log("[ManualLook]",
        "Raw mouse-delta routing installed for modifier-held manual look.", true);
    return true;
}

bool Available() noexcept {
    return g_installed.load(std::memory_order_acquire);
}

void Shutdown() noexcept {
    if (!g_installed.exchange(false, std::memory_order_acq_rel)) return;
    if (g_target) {
        MH_DisableHook(g_target);
        MH_RemoveHook(g_target);
    }
    g_target = nullptr;
    g_original = nullptr;
    g_exclusive.store(false, std::memory_order_release);
    g_horizontal.store(0, std::memory_order_release);
    g_vertical.store(0, std::memory_order_release);
}

void SetExclusive(bool exclusive) noexcept {
    g_exclusive.store(exclusive, std::memory_order_release);
}

Deltas Consume() noexcept {
    return {
        g_horizontal.exchange(0, std::memory_order_acq_rel),
        g_vertical.exchange(0, std::memory_order_acq_rel),
    };
}

} // namespace ManualMouseInput
