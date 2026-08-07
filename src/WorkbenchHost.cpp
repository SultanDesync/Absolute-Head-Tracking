#include "PCH.h"

#include "WorkbenchHost.h"

#include "AbsoluteWorkbenchAPI.h"
#include "RuntimePaths.h"
#include "SuiteState.h"
#include "UIHook.h"
#include "Workbench.h"

namespace {
std::atomic<WorkbenchHost::Selection> g_selection{WorkbenchHost::Selection::Unavailable};
std::atomic<const AbsoluteWorkbenchApi::HostApiV1*> g_hostApi{nullptr};

bool ApiValid(const AbsoluteWorkbenchApi::HostApiV1* api) {
    constexpr auto requiredSize =
        offsetof(AbsoluteWorkbenchApi::HostApiV1, isInputCaptureActive) +
        sizeof(api->isInputCaptureActive);
    return api && api->structSize >= requiredSize &&
           api->abiVersion == AbsoluteWorkbenchApi::kAbiVersion && api->moduleId &&
           std::string_view(api->moduleId) == AbsoluteWorkbenchApi::kModuleId &&
           api->getHostMode && api->requestOpen && api->isOpen &&
           api->isInputCaptureActive;
}

WorkbenchHost::Selection Map(AbsoluteWorkbenchApi::HostMode mode) {
    switch (mode) {
    case AbsoluteWorkbenchApi::HostMode::Active:
        return WorkbenchHost::Selection::Active;
    case AbsoluteWorkbenchApi::HostMode::Suppressed:
        return WorkbenchHost::Selection::Suppressed;
    case AbsoluteWorkbenchApi::HostMode::Unavailable:
        return WorkbenchHost::Selection::Unavailable;
    case AbsoluteWorkbenchApi::HostMode::Incompatible:
        return WorkbenchHost::Selection::Incompatible;
    }
    return WorkbenchHost::Selection::Incompatible;
}
} // namespace

namespace WorkbenchHost {
Selection Select() noexcept {
    try {
        const HMODULE module = GetModuleHandleW(L"AbsoluteWorkbench.dll");
        if (!module) {
            g_hostApi.store(nullptr, std::memory_order_release);
            g_selection.store(Selection::Unavailable, std::memory_order_release);
            return Selection::Unavailable;
        }

        const auto address = GetProcAddress(module, "AbsoluteWorkbench_QueryHostApi");
        if (!address) {
            RuntimePaths::Log("[WorkbenchHost]",
                              "AbsoluteWorkbench.dll is loaded without the host query export; "
                              "suppressing the local renderer to avoid competing hooks.",
                              true);
            g_selection.store(Selection::Incompatible, std::memory_order_release);
            return Selection::Incompatible;
        }

        const auto query = reinterpret_cast<AbsoluteWorkbenchApi::QueryHostApi>(address);
        const auto* api = query(AbsoluteWorkbenchApi::kAbiVersion);
        if (!ApiValid(api)) {
            RuntimePaths::Log("[WorkbenchHost]",
                              "Absolute Workbench returned an incompatible host API; suppressing "
                              "the local renderer.",
                              true);
            g_selection.store(Selection::Incompatible, std::memory_order_release);
            return Selection::Incompatible;
        }

        g_hostApi.store(api, std::memory_order_release);
        const Selection selection = Map(api->getHostMode());
        g_selection.store(selection, std::memory_order_release);
        RuntimePaths::Log("[WorkbenchHost]",
                          selection == Selection::Active
                              ? "Unified Absolute Workbench selected as renderer owner."
                              : selection == Selection::Suppressed
                                    ? "Unified Workbench graphics bypass selected; local renderer "
                                      "suppressed."
                                    : selection == Selection::Unavailable
                                          ? "Workbench host unavailable; standalone renderer may "
                                            "start."
                                          : "Workbench host is incompatible; local renderer "
                                            "suppressed.",
                          selection == Selection::Incompatible);
        return selection;
    } catch (...) {
        g_selection.store(Selection::Incompatible, std::memory_order_release);
        return Selection::Incompatible;
    }
}

Selection CurrentSelection() noexcept {
    return g_selection.load(std::memory_order_acquire);
}

bool ShouldInstallStandaloneRenderer(bool standaloneEnabled) noexcept {
    return standaloneEnabled && CurrentSelection() == Selection::Unavailable &&
           !SuiteState::InactiveByHOTAS();
}

bool IsInputCaptureActive() noexcept {
    if (SuiteState::InputCaptureActive()) {
        return true;
    }
    if (CurrentSelection() == Selection::Active) {
        if (const auto* api = g_hostApi.load(std::memory_order_acquire)) {
            return api->isInputCaptureActive() != 0;
        }
    }
    return UIHook::IsUIOpen() && Workbench::IsCapturingHotkey();
}

void HandleLegacyMenuShortcut() noexcept {
    try {
        if (CurrentSelection() == Selection::Active) {
            if (const auto* api = g_hostApi.load(std::memory_order_acquire)) {
                api->requestOpen("absolute.head_tracking", "head-tracking");
            }
            return;
        }
        if (CurrentSelection() == Selection::Unavailable && UIHook::IsAvailable()) {
            UIHook::Toggle();
        }
    } catch (...) {
    }
}
} // namespace WorkbenchHost
