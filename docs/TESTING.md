# Runtime Test Plan

The bootstrap compiles and its pure policy/math tests pass, but native hooks and
the OpenTrack-to-camera result still require in-game validation before the alpha
should be called a release candidate.

## Qualified Control Panel checkpoint — 2026-08-17

The current `AbsoluteControlPanelResearchDev.dll` and Absolute Head Tracking integration
passed an in-game configuration smoke test on Starfield 1.16.244:

- the existing PauseMenu entry opened Absolute Control and Head Tracking populated its
  General, Axes, and Bindings pages;
- toggles, integer sliders, float sliders, the recenter action, and binding values rendered;
- changed values became dirty and Apply/save persisted them;
- Cancel/Discard restored the provider snapshot;
- the host stale-setting decision path completed correctly; and
- Head Tracking gameplay remained functional while the shared menu owned configuration UI.

This qualifies the shared-menu configuration slice and the repaired ResearchDev discovery/data-
load registration path. It does not replace the remaining camera, OpenTrack transport, renderer,
or compatibility matrix below.

## Qualified full-suite menu and camera-ownership checkpoint — 2026-08-17

The camera-ownership build passed its supervised combined-suite smoke test. Absolute Control
enumerated the full installed suite in one menu, and Absolute Head Tracking coexisted with the
current AbsoluteHOTAS runtime through the ownership ABI. This qualifies daughter-module discovery,
shared-menu rendering, and the Head Tracking/HOTAS camera boundary.

Known product gaps remain explicit:

- Head Tracking Input Bus action capture and execution are build-verified but still need the
  physical DirectInput runtime journey below.
- Absolute Power actions likewise cannot yet be bound to joystick controls.
- The current AbsoluteHOTAS pages are a proof of concept/technical demonstration, not the completed
  HOTAS configuration experience.

## Baseline matrix

Run each scenario on Starfield 1.16.242 and 1.16.244 where practical.

1. Absolute Head Tracking alone.
2. Absolute Head Tracking plus AbsoluteZero Ship Control, with both plugin load
   orders observed in the SFSE log. Confirm mouse alignment and camera look work
   simultaneously and neither hook reports an exact-gate failure.
3. AbsoluteHOTAS installed beside the plugin. Exercise both apparent DLL load
   orders and confirm Head Tracking owns the camera hook, HOTAS parks its legacy
   tracker/camera hook, and HOTAS retains the selected-flight observer. Confirm
   tracking and HOTAS flight injection both work in the same cockpit session.
   Repeat with AbsoluteZero installed and confirm all three ownership lanes work.
4. OpenTrack started before Starfield, after entering the main menu, and after
   entering the pilot seat.

## Camera and transport

- Confirm initial center capture is neutral.
- Confirm yaw, pitch, and roll signs, then test each inversion independently.
- Confirm sensitivity and maximum-angle clamps at both signs.
- Stop OpenTrack and verify camera authority clears after the stale timeout.
- Restart OpenTrack and verify tracking recovers without restarting Starfield.
- Recenter at non-neutral poses and verify no discontinuity remains.
- Toggle runtime tracking off and on; re-enable must capture a fresh center.
- Capture single-key and modifier-chord bindings for the menu, activation toggle,
  and recenter. Confirm capture itself does not trigger any runtime action.
- Through Absolute Control, capture a DirectInput button and POV for tracking toggle/recenter.
  Exercise Escape cancellation, Backspace clear, timeout, Apply, Discard, restart/read-back, and
  confirm capture never replays as an action. Confirm each saved control fires once per press.
- Change the workbench binding, close it, and confirm only the new binding reopens it.
- Leave the pilot seat and verify injection stops within 400 ms.
- Walk on foot in first person for at least five minutes and confirm head pose is
  never applied. Repeat through menus, loading screens, photo mode, and dialogue.
- Re-enter the pilot seat and confirm the cockpit signal reopens automatically.

## AbsoluteHOTAS camera ownership

- In HOTAS diagnostics, confirm suite coordination reports Absolute Head
  Tracking as camera owner.
- In the Head Tracking suite API/UI, confirm both Camera hook and Cockpit
  observer report ready; the observer is borrowed from HOTAS in this mode.
- Confirm the HOTAS log reports that its legacy tracker is parked and its
  FirstPersonState hook is skipped or released.
- Confirm the Head Tracking log reports a successful camera-ownership ABI
  connection and Input Bus pilot-context selection before installing its
  FirstPersonState hook. Confirm it reports that the local observer was skipped.
- With an older/incompatible HOTAS DLL, confirm Head Tracking installs no native
  hook and reports inactive/fail-closed rather than racing the camera slot.

## Workbench renderer

The renderer itself comes from the validated AbsoluteHOTAS implementation, but
the new DLL still needs integration coverage:

- Borderless and exclusive/fullscreen transitions.
- Alt-tab while open and closed.
- Resolution changes and swap-chain recreation.
- Present and Present1 paths where available.
- Steam and Discord overlays.
- RTSS/MSI Afterburner.
- NVIDIA and AMD driver overlays.
- DLSS-G, FSR frame generation, AFMF, and Smooth Motion combinations available
  on the test machine.
- Open/close from the hotkey before and after the first renderer initialization.
- Simulated renderer failure must close only the workbench; head tracking and
  manual INI configuration must continue.

## Shared Workbench integration

- With `AbsoluteControlPanelResearchDev.dll`, confirm AHT registers at data-load and populates
  General, Axes, and Bindings. Exercise a forced `NotReady` first attempt and confirm the one
  post-post-data-load retry succeeds without installing the standalone renderer.

- Load AHT before and after Absolute Workbench; the positive handshake must
  select exactly one renderer in either order.
- Exercise host modes Active, Suppressed, Incompatible, and Unavailable.
- Verify the legacy menu chord opens `absolute.head_tracking/head-tracking` when
  hosted and never opens the local renderer.
- Capture each binding in the shared frontend and confirm no daughter action
  fires during capture.
- Save from Workbench and require apply, INI write, reload, and read-back match
  before the draft becomes clean. Force a write failure and keep it dirty.

## Configuration

- Save from the workbench, restart, and confirm every setting round-trips.
- Delete the INI and confirm defaults are safe.
- Corrupt individual numeric and boolean values and confirm sanitization/defaults
  recover without creating profiles, migration files, or additional configs.
- Replace the INI with a newer packaged default and confirm no stale auxiliary
  config can override it.
