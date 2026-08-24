# Head Tracking Aim-Restriction Design

Status: **implemented and supervised-runtime-qualified for v1.0.0-beta.1**.

This document defines the deliberately narrow gunnery behavior required by
off-axis cockpit head tracking. It is not a general targeting rewrite. A future
Absolute targeting/gunnery module may replace the implementation behind this
boundary, but it must preserve this contract unless the configuration explicitly
selects different behavior.

## Product rule

Head direction supplies aiming intent, but the ship defines the permitted arc:

- OpenTrack controls the first-person cockpit camera, up to the separately
  configured head-tracking limits.
- The firing cone is centered on the ship's forward axis, never the camera, the
  virtual joystick, or the current screen center.
- With no eligible target solution, pilot-fired forward weapons follow the
  camera-forward direction until it reaches the configured cone boundary.
- Beyond that boundary, the firing direction remains on the nearest point of the
  cone rim while the camera is free to continue turning.
- A selected target may receive the native target-linked firing solution only
  while that solution is inside the configured ship-forward cone.
- When the target solution leaves the cone, the next firing solution snaps to
  the bounded camera-directed solution. There is no blend toward the target or
  ship center.
- Looking ahead of a turn therefore directs fire naturally, while a 90- or
  180-degree head turn cannot drag fixed weapons beyond their permitted arc.

The default is bounded camera-directed aim. Fully camera-relative firing and
unrestricted native target assistance are available only through an explicit
opt-out from the cone limit.

## Configuration contract

The implementation adds one `Aim Restriction` section to Absolute Control and
the standalone editor, backed by the following INI values:

```ini
[AimRestriction]
bLimitAimToShipForwardCone = true
fFiringConeHalfAngleDegrees = 30.0
```

`fFiringConeHalfAngleDegrees` is intentionally a **half-angle**: `30` describes
a cone extending 30 degrees from ship-forward in every direction, or 60 degrees
across. The supported range is 1 to 90 degrees. The UI must label this as
`Half-angle` and may additionally show the full included angle.

Disabling `bLimitAimToShipForwardCone` makes the base firing ray fully
camera-relative and restores Starfield's native target-solution eligibility. It
is the explicit opt-in to unrestricted off-axis firing. Head-tracking yaw,
pitch, and roll limits remain independent of this setting.

The initial design uses a fixed one-degree spatial hysteresis band to prevent
boundary flicker:

- an ineligible solution becomes eligible below `half-angle - 1 degree`;
- an eligible solution remains eligible through `half-angle + 1 degree`.

There is no time delay. Invalid, non-finite, stale, or lost solutions become
ineligible immediately.

## Authoritative geometry

The base firing ray and target eligibility are evaluated in world space:

1. Obtain the normalized ship-forward vector.
2. Obtain the normalized camera-forward vector and its angular displacement
   from ship-forward.
3. If the camera is inside the cone, use camera-forward as the base firing ray.
   Otherwise, spherically clamp it to the closest direction on the cone rim.
4. Obtain the selected target solution's world-space aim point and relevant
   launch origin, then normalize `aim point - launch origin` as the candidate
   target direction.
5. Compare that target direction with ship-forward using
   `cos(fFiringConeHalfAngleDegrees)` plus the hysteresis state.
6. Require Starfield's native solution-validity conditions as well as the cone
   test. This feature may reject a native solution; it must not manufacture one.

The bounded base ray is:

```text
theta   = acos(clamp(dot(shipForward, cameraForward), -1, 1))
alpha   = min(theta, coneHalfAngle)
tangent = normalize(cameraForward - shipForward * cos(theta))
baseAim = shipForward * cos(alpha) + tangent * sin(alpha)
```

At the antiparallel singularity, the implementation retains the last finite
cone-rim azimuth reached through continuous head motion. Activation/recenter
initializes that history to ship-forward. The mouse accumulator, VJoy position,
and projected HUD coordinates are never ballistic inputs.

Evaluation is per native launch context because Starfield caches target aim
points per weapon. Automated turrets and autonomous weapon logic are excluded
from the first implementation and remain native. Direct-fire pilot weapon groups
must be validated individually; missile lock and guidance behavior must not be
changed accidentally.

## Single decision, two consumers

One immutable `AimDecision` snapshot is produced for a firing/HUD update. It
contains at least:

- mode: `Native`, `CameraDirected`, `ConeLimited`, or `TargetAssisted`;
- reason: disabled, no target, stale/invalid solution, outside cone, or eligible;
- target/solution generation;
- measured dot product or angle and configured half-angle; and
- freshness/build-validity flags.

That same decision controls both consumers:

1. **Ballistics** — write the bounded camera ray as the base launch direction,
   then reject the target-linked launch-angle overwrite outside the cone.
2. **HUD** — retain an eligible native target solution when one exists. Otherwise,
   while the camera is outside the cone, use the independent lead-circle channel
   to present the same bounded camera ray used by ballistics. The native
   ship-direction/heading channel remains owned by VJoy.

