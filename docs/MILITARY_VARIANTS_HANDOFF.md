# Equipped infantry integration

Status: coherent source/art/cook freeze released to native QA, 25 October 2026.
Portable gameplay, authority revision two, equipped renderer/scene integration,
eight-pair startup preload and shipping content promotion are complete. The
three final authored sources passed independent skin and named-socket parity.
Normal-speed native encounter quality and refreshed mobile qualification remain
acceptance gates. The six legacy character recipes/exports remain untouched.

## Approved scope and boundaries

Keep `EntityKind::soldier` for capture, rewards, catalogue and population rules.
A typed `InfantryRole` identifies rifleman, shotgunner and rocketeer, selected
from the already consumed encounter heading sample without another RNG draw.
Keep actor cap 128, soldier cap 18, warning cap 8 and shared projectile cap 96.
The bounded projectile fan is 1/3/1; 18 shotgun volleys can contribute 54
projectiles within that shared cap. Do not change global hit immunity or damage.

| Role | Damage per projectile | Fan | Speed | Warning | Wait after shot |
| --- | ---: | ---: | ---: | ---: | ---: |
| Rifleman | 6 | 1 | 6 m/s | 0.9 s | 3.0 s |
| Shotgunner | 2 | 3 at 0 and ±0.12 radians | 6 m/s | 0.9 s | 3.0 s |
| Rocketeer | 6 | 1 | 4.5 m/s | 1.2 s | 2.7 s |

The existing wait plus warning makes a full attack cycle 3.9 seconds. Summed
volley damage stays six. Current 0.55-second immunity usually limits a shotgun
volley's actual damage to one pellet; this conservative initial reduction is
accepted. Spread warnings must show the fan; the launcher must visibly travel
more slowly. HP, spawn rates, capture duration, contact damage and rewards stay
with the existing soldier authority. Explicit spatial configuration owns the new
geometry/cadence values.

New body-only core measurements use the existing HRM cross-section helper.
Fixed Y=0.5 m falls between the taller bodies' legs, so inspect their measured
pelvis→pelvis+0.30 m lower-torso bands in 0.025 m steps. Maximum root-centred
inscribed cores are about 0.166/0.223/0.069 m. Accepted shared infantry core is
0.224 m, rounded upward from the widest body instead of retaining the obsolete
0.162 m legacy soldier value. Weapons remain excluded. This is a bounded body
core, not a full silhouette collider; deterministic passive separation stays
with the existing owner.

Infantry deliberately plants during committed warning and the 0.6-second
authored fire recovery. Recovery counts inside the existing cooldown, preserving
the full 3.9-second cycle. It then resumes ordinary stride-matched travel.
This is an explicit movement rule, avoiding sideways gait or an extreme waist
twist while a cross-body weapon aims. Root/body may face the measured weapon
bearing while planted; reviewed chest elevation preserves both grips and feet.
Capture/disable preempts firing. The earlier moving-overlay diagnostic remains
authoring evidence only; no general locomotion layer system is required.

Apply raw weapon geometry once through
`actor_world * posed_socket_weapon * inverse(rest_socket_weapon)`.
The fit is already baked into that delta. Muzzle axes are raw weapon +Z,
not the socket bone's long axis. Current source aim yaw is +45°/55°/40°;
body-root aim must account for this. Head and weapon sockets are separate typed
attachments; existing hat behavior must remain intact. Source muzzle height is
approximately 1.08/1.03/1.40 metres, below the UFO. Use the actual composed muzzle
path and reviewed upper-body elevation, rather than spawning a floating shot.
Typed enemy-shot outcomes start authored fire once; active-time pause freezes
all presentation transitions. No first-use animation I/O is permitted.

## Save compatibility

Keep account schema 2. Update the independently versioned Game authority
revision and numeric rules fingerprint when adding role/projectile fields.
Old Game continuations reject before field decoding. The valid old account
remains protected as `incompatible_continuation`; existing Preserve & Bank must
first verify an exact original-file backup, bank its recorded checkpoint credits
once and remove the incompatible continuation atomically. Do not reset progress,
resume with invented role fields or recalculate historical credits. New runs
must round-trip roles, projectile metadata, warning aims, draft/RNG and future
behavior exactly. See `adr/002-durable-run-continuations.md`.

## Prepared tooling and evidence

`tools/cook_infantry_animation.py` snapshots validated infantry inputs under the
existing lock, adapts report key names only in a private copy, and calls the
unchanged HRA1/HRM1 encoder. It never rebuilds or rewrites the six existing
exports. Hash checks reject stale or changing producer inputs. Cooked pairs and
provenance publish through the existing recoverable transaction. HRT1 skin and
HWS1 full-socket files are test evidence only, outside shipping content.

