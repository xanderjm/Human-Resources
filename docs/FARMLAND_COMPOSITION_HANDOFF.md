# Farmland composition handoff — 25 October 2026

Status: **one approved overhang revision implemented; targeted native acceptance pending**.
The first17m native proof rejected full-crown clearance: it left crown slivers
and mostly invisible understory. Root approved separating lower-root safety from
tall overhang, now that shared projected protection covers the ship, live threats
and committed warning fans. Scenery sources are frozen for one targeted native
comparison; no independent recook or render was performed here.

## Review basis

Compare `build/captures/combat-presentation/final-capture-action.png` and
`final-attack-01.png` with
`art/style-frames/balanced-locations/01-sage-farmland-balanced.png`.
`final-attack-03.png`, `upgrade-identity/starter.png` and `gear-8.png` confirm that
the problem persists across positions and equipment. Earlier Suburbia/Desert
normal frames are structural context, not final shader/UI evidence.

Three material differences remain: uniform parallel edge walls, small actor
silhouettes, and scenery overlapping target silhouettes. The mint/coral colour
correction is accepted independently. This pass addresses edge grouping and
positions; the product/gameplay owners retain actor scale and fair visibility.

Original Farmland had seven or eight five-crown groups per64m sector. The first
six-group pass reduced that wall rhythm but its full-crown13m constraint left
too little scenery visible at17m. See the actual rejected native proofs under
`build/evidence/infantry-spatial-proof-corrected/`:
`synthetic-composition-station-0.png` and `synthetic-composition-station-33_6.png`.
The revision keeps the six unequal groups, stations, counts and landmark gaps;
it changes crown/root constraints and existing understory positions only.

## Spatial decision and ownership

- The coordinator superseded the initial fixed-20.5m/±8.25m plan after exact
  visibility review. Root approved field half-width5.65m and portrait camera17m,
  with forward/backward±6m retained, enemy spawn-ahead13m and retire-behind11m.
  The spatial owner measured full gear, height/bank, all three themes and phone
  extremes before approval. Scenery consumes that owner's contract; it does not
  change gameplay or zoom independently. Actual model scale, initial speeds,
  HP and economy remain unchanged.
- Keep targeting/encounter rules, materials, lighting and shader/pass state.
- Keep source meshes/textures, their asset approvals and the existing road.
- This pass changed `scenery_farmland.cpp` and its
  focused `scenery_tests.cpp` checks. No shared app, gameplay, renderer or CMake
  edits. Preserve sector-based stable identity and query-independent placement.
- For the measured candidate, intended opaque clearance is the central 13m:
  5.65m outer player centre plus0.85m radius reaches6.50m. The final approved
  envelope wins. Tall overhangs additionally require the separately
  owned projected occlusion rule for player, live threats and committed warning
  fans. This dependency is the implemented shared all-tall-prop rule in the
  product visibility owner, not the former tree-only, player-only heuristic.

The old13m decorative clearing did not cover the old playable field. Merely
passing its scenery test never proved fair visibility. The old camera also let
orbit drones leave view at extremes. The spatial owner's measured approval
covers the candidate's full-gear geometry, bank and phone extremes; combined
native play still needs visibility/telegraph judgement. Scenery does not enlarge
models.

## One placement change

Use three local profiles. Counts below are per group; the setback is the minimum
*full rotated crown* inner X edge. Roots and opaque low props retain the separate
13m ground clearance; tall crowns may project into that band with shared fading.

| Profile | Crowns | Minimum crown edge | Longitudinal offsets | Understory groups |
| --- | --- | --- | --- | --- |
| Pocket P | 2 large + 1 small | 6.8m | -2.5, 0, +2.5m | 1 at centre |
| Shoulder S | 4 large + 1 small | 5.7m | -4.5, -1.5, 0, +2, +4.5m | 2 at ±2.5m |
| Dominant D | 5 large + 2 small | 4.8m | -6, -2, 0, +2, +5, -5, +6m | 3 at -4, 0, +4m |

