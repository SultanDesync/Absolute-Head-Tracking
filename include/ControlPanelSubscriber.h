#pragma once

#include "AbsoluteControlPanelAPI.h"

namespace ControlPanelSubscriber {
[[nodiscard]] AbsoluteControlPanelApi::Result RegisterDiscoveredHost() noexcept;
[[nodiscard]] bool IsHosted() noexcept;
[[nodiscard]] bool IsMenuOpen() noexcept;
[[nodiscard]] bool IsInputCaptureActive() noexcept;
} // namespace ControlPanelSubscriber