Seven `tools/test_cook_infantry_animation.py` checks pass, covering source escape,
role identity, stale exports, overlapping outputs, actual socket axes and failed
cook rollback, plus actual baked timing. Three isolated pairs have 42 clips and 3,468 source key/half-key
samples. The existing native skin/parity fixture passes with maximum position
error 0.60 micrometres and head-matrix error 0.36 micrometres.
`native/tests/military_animation_tests.cpp` adds named head/chest/palm/weapon/
muzzle reference checks; all pass with maximum matrix element error 1.20e-6.
The extra fixture initially used nominal 0.75-second run timing, whereas the
producer bakes 22/30 seconds. It now uses actual producer frame counts/duration;
the regression prevents a repeated fixture error. No runtime tolerance changed.
The final source freeze registers the focused infantry animation fixture in
root CMake and extracts chest elevation into the animation owner. The fixture
also verifies actual composed muzzle-axis intersection (cosine > 0.999999),
bit-exact lower-body transforms, both grip contacts within 2 mm, grounded skin,
source reaction playback, pause and cold-restored recovery. Optimized and
ASan/UBSan executions pass.

Eight independent portable military groups pass strict optimized and ASan/UBSan
with stable sources: role/RNG schedules, planting and cadence, preemption/caps,
24-direction source-muzzle geometry, live projectile/recovery continuation and
transactional malformed-state rejection. Original gameplay-save and account-
resume suites also pass with revision-two wire fixtures. The mobile owner independently tested actual preserved revision-one Android
account bytes: exact protected originals/backups, transactional failure, explicit
Bank, and cold reopen without reset or duplicate settlement pass. The interrupted
fixtures record zero checkpoint reward; settled wallet/result eight remains
eight. Evidence: `build/evidence/revision2-account-compatibility/compatibility-report.json`.

The approved portrait field is now 5.65 m half-width, with 13 m spawn-ahead and
11 m retirement-behind. Forward/back allowances stay ±6 m. The visibility owner
owns the 17 m camera and projected occlusion correction. Natural-run QA must
report narrower-lane and earlier-retirement effects; no blind difficulty retune
is included.

Preparation artifacts are under `build/military-variants-prep/`; all are ignored.
Body triangles are 4,990/5,763/5,505; weapon triangles are 1,348/1,537/1,729.
Three HRA files total 2,954,676 bytes. Source preservation and exact playback
tests do not establish final animation quality or phone performance.

## Capture cancellation continuity correction

Read-only review found that public Game range loss resets a surviving actor's
capture progress immediately, while the old scene transformed that directly
from intake scale/position back to the ground. The authored release clip did
not retain the previous world transform. Actual neutral-input seed 514 cancels
pig 8 from progress 0.941666 at 6.400 seconds to zero at 6.41667 seconds, with its
beam removed immediately. This authority behavior remains unchanged.

Product `SceneEffects` now keeps at most 128 reconstructible capture poses.
`scene` accepts a mutable effects owner because frame construction explicitly
records these poses. On a surviving positive-to-zero transition it retains the
last displayed root position, scale and tilt, accounting for the scrolling
runner origin. A 1.4-second cubic active-time return follows the current moving
authoritative ground root and regrows the body. The duration matches every
accepted `beam_release`; the world path is product-owned and does not duplicate
an authored lift-height or animation curve. Reacquisition blends from the
currently displayed pose over 0.12 seconds. Pause freezes it. Absent/retyped IDs
retire before replacement allocation; normal new/restore feedback reset clears
history. Disappearance never creates a release ghost or infers an outcome.

One narrow `ActorAnimation::product_capture_transport` intent declares this
ownership. For the three capture/release clips only, the renderer removes the
named source root's XYZ translation delta against rest, preserving all other
local translations, rotations and scales. The same recomposed pose feeds skin,
head and weapon sockets. Product removes the old suspended-root compensation.
A scoped 0.08-second source release crossfade preserves early partial-capture
joint continuity. The default raw playback/parity path and ordinary locomotion
are unchanged; Game rules, codecs and source assets are untouched. All body,
wheel and attachment construction and projected visibility use the one resolved
product root rather than independently smoothing attachments.

