# Absolute Head Tracking v0.2.0-alpha

Absolute Head Tracking is an independent SFSE plugin for OpenTrack-compatible rotational cockpit
camera control in Starfield. It provides yaw, pitch, and roll tracking without requiring the HOTAS
flight stack, profiles, macros, or a virtual controller.

The current standalone release is stable for its supported rotational-tracking workflow and is the
authoritative head-tracking module in the modular Absolute suite.

## Features

- OpenTrack `freetrack 2.0 Enhanced` shared-memory input.
- Rotational yaw, pitch, and roll with independent enable, inversion, sensitivity, and limits.
- Deadzone, frame-rate-independent smoothing, stale-frame rejection, recenter, and runtime toggle.
- Cockpit-only camera injection through a validated first-person quaternion seam.
- Native Absolute Control pages for General settings, Axes, and Bindings.
- Keyboard binding capture for menu access, recenter, and tracking toggle.
- Optional DirectInput button/POV capture when AbsoluteHOTAS supplies the Absolute Input Bus.
- A standalone Dear ImGui tuning window and direct INI configuration when the shared menu is absent.
- Fail-closed camera ownership and runtime validation.

Translation is intentionally not implemented; this release provides rotational tracking only.

## Requirements

- Starfield `1.16.242` or `1.16.244`
- [SFSE](https://sfse.silverlock.org/) `0.2.20` or later
- [OpenTrack](https://github.com/opentrack/opentrack) with `freetrack 2.0 Enhanced` output
- Absolute Control is recommended for the native in-game editor, but is not a gameplay dependency.
- Address Library is **not** required by Absolute Head Tracking.

## Installation

Install the archive with MO2 or Vortex, or place these files manually:

```text
Data\SFSE\Plugins\AbsoluteHeadTracking.dll
Data\SFSE\Plugins\AbsoluteHeadTracking.ini
```

Back up `AbsoluteHeadTracking.ini` before updating if you have changed its settings.

## OpenTrack setup

1. Select your preferred OpenTrack input, including Tobii or NeuralNet webcam tracking.
2. Select `freetrack 2.0 Enhanced` as the output protocol.
3. Start tracking.
4. Launch Starfield through SFSE and enter the pilot seat.
5. The first valid pose becomes the center automatically; use the recenter binding whenever needed.

## Configuration

With Absolute Control installed, open the Pause Menu, select **MOD OPTIONS**, and choose
**Absolute Head Tracking**:

- **General** — enablement, polling, response, diagnostics, and recenter controls.
- **Axes** — yaw, pitch, and roll response settings in one scrollable page.
- **Bindings** — keyboard fallbacks and optional HOTAS/Input Bus bindings.

Absolute Head Tracking remains authoritative for validation, live state, persistence, Apply, and
Cancel. Absolute Control is only the shared presentation host. If it is absent or incompatible,
the plugin retains its standalone tuning window, legacy Workbench route, hotkeys, and INI.

## Default controls

- `Ctrl+Alt+H` — open the standalone/legacy tuning interface when Absolute Control is unavailable.
- `Ctrl+Alt+C` — recenter the current pose.
- `Ctrl+Alt+T` — toggle tracking at runtime.

Bindings can be changed in Absolute Control, the standalone tuning window, or
`AbsoluteHeadTracking.ini`. In binding capture, Escape cancels and Backspace clears a binding.

## Compatibility and ownership

- **AbsoluteHOTAS:** compatible through the camera-ownership ABI. When this standalone module is
  installed, it declares camera-composition ownership and HOTAS parks its embedded legacy tracker
  and camera hook. HOTAS continues to provide flight controls, pilot context, and—when available—the
  Input Bus used for DirectInput binding capture.
- **AbsoluteZero:** compatible. AbsoluteZero owns mouse pitch/yaw steering and idle centering;
  Head Tracking owns cockpit camera composition. They do not write the same seam.
- **Older incompatible HOTAS builds:** Head Tracking fails closed instead of allowing competing
  camera writers.

All optional host discovery is dynamic. A missing Absolute Control, HOTAS, Input Bus, or legacy
Workbench is a normal supported state and never prevents the gameplay plugin from loading.

## Absolute suite

- [Absolute Control](https://github.com/SultanDesync/Absolute-Control) — shared native PauseMenu
  configuration host.
- [AbsoluteHOTAS](https://www.nexusmods.com/starfield/mods/16668) — HOTAS/HOSAS flight control and
  optional Input Bus provider.
- [AbsoluteZero](https://www.nexusmods.com/starfield/mods/17460) — mouse alignment and locked-reticule
  steering.
- [Absolute Power](https://github.com/SultanDesync/Absolute-Power) — priority-based ship-power
  presets.
## Building

Install xmake, then run:

```powershell
xmake f -m releasedbg
xmake build AbsoluteHeadTracking
```

The release build stages an MO2-shaped layout under
`contrib/PluginRelease/Data/SFSE/Plugins`.
