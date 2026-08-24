# Absolute Head Tracking v1.0.0-beta.1

Absolute Head Tracking is an independent SFSE plugin for OpenTrack-compatible rotational cockpit
camera control in Starfield. It provides yaw, pitch, and roll tracking without requiring the HOTAS
flight stack, profiles, macros, or a virtual controller.

The current standalone release is stable for its supported rotational-tracking workflow and is the
authoritative head-tracking module in the modular Absolute suite.

## Features

- OpenTrack `freetrack 2.0 Enhanced` shared-memory input.
- Rotational yaw, pitch, and roll with independent enable, inversion, sensitivity,
  negative/positive limits, and configurable neutral output angles.
- Deadzone, frame-rate-independent smoothing, stale-frame rejection, recenter, and runtime toggle.
- Cockpit-only camera injection through a validated first-person quaternion seam.
- Native Absolute Control pages for General settings, Axes, and Bindings.
- Retained manual-look trim from held keyboard directions, modifier-held raw mouse motion,
  digital hats/buttons, or proportional flight-control axes.
- Optional DirectInput axis/button/POV capture when AbsoluteHOTAS supplies the Absolute Input Bus.
- A standalone Dear ImGui tuning window and direct INI configuration when the shared menu is absent.
- Native Absolute Control live side-profile pitch, top-view yaw, and artificial-horizon roll
  calibration instruments with mirrored sliders.
- Fail-closed camera ownership and runtime validation.
- A ship-forward firing cone for bounded camera-directed aim and native target-assist eligibility.

Translation is intentionally not implemented; this release provides rotational tracking only.

## Aim restriction

Off-axis head tracking uses a configurable firing cone centered on the ship's forward axis.
With no eligible target, the base firing direction follows the camera until it reaches the cone
rim, then remains clamped there while the pilot keeps looking around. Starfield's native
target-linked solution is admitted only while it remains inside the same cone. The base-aim and
aim-assist HUD follow the same authoritative decision. This is a focused safety policy, not a
general targeting or gunnery rewrite. The v1.0.0-beta.1 implementation is feature-complete and has
passed its supervised aim, HUD, camera, and manual-input validation; broader public soak remains the
purpose of the beta.

The configuration semantics, ownership boundary, failure behavior, and validation requirements are
defined in [Head Tracking Aim-Restriction Design](docs/AIM_RESTRICTION_DESIGN.md).

## Requirements

- Starfield `1.16.244`
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

- **Axes** — the landing page, with yaw, pitch, roll, deadzone, and manual physical recenter controls.
- **General** — enablement, polling, smoothing, and diagnostics.
- **Aim Restriction** — ship-forward firing-cone enablement and half-angle.
- **Bindings** — keyboard fallbacks and optional HOTAS/Input Bus bindings.

Absolute Head Tracking remains authoritative for validation, live state, persistence, Apply, and
Cancel. Absolute Control is only the shared presentation host. If it is absent or incompatible,
the plugin retains its standalone tuning window, legacy Workbench route, hotkeys, and INI.

On hosts advertising the optional HeadPose capability, the Axes page becomes a live native
instrument. The sparse profile/top pilot-helmet lines, artificial horizon, asymmetric blue/green
arcs, yellow live marker, proportional red inner deadzone gate, center-aligned shared deadzone
slider, enable/invert toggles, readable tuning sliders, and manual recenter button are rendered by
Absolute Control's SWF. Those controls still use Absolute Head Tracking's normal transaction and
controller-focus path, but their redundant flat rows are hidden. Older hosts and the deprecated Dear
ImGui fallback retain the scalar controls without this visual surface.

Axis calibration separates physical recentering from camera bias. Recenter captures the current
tracker pose; each configured center angle is the camera output produced at that pose. Minimum and
maximum angles are independent, so pitch can use, for example, `-20` degrees down, `0` degrees at
neutral, and `+90` degrees up. Existing INIs containing the older `fMaxYawDegrees`,
`fMaxPitchDegrees`, and `fMaxRollDegrees` keys load as symmetric ranges and are rewritten using the
new minimum/center/maximum keys on the next save.

## Manual look and trim

The Bindings page can supplement OpenTrack or operate the rotational camera by itself:

- six keyboard bindings trim yaw, pitch, and roll at a configurable rate;
- Alt, Ctrl, or Shift can temporarily route raw mouse movement into retained yaw/pitch trim;
- six Absolute Input Bus button/POV bindings provide digital trim, including a four-way hat; and
- three captured Input Bus axes provide proportional yaw, pitch, and roll offsets.

Digital and mouse movement retain the angle reached after release. The existing keyboard or flight-
control recenter action clears that retained trim and captures the current OpenTrack pose when a
fresh tracker frame exists. A centered proportional axis contributes zero; its two ends map
independently into the configured negative and positive axis ranges. The controller deadzone and
per-axis inversions are configured beside those bindings. While the chosen mouse modifier is held,
relative mouse motion is routed to look instead of ship steering. Manual movement is ignored outside
fresh pilot context and while Absolute Control owns the menu or binding capture.
Starfield's composed yaw and roll are positive-left internally; AHT normalizes both at the manual-
input boundary so physically left/right and bank-left/right controls follow their labels before any
user inversion is applied.

## Default controls

- `Ctrl+Alt+H` — open the standalone/legacy tuning interface when Absolute Control is unavailable.
- `Ctrl+Alt+C` — recenter the current pose.
- `Ctrl+Alt+T` — toggle tracking at runtime.

Bindings can be changed in Absolute Control or `AbsoluteHeadTracking.ini`. The deprecated standalone
tuning window exposes only its legacy controls. In Absolute Control binding capture, Escape cancels
and Backspace clears a binding.

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
