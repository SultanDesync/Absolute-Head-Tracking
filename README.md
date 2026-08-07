# Absolute Head Tracking

Absolute Head Tracking is an independent SFSE plugin that brings Absolute's
OpenTrack-compatible rotational cockpit camera look to Starfield without the
HOTAS/HOSAS input stack, profiles, macros, or multi-file configuration system.

Version `0.2.0-alpha` adds the versioned POD configuration API and manifest used
by Absolute Workbench. The gameplay DLL remains authoritative and continues to
work without the optional shared frontend.

## Compatibility contract

- **Compatible with AbsoluteZero Ship Control.** AbsoluteZero owns the flight
  writer block; this plugin does not hook or write that seam.
- **Not compatible with AbsoluteHOTAS.** AbsoluteHOTAS already provides the same
  head-tracking feature and owns the same camera/flight-handler vtable seams.
  Absolute Head Tracking checks both the loaded module and the installed DLL and
  gracefully declines to install hooks when `AbsoluteHOTAS.dll` is present.
- Runtime metadata is pinned to Starfield `1.16.242` and `1.16.244`, the builds
  on which these exact native offsets were validated.

## Features in the bootstrap

- OpenTrack `freetrack 2.0 Enhanced` shared-memory input.
- Rotational yaw, pitch, and roll with independent enable, inversion,
  sensitivity, and maximum-angle controls.
- Deadzone, frame-rate-independent smoothing, stale-frame rejection, recenter,
  and runtime toggle.
- Cockpit-only fail-closed camera injection through the validated
  `FirstPersonState` quaternion seam.
- A compact standalone Dear ImGui tuning window when Absolute Workbench is not
  installed.
- A versioned C-compatible API and `AbsoluteHeadTracking.workbench.json`
  manifest. Absolute Workbench can edit, save, reload, and verify this plugin's
  configuration without sharing C++ objects or runtime ownership.
- Hotkey capture for the menu, tracking activation toggle, and
  recenter action, supporting single keys and Ctrl/Alt/Shift chords.
- One easy-to-recreate configuration file: `AbsoluteHeadTracking.ini`.

## Default controls

- `Ctrl+Alt+H` — open the Head Tracking page in Absolute Workbench, or toggle
  the standalone tuning window when Workbench is absent.
- `Ctrl+Alt+C` — recenter the current OpenTrack pose.
- `Ctrl+Alt+T` — toggle camera look at runtime.

The hotkeys can be changed in Absolute Workbench, the standalone tuning window,
or the INI. Click a displayed binding, then
press a single key or a chord such as `Ctrl+1`. Escape cancels capture and
Backspace clears a binding.

Renderer selection is deferred until all SFSE plugins have loaded. A compatible
active Absolute Workbench owns the single suite overlay; a suppressed or
incompatible host does not cause AHT to install a competing renderer. AHT uses
its standalone renderer only when the host DLL is genuinely unavailable.

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
