#pragma once

namespace WorkbenchHost {
enum class Selection {
    Active,
    Suppressed,
    Unavailable,
    Incompatible,
};

[[nodiscard]] constexpr bool ShouldInstallStandaloneRendererForSelection(
    Selection selection, bool standaloneEnabled, bool inactiveByHOTAS) noexcept {
    return standaloneEnabled && !inactiveByHOTAS && selection == Selection::Unavailable;
}

[[nodiscard]] Selection Select() noexcept;
[[nodiscard]] Selection CurrentSelection() noexcept;
[[nodiscard]] bool ShouldInstallStandaloneRenderer(bool standaloneEnabled) noexcept;
[[nodiscard]] bool IsMenuOpen() noexcept;
[[nodiscard]] bool IsInputCaptureActive() noexcept;
void HandleLegacyMenuShortcut() noexcept;
} // namespace WorkbenchHost
