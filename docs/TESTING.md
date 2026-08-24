# Runtime Test Plan

The bootstrap compiles and its pure policy/math tests pass, but native hooks and
the OpenTrack-to-camera result still require in-game validation before the alpha
should be called a release candidate.

## Aim-restriction validation (supervised qualification pending)

The firing-cone feature is governed by
[AIM_RESTRICTION_DESIGN.md](AIM_RESTRICTION_DESIGN.md). Its release matrix must
cover camera-directed fire inside the ship-forward cone, stable cone-rim clamping
beyond it, admitted target-assisted fire inside the cone, immediate bounded-ray
fallback and HUD agreement when a target leaves it, boundary hysteresis, native
restoration, exact-build failure behavior, third-person negative controls, each
pilot-fired weapon class, missiles, turrets, and combined
AbsoluteZero/AbsoluteHOTAS operation. Probe observations alone do not qualify a
production hook. The v0.3.0-alpha implementation and pure policy coverage are
present, but none of the firing/HUD acceptance rows are marked qualified until
the supervised observations pass.

### First integration finding — 2026-08-23

The first deployed v0.3.0-alpha build suppressed primary direct-fire weapons
while missiles remained on their separate native path. Static comparison with
the accepted phase-22 capture identified an initial coordinate-frame error: the
camera hook quaternion is the local head/view rotation, not a world-space ship
orientation. A subsequent supervised run restored gun firing but exposed a
second frame error: the native base-angle resolver already supplies the
ship-orientation neutral basis, so removing the local head rotation from it
rotated ship-forward twice and made aim appear fixed. The revised build treats
that basis as ship space, composes local head-forward into it for the bounded
camera ray, and leaves completed native aim bytes untouched while inside the
cone. This revision is build/test verified but remains unqualified until the
next supervised gun-fire and target-assist smoke test.

The same run showed that the native ship-direction HUD coordinates do not
incorporate the head-only camera transform, as predicted by the phase-26 probe.
The base-reticle update projects the bounded ray through Starfield's active
camera matrix. A follow-up supervised run showed that writing
`shipDirectionCrosshair` steals the inner heading/VJoy puck even though the
projection itself is broadly correct. Inspection of the shipped
`ShipReticle.as` established that `HeadingReticle_mc` consumes that native pair,
while the separate `LeadCircle_mc` can consume `AimAssistCrosshair`.

The revised test build therefore leaves `shipDirectionCrosshair` entirely
native. Only while the head is outside the cone and no eligible target solution
exists, it projects the bounded ray through the independent aim-assist pair and
asserts that lead circle's display state. Inside the cone, and whenever a real
target solution is eligible, all three native HUD fields pass through. This is a
data-binding change using Bethesda's existing clips, not a replacement SWF.
The first decoupled test exposed a vertically mirrored lead-circle displacement;
the native writer at `0x155A2A4` publishes `1.0 - projectedY`, whereas X is
published directly. The bounded-ray projection now applies that same Y
conversion.

The plugin tracks ownership of the temporary lead-circle visibility. Returning
inside the cone, disabling tracking, or losing a fresh camera frame hides only
a marker that the plugin previously asserted. If the native update has already
produced an eligible target solution, ownership is relinquished without
touching native visibility. The double-ring `shipDirectionCrosshair` pair
remains unwritten on every path.

The detached lead circle and direct-fire rewrite now share one ship-local
bounded ray and one cone-rim history. The projectile path transforms that exact
ray through the native launch basis; it does not independently clamp a second
world-space reconstruction or select the earlier ship-center fallback. Thus an
untargeted shot outside the cone must follow the displayed rim marker.

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

Run each scenario on the supported Starfield 1.16.244 runtime. Version 1.16.242
is no longer advertised because the new gunnery/HUD seams have not been causally
qualified on that executable.

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
- Confirm sensitivity and independent minimum/maximum clamps at both signs.
- Configure pitch to `-20 / 0 / +90` degrees and verify downward motion stops at
  `-20` while upward motion reaches `+90` without shifting physical recenter.
- Configure a nonzero center on each axis and verify the recentered physical pose
  produces that output, deadzone remains centered on the physical pose, and both
  asymmetric endpoints remain absolute camera-angle clamps.
