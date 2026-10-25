# Installed upgrade presentation

The product assembles its saucer from the existing approved modular UFO and
part GLBs. The development study continues to cook its upgraded assembly.
Product cooking explicitly selects the base GLB and excludes only the replaceable
antenna subtree; its spinning ring, six original rim lenses and fixed hull remain
unchanged. The client supplies the actual baseline beam emitter and one laser.

The authored top socket holds exactly one antenna or shield generator. Beam
optics occupy the existing underside socket. Laser Array fits two emitters at
the authored left/right sockets, then compact paired barrels about those same
mounts for three/four emitters, below the rotor. Parts and muzzle origins follow
the banked fixed hull; guns aim at their actual published targets. The outer
ring alone receives the global rotor animation.

## Functional visual mapping

| Installed system | Existing geometry / truthful activity |
| --- | --- |
| Laser Array | One through four mounted laser parts; actual emitter index and independently aimed shots |
| Rail Lens | Fine continuous piercing extension between the actual hit endpoints |
| Arc Coil | Two-segment blue arc between actual primary/secondary hit endpoints |
| Orbit Drones | Up to three small source saucers with laser parts, orbiting only in active time; each real drone shot starts at its own drone |
| EMP Ring | Expanding mint ground ring emitted by the actual pulse branch, carrying the exact damage radius; no invented radial laser spokes |
| Beam Splitter | Enlarged existing emitter housing and actual simultaneous capture cones; active rank-III capture burst lights the intake collar |
| Tractor Lens | Fixed intake collar; authoritative capture selection/progress retains its original radius/rules |
| Heavy Winch | Paired approved optics brace the existing intake; vehicle capture uses its real target and two-slot rules |
| Overtime Reactor | Intake power collar while actual capture beams are occupied |
| Reserve Battery | Intake power collar when actual beam slots are unused, counting vehicle beams as two |
| Shield Generator | Existing shield part replaces antenna; one camera-facing sphere rim exists only while charged |
| Repair Swarm | Small repair cross only after an actual positive hull/shield restoration, not every capture or cosmetic timer |
| Mirror Plating | Lavender shield rim while shield remains; actual reflection creates a return shot and response |
| Vector Thrusters | Short underside drive trails while moving; actual capture-triggered movement burst lengthens them |
| Phase Drive | Existing dash availability UI; a trail only from the committed dash event |
| Bounty Scanner | Ochre scan brackets on actual biological beam targets |

The six evolutions retain recognizable functional signatures: Great Vacuum has
six intake optics and the real six-beam capacity; Office Shredder widens actual
laser cores; Hostile Acquisition braces actual vehicle intake paths; Mirrorball
shows only its real stored-damage discharge/return rays; Employee Benefits uses
actual capture-triggered drone-burst duration and actual projectile interception;
Storm Drain expands the real pulse to its published wider radius with a second
cool ring. These are presentation changes, not additional rules or meshes.

## Portable outcomes and bounds

`ShotView.kind` distinguishes laser, rail, arc, drone, EMP, reflection and
Mirrorball; `emitter_index` identifies real gun/drone emitters. The existing
`secondary` field remains for compatibility. Additional recent outcome kinds
cover committed EMP pulse, reflection, interception, Mirrorball discharge, dash
and actual repair. Area outcomes carry their gameplay radius. Existing event
sequence, active timestamps, 64-event bound and 48-beat presentation cache remain.
No new RNG draws, damage arithmetic, targeting choices or persistent schema are
introduced. Beam/capture, warning, military projection and skeletal socket owners
are unchanged.

The new helper is stateless and bounded by published capacities: four hull guns,
three drones, one persistent charged shield, at most two segments per shot and
one/two rings per EMP outcome. Reduced motion fixes drone positions and their
shot origins, removes dash trails and leaves functional area/readability cues.
Pause/menu/background handling already freezes active clocks and sequence
consumption; reset clears the derived cache and all run equipment.

## Shield material boundary

The existing `fx-shield-shell/unit-sphere.glb` is consumed as a bounded mesh.
Its transparent GLB material is only a viewer fallback, so the product cook gives
its vertices unit coverage and the renderer applies the supplied rim equation.
Instance effect mode 2 selects that equation within the existing transparent,
depth-tested, no-depth-write pass. The orthographic camera matrix supplies the
view direction; rear-facing sphere fragments are discarded to avoid double
opacity. Effect mode 1 retains the corrected sRGB-authored palette; ordinary
opaque albedo, imported emission, lighting, shadows, MSAA and uniforms retain
their existing paths. No new pass, framebuffer, shader SDK or source artwork.

## Verification

`gear_presentation_contracts` checks starter truth, socket banking, replacement,
exact emitter/drone bounds, shield depletion, explicit pulse radius/pause/expiry,
reduced-motion shot alignment and real public-API earned drone/EMP activity.
Existing scene projection/capture-convergence tests now verify the physical
banked muzzle rather than the former ship-center ray origin. Product recipe
checks protect the explicit base/part selection. Native visual and mobile/profile
acceptance apply only to the evidence recorded with final source hashes.

### Verified checkpoint — 25 October 2026

- Optimized full `dev` build passes, followed by five focused suites: gear,
  existing capture/effects, projection, accessible metadata and portrait layout.
  `build/evidence/gear-final-build.log`, `gear-final-muzzle-build.log` and
  `gear-final-focused.log` preserve exact command output. The five source-recipe
  Python checks and `project.py verify --static-only` pass. The coordinator owns
  the later aggregate/device refresh; these focused results are not that gate.
