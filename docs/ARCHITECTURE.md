# Architecture

## Runtime boundary

The plugin is intentionally divided into six narrow services:

1. `OpenTrack` owns the FreeTrack 2.0 shared-memory mapping and stale-frame
   detection.
2. `HeadTracking` owns centering, tracker/manual-source composition, axis
   shaping, smoothing, hotkeys, and the polling thread. `ManualMouseInput`
   observes Starfield's raw relative mouse packets and routes only their motion
   fields away from flight steering while the configured look modifier is held.
3. `CameraHook` always owns the first-person camera rotation post-composition
   hook. Standalone, it also owns a read-only flight-output observer used as a
   fresh cockpit signal. With AbsoluteHOTAS, pilot context comes from the
   Absolute Input Bus and no duplicate observer hook is installed.
4. `Configuration` owns one synchronized `Config` value and one INI file.
5. `SuiteApi` exposes copied POD snapshots and commands. `WorkbenchHost`
   chooses either the optional suite host or the local `UIHook`/`Workbench`
   renderer. Renderer failure cannot disable manual INI configuration or alter
   the native camera.
6. `InputBusClient` optionally consumes copied DirectInput axes, held digital
   state, action edges, capture results, profile identity, and authoritative
   pilot context from AbsoluteHOTAS.
   Discovery is dynamic; no import library or device interface crosses the DLL
   boundary.

## Camera safety gate

A pose reaches Starfield only when all of the following are true:

- the exact-build camera hook and either the local observer or authoritative
  HOTAS Input Bus context are ready;
- either OpenTrack has a finite frame newer than the configured stale timeout
  or at least one manual-look source is configured;
- tracking and its runtime toggle are enabled;
- the flight-output observer ran within the last 400 ms;
- the hooked state is still the player camera's current state.

Any failed condition leaves Starfield's native quaternion untouched.

## Axis calibration model

Physical recentering and camera-angle calibration are deliberately separate.
The first valid OpenTrack pose, or a later recenter request, becomes the physical
tracker origin. After inversion and sensitivity, deadzone shaping produces a
signed displacement from that origin. The configured output center is added and
the result is clamped to independent minimum and maximum camera angles before
smoothing and quaternion composition. Therefore an axis range need not be
symmetric around zero, and a physically neutral head may intentionally produce
a nonzero camera angle.

Manual look joins after tracker shaping and before the final clamp. Held
keyboard or Input Bus digital directions integrate into a retained trim angle;
modifier-held mouse deltas integrate into the same yaw/pitch trim. Recenter
zeros that trim. A normalized Input Bus axis is not retained: after its own
normalized deadzone it maps `-1` from center to the configured minimum and `+1`
from center to the configured maximum. The tracker base, retained trim, and
proportional offset are clamped once and smoothed once. With no fresh tracker
frame, a configured manual source uses the calibrated axis center as its base.
Manual accumulation is suspended outside fresh cockpit context and while the
configuration host owns menu input.

Absolute Control renders this state through the optional, capability-gated
`HeadPose` live component: pitch is a side-profile head in a calibrated ring,
yaw is a top-view head in a calibrated ring, and roll is an artificial horizon.
Cyan indicates the host-owned wireframe;
the ring is split into blue negative travel and green positive travel around the
configured center, and a yellow pointer indicates shaped live output. Zero and
configured center use distinct host ticks, while raw and shaped values remain
numerically readable. The source sketch is preserved as
[`headtrackinguiconcept.png`](headtrackinguiconcept.png). AHT publishes copied
atomic telemetry plus ordinary control IDs; it never supplies drawing commands,
colors, ActionScript, or SWF assets. Mirrored sliders use the same provider-owned
draft and Apply/Cancel lane as the 18 ordinary Axes controls. Missing capability
falls back to those controls, and the deprecated Dear ImGui menu stays scalar-only.

## AbsoluteZero coexistence

AbsoluteZero hooks the flight-control writer block to shape mouse alignment.
Absolute Head Tracking never hooks or writes that block. Its cockpit observer is
the flight-handler vtable output method, and its only mutation is post-composing
the player camera quaternion. The two plugins therefore have no shared hook slot.
While manual mouse look is modifier-held, AHT zeros only the relative motion
fields in the raw packet after retaining their original deltas; buttons and
wheel data continue natively. This deliberately parks mouse ship steering,
including AbsoluteZero's downstream response, only for the duration of manual
mouse look.

## Locked aim-restriction boundary

Off-axis camera tracking requires a narrow `AimRestriction` service. It turns
camera-forward into a world-space base firing ray clamped by a ship-forward cone,
gates the native target-linked launch-angle overwrite to that same cone, and
synchronizes the base-aim/aim-assist HUD from one decision. `CameraHook` remains
the sole camera writer; `AimRestriction` does not select targets, calculate new
lead solutions, steer the ship, or derive ballistics from HUD coordinates.

Camera and restriction readiness are atomic. If the executable-specific gunnery
or HUD seams are unavailable, the plugin does not activate off-axis camera
tracking and leaves Starfield's native camera and weapons untouched. When
tracking is inactive, all gunnery behavior remains native. The full locked
contract and its release gates are in [AIM_RESTRICTION_DESIGN.md](AIM_RESTRICTION_DESIGN.md).

## AbsoluteHOTAS coexistence

Camera arbitration is deferred to SFSE `kPostPostLoad`, after daughter DLL load
order has settled. Absolute Head Tracking queries
`AbsoluteHOTAS_QueryCameraOwnershipApi(1)` and declares camera ownership. HOTAS
then parks its embedded tracker, skips or restores its legacy camera hook, and
continues owning the flight-output observer required by its flight runtime. The
Input Bus publishes copied runtime context, validity bits, and selected-output
age. Head Tracking requires authoritative `Piloting` context and still applies
its strict 400 ms camera freshness gate. The local flight observer is skipped.
The older camera-ownership signal-age callback remains an external compatibility
fallback and never re-enables local detection. No game object or address crosses
either DLL boundary. If declaration/release checks fail, this plugin installs no
gameplay hook.

## Workbench renderer provenance

The D3D12/DXGI overlay is ported from AbsoluteHOTAS's hardened workbench hook,
including cooperative chaining behind existing detours, Present and Present1,
creation-time swap-chain/queue association, ExecuteCommandLists fallback,
ResizeBuffers handling, frame fences, WndProc restoration, and fail-latched
teardown for bad renderer combinations.

## Optional Absolute Workbench host

`AbsoluteHeadTracking.workbench.json` is declarative discovery metadata. The
actual boundary is `AbsoluteHeadTracking_QueryApi(1)`, which returns a
size-versioned table of C-callable functions and trivially copyable structures.
Workbench never receives a pointer to `Config`, a mutex, or another daughter C++
object. Apply, save, reload, recenter, and runtime-enable requests remain inside
this DLL.

Renderer ownership is selected from the positive
`AbsoluteWorkbench_QueryHostApi(1)` handshake at SFSE `kPostPostLoad`:

- `Active`: Workbench owns renderer and suite menu input.
- `Suppressed`: graphics are intentionally disabled; AHT remains headless.
- `Incompatible`: a host DLL exists but cannot provide the agreed ABI; AHT
  remains headless to prevent a second overlay from racing it.
- `Unavailable`: the host DLL is absent; AHT may install its standalone renderer
  when enabled in its INI.

While Workbench is open or capturing a binding, AHT suppresses its local hotkey
edges. The legacy `Ctrl+Alt+H` action routes to the Head Tracking page when the
host is active.
