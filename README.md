# Absolute Head Tracking

Absolute Head Tracking is an independent SFSE plugin that brings Absolute's
OpenTrack-compatible rotational cockpit camera look to Starfield without the
HOTAS/HOSAS input stack, profiles, macros, or multi-file configuration system.

Version `0.2.0-alpha` adds optional shared configuration frontends. The gameplay
DLL remains authoritative and continues to work without either frontend.

## Absolute Control Panel integration (experimental)

This branch can register three native-menu pages with Absolute Control Panel:

- **General** — runtime, polling, response, diagnostics, and recenter controls;
- **Axes** — the complete scrollable yaw, pitch, and roll configuration surface; and
- **Bindings** — keyboard fallbacks plus optional Input Bus flight-control
  bindings for tracking toggle and recenter.

The adapter dynamically discovers the already-loaded host at SFSE data-load, after the native
menu host becomes ready, with one bounded retry at post-post-data-load. Discovery recognizes the
canonical host, the current ResearchDev host, and the earlier research artifact name.
It does not link against Absolute Control Panel, transfer configuration ownership, or make the
host a gameplay requirement. When the host advertises provider-owned capture and AbsoluteHOTAS
exports Input Bus ABI 1, the two action rows record DirectInput buttons/POV directions through
HOTAS's shared device service. Draft changes use the existing live configuration path, Apply uses
the existing save path, and Cancel restores the pre-edit snapshot. Host-owned binding capture
records single keyboard keys and Ctrl/Alt/Shift chords into the existing INI representation;
provider-owned controller capture stores stable device binding strings in the same file.

Frontend selection remains deliberately fail-optional during this experimental period:

1. use Absolute Control Panel when its compatible host accepts registration;
2. otherwise retain the existing Absolute Workbench integration; and
3. otherwise retain the standalone Dear ImGui tuning window when enabled.

The Control Panel is opened through its own PauseMenu entry or fallback hotkey. The existing
`Ctrl+Alt+H` binding remains the legacy Workbench/standalone shortcut.

## Absolute suite

- [Absolute Workbench](https://github.com/SultanDesync/Absolute-Workbench) — optional
  shared configuration UI for this plugin and the other daughter mods.
- [AbsoluteHOTAS](https://github.com/SultanDesync/AbsoluteHOTAS) — complete HOTAS/HOSAS
  flight-control stack with its own integrated head-tracking implementation.
- [AbsoluteZero Ship Control](https://github.com/SultanDesync/AbsoluteZero-Ship-Control) —
  lightweight mouse-alignment assistance that is compatible with this plugin.
- **Absolute Power** — priority-based ship-power presets and automation; its public
  repository is forthcoming.

## Compatibility contract

- **Compatible with AbsoluteZero Ship Control.** AbsoluteZero owns the flight
  writer block; this plugin does not hook or write that seam.
- **Compatible with the current AbsoluteHOTAS camera-ownership ABI.** Absolute
  Head Tracking declares sole ownership of camera composition after all SFSE
  plugins load. HOTAS parks its legacy tracker and camera hook while retaining
  its core selected-flight observer. Current builds publish authoritative pilot
  context and DirectInput edges through Absolute Input Bus ABI 1; this plugin
  installs no duplicate flight observer. Older ownership-compatible builds use
  the external bounded signal-age callback, while builds without the ownership
  ABI remain fail-closed.
- Runtime metadata is pinned to Starfield `1.16.242` and `1.16.244`, the builds
  on which these exact native offsets were validated.

## Features in the bootstrap

- OpenTrack `freetrack 2.0 Enhanced` shared-memory input.
- Rotational yaw, pitch, and roll with independent enable, inversion,
  sensitivity, and maximum-angle controls.
- Deadzone, frame-rate-independent smoothing, stale-frame rejection, recenter,
  and runtime toggle.
- Cockpit-only fail-closed camera injection through the validated
  `FirstPersonState` quaternion seam, using either the standalone local observer
  or HOTAS's authoritative Input Bus pilot context.
- A compact standalone Dear ImGui tuning window when neither shared frontend is active.
- A versioned C-compatible API and `AbsoluteHeadTracking.workbench.json`
  manifest. Absolute Workbench can edit, save, reload, and verify this plugin's
  configuration without sharing C++ objects or runtime ownership.
- Hotkey capture for the menu, tracking activation toggle, and
  recenter action, supporting single keys and Ctrl/Alt/Shift chords.
- Optional flight-control capture for tracking toggle and recenter through the
  Absolute Input Bus, with keyboard bindings retained as fallbacks.
- One easy-to-recreate configuration file: `AbsoluteHeadTracking.ini`.

## Default controls

- `Ctrl+Alt+H` — open the Head Tracking page in legacy Absolute Workbench, or toggle
  the standalone tuning window when no legacy host is active.
- `Ctrl+Alt+C` — recenter the current OpenTrack pose.
- `Ctrl+Alt+T` — toggle camera look at runtime.

The hotkeys can be changed in Absolute Control Panel, Absolute Workbench, the standalone tuning
window, or the INI. Click a displayed binding, then
press a single key or a chord such as `Ctrl+1`. Escape cancels capture and
Backspace clears a binding.

Renderer selection is deferred until all SFSE plugins have loaded. A compatible Absolute Control
Panel takes precedence without installing an overlay. If it is absent or incompatible, the
existing Workbench/standalone arbitration runs unchanged. A suppressed or incompatible shared
host never prevents the gameplay plugin from loading.

## OpenTrack setup

1. Choose any OpenTrack tracker input, including Tobii or NeuralNet webcam.
2. Select `freetrack 2.0 Enhanced` as the output protocol.
3. Start tracking, then launch Starfield through SFSE.
4. Enter the pilot seat. The first valid pose becomes center automatically.

Translation is intentionally not implemented in this bootstrap.

## Build

Install [xmake](https://xmake.io), then run:

```powershell
xmake f -m releasedbg
xmake
xmake test
```

The build stages an MO2-shaped layout under
`contrib/PluginRelease/Data/SFSE/Plugins`.