- `build/captures/upgrade-identity/earned-progression.mp4` is an actual native
  Metal FIFO readback: 11,400 frames, 588×1280, 60 fps, 190.000 seconds, full
  decode and stable hashes across 255 source/content/executable inputs. Movie
  SHA-256: `6f14333e3561a8e93b4f821f971bc884427142c9caee536c9c147183eaa7c0d1`.
  It starts from a fresh account, uses seed 17 and picks the first actually earned
  card without forcing ranks, choices, health or unlocks. Title and each earned
  choice dwell for 45 QA-only render frames; ordinary active clocks freeze.
  Final active time is 183.4 seconds. `earned-route.json` records frame mappings;
  `title.png`, `starter.png`, `choice-1..8.png` and `gear-1..8.png` are extracted
  frames. Root reviewed and accepted the starter/late gear checkpoint.
- Final producer audit added the exact consumed graphics-GLB dependency to root
  CMake. The isolated real build-graph regression now changes that fixture source
  and proves first-invocation recooking, while retaining deleted-output recovery.
  It passes at `build/evidence/gear-build-graph-regression.log`. This dependency-only
  correction follows the movie/profile checkpoint and changes no runtime code.
- The independent performance owner produced source-stable, pixel-verified
  labelled synthetic starter/readable-gear/capacity stills in
  `build/evidence/visual-capacity-proof/`. The legal six-slot example uses rank-III
  splitter, tractor lens, laser array, orbit drones, shield generator and EMP,
  with Great Vacuum and Storm Drain. That controlled fixture proves presentation
  capacity/appearance, not a naturally earned maximum build or survival result.
- The native film shows a distinct starter, orbit craft, actual EMP ring, laser
  paths and charged shield silhouette. Main hull weapons remain small at the
  authored underside sockets; functional shot signatures carry much of their
  identity. No source art was replaced. No visible rotor intersection was found
  in the reviewed frames; this is not exhaustive geometric collision proof for
  every independently aimed pair.
- A focused build briefly encountered Blender `SIGSEGV` while stale-export
  detection attempted regeneration. The failure remains in
  `build/evidence/gear-tests-build.log`. After animation ownership/export freeze,
  normal regeneration/cooking and subsequent builds passed; no validation was
  bypassed. The first sandbox native launch also failed macOS GUI/XPC access
  before initialization. The authorized GUI-enabled launch produced the movie.
- The rim branch preserves existing effect mode 1 and ordinary opaque shader
  arithmetic by code inspection. The earlier accepted palette fixture predates
  this rim branch; no new claim of pixel-identical palette controls is made.
  A proposed additional palette launch was cancelled to preserve the independent
  timed profiling window. Physical-device appearance and final native shader
  package qualification remain separate.

## Continuation codec handoff

This slice adds **no authoritative state**. Do not serialize the public Snapshot
as a raw structure: its layout now contains additional transient presentation
fields and can change without changing gameplay authority.

| Field | Authority and restore policy |
| --- | --- |
| `Snapshot.modules`, `research`, `evolution_bits` | Existing authoritative installed run upgrades; preserve explicit named values. |
| `Snapshot.drone_burst_seconds` | Derived in `State::sync()` from existing `State::drone_burst`; serialize the private timer and rebuild the view. |
| `Snapshot.capture_burst_seconds` | Derived from existing `State::capture_burst`; preserve that timer. |
| `Snapshot.movement_burst_seconds` | Derived from existing `State::movement_burst`; preserve that timer. |
| `ShotView.kind`, `emitter_index`, `remaining_seconds` and all `view.shots` | Presentation-only flash geometry; damage was already committed. May be cleared on restore. |
| Presentation events, sequence and `SceneEffects` | Bounded reconstructible feedback; clear ring/cache and reset sequence together on restore to prevent stale sounds, impacts or pulse flashes. |
| `StyleInstance.shield_rim`, socket poses and orbit angle | Derived presentation; never persistent. Orbit uses restored active time, or fixed reduced-motion positions. |

The existing authoritative combat clocks still need their normal codec owners:
`laser_clock`, `drone_clock`, `pulse_clock`, `mirrorball_clock`, `reflected_damage`,
`capture_burst`, `vehicle_burst`, `movement_burst`, `drone_burst`,
`vehicle_capture_cooldown`, `disable_cooldown`, `intercept_cooldown`,
`time_since_hit`, `capture_shield_budget` and `shield_budget_clock`. Preserve actual
player dash cooldown/invulnerability and live projectile/actor/capture/focus/slow
state as gameplay authority. Resetting a rendered EMP ring is safe; resetting
its future pulse cooldown or a real interception cooldown changes gameplay.
Call the normal synchronization path after reconstructing authoritative fields.
No render state or presentation callback participates in damage/RNG decisions.

### Aggregate projection correction

The later root aggregate exposed an older `product_projection_contracts` check
that still required a ray midpoint halfway between ship centre and actor. The
runtime correctly starts the ray at its real aimed muzzle. A test-only correction
now checks the mounted gun against the authored banked hull socket, transforms
the actual gun muzzle and rendered ray endpoints, and verifies their common
actor endpoint and projected positions in both reduced-motion and banked poses.
The focused suite passes 1/1; production sources were not edited. Before/after
test hashes and focused evidence are in
`build/evidence/product-projection-test-correction.json`. This post-snapshot test
change does not rewrite the earlier film/profile source-stability manifests.