The HUD is presentation, not the source of ballistics. Writing either crosshair's
screen coordinates is not a permitted way to steer weapons. The HUD may use a
display-level target solution while the ballistic decision is per weapon; it
must never show aim assistance for a solution rejected by the shared cone policy.
When the bounded ray is behind or outside the camera frustum, its crosshair may
be off-screen or hidden; it must not be clamped to a screen edge in a way that
falsely represents the projectile direction.

## Runtime states

| State | Ballistic behavior | HUD behavior |
| --- | --- | --- |
| Tracking inactive | Completely native | Completely native |
| Tracking active, camera inside cone, no valid target | Camera-forward base ray | Base-aim reticle at camera center |
| Tracking active, camera outside cone, no valid target | Base ray clamped to nearest cone rim | Independent lead circle projects the clamped ray; native VJoy heading remains visible |
| Tracking active, valid solution inside cone | Permit native target overwrite | Native aim-assist crosshair visible |
| Tracking active, solution outside/lost/stale | Immediate return to bounded camera ray | Hide aim-assist in the same decision update |
| Cone limit explicitly disabled | Fully camera-relative base ray; native target eligibility | Native target-assist visibility |

Transitioning out of `TargetAssisted` is a snap, not a lerp. Re-entry may restore
the native target solution as soon as the spatial hysteresis admits it.

## Ownership and failure behavior

`CameraHook` remains the sole camera writer. A new narrow `AimRestriction`
service owns the bounded world-space base ray, target eligibility decision, and
exact-build gunnery/HUD hooks. It does not own target selection, aim prediction,
projectile simulation, weapon guidance, VJoy input, or ship rotation.

Restricted head tracking is an atomic safety feature:

- the camera hook may apply off-axis tracking only when every required
  aim-restriction hook for the current executable is positively validated;
- an unsupported build, unresolved hook, stale policy input, or irrecoverable
  runtime mismatch suspends head-tracking camera composition and leaves native
  camera and gunnery untouched;
- disabling tracking at runtime releases all gunnery/HUD policy and restores
  native behavior; and
- third-person, free camera, on-foot, menu, loading, and non-pilot states remain
  native.

This avoids the unsafe partial state in which the camera can turn off-axis while
weapons unexpectedly remain coupled to it. Exact addresses are build data, not
cross-DLL API, and no game object pointer crosses an Absolute suite boundary.

## Known native seams

Reverse engineering established the following contract on the qualified builds:

- `shipDirectionCrosshairX/Y` drives the native heading reticle nested in the
  VJoy display and must remain native so head tracking does not steal steering
  feedback;
- `AimAssistCrosshairX/Y` and `bShowAimAssist` drive a separate lead-circle clip.
  They present the selected native target solution when eligible and provide the
  bounded base-aim presentation only while the head is outside the cone and no
  eligible target solution exists;
- the target path retrieves a cached per-weapon world aim point, subtracts the
  launch origin, converts the direction to launch angles, and overwrites the
  neutral launch angles; and
- exterior neutral fire is ship-orientation based and independent of the camera.

The implementation must replace the resolved base launch direction after native
neutral angles exist, then gate the subsequent target-linked overwrite. It must
not replace upstream inputs that later native code corrects. The same decision
must project the base ray for the HUD and control aim-assist visibility.

The evidence and exact runtime/static paths are recorded in
`AbsoluteProbe/contracts/aim-path-phase-06-hud-ballistic-link.md`. Those findings
bound the implementation, but a probe address is not considered production-ready
until it is converted to an exact-build signature and passes causal in-game
validation.

## Scope boundary for a future gunnery module

The following belong in a separate targeting/gunnery module rather than this
head-tracking pass:

- new target selection or cycling rules;
- replacement lead prediction or convergence solving;
- target-relative weapon arcs, turret control, missile guidance, or weapon-class
  profiles;
- weapon-specific free-aim profiles or a user-authored world-space aim ray; and
- generalized aim-reticle replacement.

`AimRestriction` should therefore expose a small internal policy boundary rather
than embed a new targeting system in `HeadTracking` or `CameraHook`.

## Release acceptance criteria

The feature is not releasable until automated probes and supervised observation
demonstrate all of the following on every supported runtime:

1. With no target and the camera inside the cone, direct-fire projectiles follow
   camera-forward and the base-aim reticle remains at camera center.
2. With no target and the camera outside the cone, projectiles remain on the
   nearest cone-rim direction while the camera continues turning; the reticle
   projects that actual direction whenever it is in the frustum.
3. With a target solution inside the cone, firing uses the native target-linked
   solution even when the pilot is looking elsewhere.
4. Moving the target solution outside the cone makes the next shot use the
   bounded camera ray and hides aim assist without a one-frame disagreement.
5. Re-entry restores target assistance without oscillation at the boundary.
6. Disabling tracking or disabling the cone limit restores the documented
   native behavior.
7. Missing/incompatible gunnery hooks prevent off-axis camera activation rather
   than leaving a partial feature active.
8. Third-person and non-pilot firing remain unchanged.
9. Multiple direct-fire weapon groups, missiles, and automated turrets pass
   explicit regression checks.
10. AbsoluteZero and AbsoluteHOTAS retain their existing input, ship-rotation, and
   camera-ownership lanes.