- In the standalone workbench, verify the side-profile pitch head, top-view yaw
  head, and roll horizon follow live output and that the center/limit ticks agree
  with their adjacent controls at narrow, default, and ultrawide window sizes.
- With every manual-look binding disabled, stop OpenTrack and verify camera
  authority clears after the stale timeout.
- Restart OpenTrack and verify tracking recovers without restarting Starfield.
- Recenter at non-neutral poses and verify no discontinuity remains.
- Toggle runtime tracking off and on; re-enable must capture a fresh center.
- Capture single-key and modifier-chord bindings for the menu, activation toggle,
  and recenter. Confirm capture itself does not trigger any runtime action.
- Through Absolute Control, capture a DirectInput button and POV for tracking toggle/recenter.
  Exercise Escape cancellation, Backspace clear, timeout, Apply, Discard, restart/read-back, and
  confirm capture never replays as an action. Confirm each saved control fires once per press.
- Bind all six keyboard trim directions. Confirm held input moves at the configured rate,
  release retains the reached angle, opposite directions cancel, asymmetric endpoint clamps hold,
  and both the Axes live surface and camera show the composite result.
- Confirm horizontal semantics explicitly: Look Left moves the camera left, Look Right moves it
  right, mouse-right looks right, and a conventional positive-right joystick X axis looks right
  before enabling the corresponding inversion toggle. Bank Left/Right and a conventional positive-
  right roll axis must likewise follow their labels before roll inversion.
- Select each mouse modifier in turn. Confirm unmodified mouse motion remains native; while held,
  raw horizontal/vertical motion trims the camera, does not steer the ship, respects both mouse
  inversions and sensitivity, and retains its result on release.
- Capture three Input Bus axes and six button/POV trim directions. Verify centered axes contribute
  exactly zero, deadzone/rescaling are continuous, endpoints map independently into asymmetric
  negative/positive ranges, per-axis inversion works, held digital directions use the shared trim
  rate, and disconnected devices retry without moving the view.
- Use the center push of a five-way hat as the existing recenter flight-control binding. Confirm it
  clears retained keyboard/mouse/hat trim, captures a fresh OpenTrack center when available, and
  still clears manual trim when OpenTrack is stale or absent. Repeat with the Axes-page manual
  recenter button while the menu is open.
- Stop OpenTrack with at least one manual source configured and verify manual-only view continues
  from each calibrated center. Clear every manual binding and disable the mouse modifier; camera
  authority must then clear after the tracker stale timeout.
- Outside the pilot seat and while Absolute Control is open/capturing, move every manual source and
  confirm no retained trim accumulates. Re-enter/close the menu and require no delayed jump.
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
  General, Axes, Aim Restriction, and Bindings. Exercise a forced `NotReady` first attempt and confirm the one
  post-post-data-load retry succeeds without installing the standalone renderer.

- With a host advertising `kLiveCapabilityHeadPose`, open Axes and confirm one pinned native
  HeadPose surface renders profile pitch, top-view yaw, and the roll horizon. Blue negative and
  green positive arcs must follow the current asymmetric draft, the yellow marker must follow shaped
  output, and raw/output text must advance only while OpenTrack frames are available.
- Drag every mirrored sensitivity/minimum/center/maximum slider and verify it updates the matching
  ordinary row, obeys `minimum <= center <= maximum`, participates in dirty state, rolls back on
  Cancel, and persists/read-backs on Apply. Repeat the ordinary rows with keyboard and controller.
- Remove the HeadPose capability and confirm all 18 ordinary Axes controls remain task-complete and
  no AHT gameplay or camera path changes.

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
- Load an INI containing only the legacy symmetric `fMaxYawDegrees`,
  `fMaxPitchDegrees`, and `fMaxRollDegrees` keys. Confirm it behaves symmetrically,
  then save and verify it contains explicit minimum/center/maximum keys.
- Delete the INI and confirm defaults are safe.
- Corrupt individual numeric and boolean values and confirm sanitization/defaults
  recover without creating profiles, migration files, or additional configs.
- Replace the INI with a newer packaged default and confirm no stale auxiliary
  config can override it.
