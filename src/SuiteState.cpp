#include "SuiteState.h"

#include <atomic>

namespace {
std::atomic<bool> g_inactiveByHOTAS{};
std::atomic<bool> g_workbenchOpen{};
std::atomic<bool> g_inputCaptureActive{};
} // namespace

namespace SuiteState {
void SetInactiveByHOTAS(bool inactive) noexcept {
    g_inactiveByHOTAS.store(inactive, std::memory_order_release);
}

bool InactiveByHOTAS() noexcept {
    return g_inactiveByHOTAS.load(std::memory_order_acquire);
}

void SetWorkbenchSession(bool open, bool inputCaptureActive) noexcept {
    g_workbenchOpen.store(open, std::memory_order_release);
    g_inputCaptureActive.store(inputCaptureActive, std::memory_order_release);
}

bool WorkbenchOpen() noexcept {
    return g_workbenchOpen.load(std::memory_order_acquire);
}

bool InputCaptureActive() noexcept {
    return g_inputCaptureActive.load(std::memory_order_acquire);
}
} // namespace SuiteState
