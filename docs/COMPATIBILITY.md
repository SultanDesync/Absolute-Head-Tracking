# Compatibility

## Supported

- AbsoluteZero Ship Control (`AbsoluteZero.dll`)
- AbsoluteHOTAS builds that expose camera-ownership ABI 1; current builds also
  expose Absolute Input Bus ABI 1 for pilot context and digital/analog controller bindings
- OpenTrack inputs that publish FreeTrack 2.0 Enhanced output
- Starfield runtime 1.16.244. The older 1.16.242 runtime is not advertised by
  v1.0.0-beta.1 because its gunnery/HUD seams have not been causally qualified.

## Fail-closed HOTAS arbitration

Absolute Head Tracking becomes the sole camera-composition owner only after HOTAS
explicitly releases its legacy camera seam. Current builds then consume HOTAS's
authoritative pilot classification through the Input Bus and install no local
flight observer. Older builds with the camera-ownership ABI use its external
signal-age callback instead. A HOTAS DLL without the ownership ABI remains
fail-closed: Head Tracking installs no competing gameplay hook and records the
reason in its log.

## Overlay compatibility

The workbench uses the same cooperative D3D12 hook strategy as AbsoluteHOTAS.
Steam, Discord, capture tools, RTSS, driver overlays, upscalers, and frame-
generation layers may already own public DXGI/D3D12 entry points. The hook detects
an initial jump and, where safe, chains behind its executable destination. If the
renderer encounters an unrecoverable pipeline transition, it disables only the
workbench for the remainder of the session and leaves head tracking and manual
configuration active.
