#pragma once

namespace SuiteState {
void SetInactiveByHOTAS(bool inactive) noexcept;
[[nodiscard]] bool InactiveByHOTAS() noexcept;

void SetWorkbenchSession(bool open, bool inputCaptureActive) noexcept;
[[nodiscard]] bool WorkbenchOpen() noexcept;
[[nodiscard]] bool InputCaptureActive() noexcept;
} // namespace SuiteState
