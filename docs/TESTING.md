# Runtime Test Plan

The bootstrap compiles and its pure policy/math tests pass, but native hooks and
the OpenTrack-to-camera result still require in-game validation before the alpha
should be called a release candidate.

## Baseline matrix

Run each scenario on Starfield 1.16.242 and 1.16.244 where practical.

1. Absolute Head Tracking alone.
2. Absolute Head Tracking plus AbsoluteZero Ship Control, with both plugin load
   orders observed in the SFSE log. Confirm mouse alignment and camera look work
   simultaneously and neither hook reports an exact-gate failure.
3. AbsoluteHOTAS installed beside the plugin. Confirm Absolute Head Tracking
   logs its compatibility refusal and installs neither native nor renderer hooks.
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
- Change the workbench binding, close it, and confirm only the new binding reopens it.
- Leave the pilot seat and verify injection stops within 400 ms.
- Walk on foot in first person for at least five minutes and confirm head pose is
  never applied. Repeat through menus, loading screens, photo mode, and dialogue.
- Re-enter the pilot seat and confirm the cockpit signal reopens automatically.

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

## Configuration

- Save from the workbench, restart, and confirm every setting round-trips.
- Delete the INI and confirm defaults are safe.
- Corrupt individual numeric and boolean values and confirm sanitization/defaults
  recover without creating profiles, migration files, or additional configs.
- Replace the INI with a newer packaged default and confirm no stale auxiliary
  config can override it.