Retain the current scale/yaw vocabulary: large scales at most 1.9, small scales
at most the existing small-tree maximum; yaw from 0.3, 0.8, 1.4, 2.2, 3.1 radians.
For successive crowns use outward depth offsets `[1.2,0,2.8,.8,1.3,2.8,2.5]m`,
taking the prefix required by the profile. Derive each root from the measured
rotated crown extent plus its profile edge/depth. Also clamp the actual lower
trunk/root footprint outside6.55m. Road correction cannot violate either minimum.
These implemented poses await the single targeted native silhouette comparison.

The measured source Y=0..0.6m band includes triangle edges clipped at the height
plane, not only vertices below it. Current grounded trees have uniform scale>=1
and yaw only; that source band conservatively covers world Y=0..0.6m. Large-tree
XZ half extents are0.635616/0.604305m; small-tree0.861215/0.609069m. Production
rounds outwards to0.64/0.61 and0.87/0.62; independent assertions use separately
rounded measured values0.636/0.605 and0.862/0.610. Mesh changes, pitch/roll or
sub-unit scaling require remeasuring this contract. Hash-bound triangle-clipped
measurements are in `build/evidence/farmland-overhang/lower-envelope-measurement.json`.

Existing understory shapes/scales and six parts per call are retained. Group
centres move to7.65m/7.80m/7.95m for D/S/P, with mirrored outward offsets at
0.95m spread. Each bush/rock/grass mesh independently clamps its entire rotated
footprint outside6.55m and clear of the trail over its ground span. The farmyard
understory uses7.95m too. This replaces an offscreen cluster-centre proxy with
actual per-prop placement; it does not add dressing.

Use six groups per sector: one P, S and D on each side. Stations below are
relative to the absolute 64m sector centre, not the moving view.

| Composition | Left groups, station/profile | Right groups, station/profile | Landmark openings |
| --- | --- | --- | --- |
| 0 | +29 P; -4 D; -27 S | +30 P; +6 S; -27 D | Barn +19; crops +20; silo -16; windmill -13 |
| 1 | +29 S; +8 P; -27 D | +24 S; -3 D; -31 P | Barn -11; crops -21; silo +18; windmill +11 |
| 2 | +28 P; -7 D; -29 S | +25 S; +4 P; -28 D | No barn; left crops +8; silo -18; windmill -13 |

Keep the barn/crops as the large paired landmark opening, stagger the middle
crown masses by about ten metres, and reserve the silo/windmill gaps. Do not
put a second duplicate crop plot into a gap. Reallocate the existing five loose
bales into the landmark/understory groups rather than spacing them down a line:
two at `(x=+7.3, s=windmill±2)`; one at `(x=crop_side×7.3, s=crops-4.7)`;
one at `(-7.3,silo)`; one at `(-7.3,left_dominant-4)`. Keep existing bale
scales/yaws and apply road/rotated-footprint checks. The two existing barn bales
remain part of the farmyard, not five additional replacements.

Retain at most two short fence runs per side. Anchor ends to the group/landmark
edge and end them beneath an understory group; omit rails through landmark
aprons. On the left, use outward diagonals from `x=-7.20` to `x=-10.30` over 4–6m
of station rather than another full-length parallel edge. On the right, preserve
the trail's inner-side clearance; omit a run where a ground-safe interval
does not fit. Keep the current post/rail thickness and materials. No new fence
mesh or extra return runs.

The candidate left run stations are `[left_dominant-7,left_dominant-3]` and
`[barn-10,barn-6]`; without a barn use `[crops+7,crops+11]` for the second.
Right runs use `[windmill-8,windmill-4]` and `[crops+4,crops+9]`. These use the
table's absolute-sector landmark anchors. The existing road and ground-envelope
rules can shorten/omit a run; they cannot generate extra compensating runs.

