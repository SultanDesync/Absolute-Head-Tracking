# Architecture

## Runtime boundary

The plugin is intentionally divided into five narrow services:

1. `OpenTrack` owns the FreeTrack 2.0 shared-memory mapping and stale-frame
   detection.
2. `HeadTracking` owns centering, axis shaping, smoothing, hotkeys, and the
   polling thread.
3. `CameraHook` owns two exact-gated vtable slots: a read-only flight-output
   observer used as a fresh cockpit signal, and the first-person camera rotation
   post-composition hook.
4. `Configuration` owns one synchronized `Config` value and one INI file.
5. `SuiteApi` exposes copied POD snapshots and commands. `WorkbenchHost`
   chooses either the optional suite host or the local `UIHook`/`Workbench`
   renderer. Renderer failure cannot disable manual INI configuration or alter
   the native camera.

## Camera safety gate

A pose reaches Starfield only when all of the following are true:

- both exact-build vtable hooks installed successfully;
- OpenTrack has a finite frame newer than the configured stale timeout;
- tracking and its runtime toggle are enabled;
- the flight-output observer ran within the last 400 ms;
- the hooked state is still the player camera's current state.

Any failed condition leaves Starfield's native quaternion untouched.

## AbsoluteZero coexistence

AbsoluteZero hooks the flight-control writer block to shape mouse alignment.
Absolute Head Tracking never hooks or writes that block. Its cockpit observer is
the flight-handler vtable output method, and its only mutation is post-composing
the player camera quaternion. The two plugins therefore have no shared hook slot.

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
