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
5. `UIHook` and `Workbench` own rendering and tuning. Renderer failure cannot
   disable manual INI configuration or alter the native camera.

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