Apply the final ground-safe envelope to low props/landmarks individually,
including understory offsets. Preserve their current lateral landmark anchors:
barn-9.4m, silo-9.5m, windmill+8.5m; retain the existing road-clear crop anchor and
5×10 arrangement. Their measured full rotated bounds clear6.5m at those poses;
validate actual understory/bale rotations too. The earlier9.1m outward-building
proposal is withdrawn: it assumed the old wide movement field and would push
landmarks out of the approved candidate view. No compensating props or plots.

Opaque roots/low props must not violate6.5m merely to imitate an inward mass.
The separately bounded tall overhang is permitted by root's explicit revision
decision. If the targeted native comparison fails, report the residual spatial
gap instead of making another texture/prop iteration.

## Budget and one acceptance checkpoint

Implemented30 crowns/sector versus35–40 before;12 grove-understory calls versus14–16
(retain the existing farmyard call); at most 80 fence boxes versus roughly 114
before apron omissions. Existing other prop counts do not grow. New positions
must pass the existing1,728-window suite and retain the frozen417 prop maximum
and4,608 surface vertices (the original577 gate also remains). Measure visible triangles and
shadow draws too: fewer sector props alone does not prove lower visible cost.

After the runtime milestone, the coordinator will capture **seed514 at station0 and adjacent
station33.6m**, at the same portrait aspect and the final approved camera. Use
actual native scenery/presentation and renderer; do not compare a stale20.5m
preview with a new narrower playable. The viewer's `--time 0` and `--time 14`
reproduce those stations (2.4m/s) only if its camera has been explicitly aligned
by the spatial owner, otherwise use the shared native comparison fixture. This
is a native scenery comparison, not a substitute for gameplay.
Show original reference/before/after at equal
displayed width with aspect preserved; never stretch frames or accept a proxy
diagram. Record executable, source and cooked-content hashes.

Independent QA will combine the two comparison stations with one short
role-transition/cancellation proof, not repeat the190-second film or mobile build.
The product owner must
check outer X movement, upgraded shield/orbit gear, live threats and committed
warnings across phone aspects. Reuse this checkpoint; no separate texture,
lighting or repeated detail loop is authorized by this plan.

Accept only if the two native windows show distinct unequal masses, visible
landmark openings and fewer parallel strips without new opaque field intrusion,
scroll pops or cost growth. Otherwise report the remaining structural/spatial
gap and stop for the coordinator's decision. This is not whole-game/reference
acceptance or physical iPhone qualification.

## Focused implementation verification

The optimized macOS scenery target compiles with strict warnings. All1,728
existing seeded windows pass, with max417 generated props versus577 before,
unchanged4,608 surface vertices and max54 streamed crowns. Another196 focused
queries cover the six-group budget and independent full-crown opening checks at
stations0/33.6/64/128 across seeds0..47 and514. Rotated opaque footprint checks cover fences,
buildings, low vegetation, crops and bales throughout the1,728-window sweep;
the full-Z landmark, road, scrolling identity and UV continuity checks remain.

The old full-crown13m assertion was explicitly split after native rejection:
lower-root band remains>=6.49m; full crowns remain>=4.79m; tests require real
overhang to exercise the new contract and enforce grounded/unpitched uniform
scales>=1 for the band measurement. Low-prop/fence/building assertions retain
their original13m role unchanged. Minimum sampled lower-root edge is6.72093m;
minimum full-crown edge4.81528m. Understory centres inside the conservative17m
fringe box number29/35 at stations0/33.6. These are availability counts, not proof
of projected layering or absence of occlusion; the targeted native proof owns that.

The first-pass source/test/result remain in `build/evidence/farmland-composition/`.
Rejected-pass sources and the measured-band contract are preserved in
`build/evidence/farmland-overhang/`. Its `after-windows.json` records current counts;
the source checkpoint binds the final owned files. The pinned clang-format22.1.8
check and `git diff --check` pass. The coordinator owns the combined aggregate,
native camera/fade/infantry integration and capture. No independent recook, GPU
render, performance improvement or new device qualification is claimed here.
