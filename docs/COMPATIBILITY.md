# Compatibility

## Supported

- AbsoluteZero Ship Control (`AbsoluteZero.dll`)
- OpenTrack inputs that publish FreeTrack 2.0 Enhanced output
- Starfield runtimes 1.16.242 and 1.16.244

## Deliberately blocked

`AbsoluteHOTAS.dll` is blocked even if SFSE has not loaded it yet. Merely checking
`GetModuleHandle` would make compatibility depend on alphabetical plugin load
order, so startup also checks for the DLL beside this plugin. The process returns
success to SFSE but installs no hooks and records the reason in the log.

## Overlay compatibility

The workbench uses the same cooperative D3D12 hook strategy as AbsoluteHOTAS.
Steam, Discord, capture tools, RTSS, driver overlays, upscalers, and frame-
generation layers may already own public DXGI/D3D12 entry points. The hook detects
an initial jump and, where safe, chains behind its executable destination. If the
renderer encounters an unrecoverable pipeline transition, it disables only the
workbench for the remainder of the session and leaves head tracking and manual
configuration active.