Nine rebuilt focused suites pass. Product checks include early/0.90/0.99
cancellation, the actual public-Game range-loss trace, moving ground/runner
origin, pause/reacquisition/reset, 128-to-128 history turnover and retyped IDs.
Composed tests on all eight exact shipping pairs verify root-only normalization,
source-relative joints and skin/head/weapon agreement within 1e-5. Twenty-four
60 Hz early/mid/mature cancellations have maximum first-frame relative-joint
vertex movement 0.060 m; the former source-root jump is absent. Every release
lands grounded. These composed tests also pass ASan/UBSan. Native visual proof
of the real discontinuity and the corrected release remains QA/root acceptance.
Evidence: `build/military-variants-prep/capture-continuity-freeze.json`.

## Consumed versus authoring assets

Before this slice, the recipe had 50 model IDs and six animated pairs, directly
using 43 of 56 inventory Tripo models. The frozen recipe now has 55 model IDs,
eight animation pairs, six material sources, two font faces and three locations.
It directly uses 48 of 56 inventory Tripo models; seven model IDs are approved
modular-saucer/effect derivatives. Three equipped bodies and three exact weapons
replace the legacy soldier body. Cleared minigun parts would make
57 model IDs and 50 of 56 Tripo models. Do not ship unused original source GLBs,
authoring scenes, references or validation fixtures to inflate that tally.

Accepted content still outside the frozen infantry core has explicit roles:

- Minigun head/mount: separate jeep threat identity after whole-jeep mounting
  and sweep clearance. Neutral head/yoke fit alone is insufficient. Seated crew
  requires an accepted authored seated rifleman clip and cannot delay infantry.
- Legacy soldier body: historical visual alternative after infantry replacement.
- Four original UFO body/emitter/mount models: alternatives to the accepted
  assembled modular UFO and its exact extracted parts; avoid duplicate craft.
- Round bush: alternate vegetation mass; current scenery uses the low bush.
- Twelve measured ground/path/road/pavement/fence modules: accepted modular
  construction kit, currently not consumed as GLBs. Native scenery presently
  owns procedural surfaces/fences; a deliberate prefab integration would need
  seam, repetition, composition and draw-cost review.
- Wood material: available for a future consumed timber surface; the other six
  material sources are used by the three locations.
- Seven UI graphics and the shield-shell geometry derivative are consumed.
  Remaining capture ring/crop-circle/dot, laser, hit-spark, exhaust, blob-shadow,
  threat-chevron and road-centre-dash graphics have clear feedback/marking roles;
  current equivalents use procedural geometry. Their authored graphics are not
  yet consumed, and should be assessed against current native readability.
- Two of six approved v3 font faces are consumed; the large logo, field lettering
  and other Beam weights remain alternatives for titles/signage/emphasis.

## Frozen runtime and verification handoff

Named head and primary-weapon attachment deltas use the same composed pose as
skin. A typed enemy-shot event starts the authored fire clip once. Each live
projectile has one bounded firing-time muzzle record; subsequent recoil cannot
move that record, and source death cannot remove it while the shot is live.
Only the initial XZ difference decays over 0.12 seconds; its height converges on
the committed 2.5 m target plane. New/restored runs clear launch and actor history.
Cold recovery uses its saved age without creating a new event or flash; an aged
projectile cannot replay its initial bridge. The 96-entry cache retires absent
IDs before admitting replacements, including a complete 96-to-96 frame change.

The optimized native player builds. Eight rebuilt focused CTest suites pass:
animation instances, world content, eight-pair preload/cache replacement,
military animation, gameplay saves, military gameplay, account resume, and scene
projection. Scene assertions cover all three source bodies/guns, one/three/one
warning lanes, real muzzle counts, typed fire, shared projectile/trail launch
identity and cold restore without duplicate flash. The isolated runtime build
graph still rebuilds deleted/changed outputs in one invocation. Infantry cook
boundary tests pass with the configured art Python (system Python lacks NumPy).
Final skin parity is 3,468 key/half-key samples over 42 clips: maximum position
error 0.596 micrometres. All named socket matrix elements differ by at most
1.20e-6. These numerical checks establish playback fidelity and bounded state;
QA/root still judge animation/readability in the actual native film.

Freeze inputs, cooked hashes and test evidence are recorded in
`build/military-variants-prep/integration-freeze.json`. The shipping allowlist has
66 HRM files (55 selected models plus 11 primitive/material/UI meshes), eight HRA
files and provenance/licence files; no HRT/HWS references, GLBs, authoring scenes
or legacy soldier pair ship. The staged runtime package has 98 files including
its other existing assets.

Next: independent native seed-514 normal-speed film and all-role fixtures,
bounded performance/count review, aggregate verification, then refreshed mobile
qualification. Natural capture/fire/fan coverage and the narrower field/earlier
retirement effects must be reported from actual play, not inferred from tests.
The jeep minigun and seated crew remain independent deferred content.
