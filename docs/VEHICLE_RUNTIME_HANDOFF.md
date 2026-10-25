# Source-coherent vehicle runtime

This slice integrates the existing unmanned jeep minigun and corrects composite
vehicle transforms. It changes gameplay authority to revision 3; account schema
2 is unchanged. Implementation and portable checks do not establish mobile,
store, or final native visual qualification.

## Assembly and source ownership

`assets/production/assemblies.json` owns the accepted chassis +0.12 m offset,
four actual wheel sockets and left hub orientations, and tank turret socket
[-0.04, 0.955, -0.24] m. Product composes every rigid child through the complete
resolved parent translation, rotation and uniform scale before flattening to
the existing instance contract. Wheels and turret therefore follow capture
pitch/roll. The existing bounded capture history retains the last vehicle yaw
when lifting zeroes velocity and returns toward the moving ground heading over
the shared release duration; it does not change movement or collision.

`assets/production/vehicle-weapons.json` records source hashes, reviewed mount
pivots, six head-local muzzle points and the measured tank aperture. The offline
`tools/cook_vehicle_content.py` reproduces fixed post (424 triangles) and yaw cap
(560 triangles) from the original mount GLB. It applies the source node matrix
once and subtracts the cap yaw pivot once. All 984 source triangles, directed
topology, UVs and albedo pixels are retained; repeated output bytes match. The
original head is consumed directly. Runtime provenance names the durable source
and subset hash, never an ignored build path as an art authority.

The minigun remains unmanned. No new model, bitmap, seated pose, barrel spin,
renderer hierarchy, physics engine or active-frame asset I/O was introduced.

## Firing and continuity

Portable authority analytically solves the selected actual minigun opening
against the committed 2D aim point and target height 2.5 m. Eligibility requires
at least 4.5 m from the muzzle and total upward elevation 0–14 degrees. It is
checked before commitment, during warning and at firing because the jeep moves.
Unreachable or close targets cancel without a projectile, event or ID and retry
on the ordinary cooldown. This deliberately gives close-range counterplay.
Barrels advance deterministically only on accepted commitments; there is one
projectile per existing cadence, with unchanged damage, speed, warning, cooldown,
spawn rate and abstract 2D collision radius. Many closing targets can cancel
warnings; ordinary native play must report that effect without silently
compensating with a damage/timing change.

A bounded independent public-Game check across three seeds and neutral/weave
inputs resolved the first-engagement concern without retuning: weaving produced
36 actual minigun shots, with 25 of 27 jeeps firing and only two geometry
cancellations among 42 accepted warnings. Neutral input produced four shots
across 12 jeeps. All six runs ended in natural death (151.65–329.37 active
seconds); no injection, revival or rule change was used. This source-stable
headless sample establishes input dependence, not native visual quality or fun.
Evidence: `build/vehicle-assembly-audit/ordinary-jeep-sample/`.

`tools/measure_tank_cannon.py` independently identifies the connected dark bore
and closed ten-edge actual inner rim. It measures aperture area-centroid XY
[-0.006042052873, 0.528119092772], source forward +Z, and nonplanar front rim
maximum Z 1.284246575342 m. Launch is 10 mm beyond that front rim. The existing
missile at uniform XYZ scale 0.20 has bounding sphere 0.087000221 m versus
0.099936253 m launch-to-turret clearance: a conservative 12.936 mm guard.
The boss applies its existing whole-assembly 1.6 scale once. Source and visible
missile use the exact same turret/muzzle matrix; neither mesh diameter nor length
is derived from the abstract damage radius.

Tank/boss missiles leave horizontally along the real cannon. Product smoothly
guides elevation to the committed UFO plane over the saved flight. The boss's
unchanged three-shot ±0.22-radian 2D fan uses a bounded 120 ms presentation join
from the saved central barrel axis to its existing collision path. The saved
source/profile survives source death and cold restore. A compact coral trail
and ground hazard ring communicate the unchanged abstract hit radius.

Capturable jeep and tank suppress commitment/fire during positive capture and
the bounded return. One generated `capture_return_seconds=1.4` contract is
checked against both accepted authored release recipes and consumed by product
history and authority. Capture loss resets progress immediately; no movement,
reward or capture outcome is delayed. Pause freezes the clock, reacquisition
preempts it, and disable does not extend it. Boss remains uncapturable.

Correct source turret placement required a measured ingress correction from
13 to 12.6 m: the complete 320/390 phone envelope had only 8.252 px top guard at
13 m, versus 13.764 px at 12.6 m. Camera 17 m, field half width 5.65 m, retirement
11 m, forward/back ±6 m, HP, speeds, damage, rates and credits are retained.
Final HUD changes must recheck this against the actual occupied control region.

## Save and proof boundaries

Authority 3 explicitly serializes selected barrel, projectile kind and physical
launch direction. It updates the numeric rules fingerprint. Older authority 2
continuations use the existing protected-account backup/Bank path rather than
decoding the changed layout. An actual qualified Android interrupted account
with 80 pending credits passes optimized and ASan/UBSan proof: zero ordinary
writes, exact backup, injected replacement failure preservation, explicit
Bank 80 once, cold reopen and repeated Bank idempotence. The full resulting
settlement matches its historical 80-credit account except the recovery flag,
including the earned blueprint unlock. Actual settled 7-credit/rank-one bytes
load unchanged without writes. Originals remain hash-identical. This proves
portable compatibility against historical device bytes, not a new Android run.

Focused vehicle checks cover 3,456 independent minigun source rays, 28,848
near-envelope samples, 1,152 tank/boss source rays, shared capture/return gates,
typed source-death continuations and malformed metadata. Optimized and
ASan/UBSan pass. Product checks cover full parent matrices, actual muzzle/flash
agreement, tank/boss source-fitted missile size, initial departure tangent,
guided elevation, short fan join, capture-enter yaw and full return.

Current aggregate passes 39/39 host suites. Evidence is under
`build/vehicle-assembly-audit/`: `final-aggregate-release-toolchain.log`, `final-gameplay/evidence.json`,
`authority3-account-compatibility/evidence.json`, `tank-aperture.json`,
`tank-aperture-reproduction.json` and `split-evidence.json`. Earlier failed
fixture/qualification attempts are preserved.

Root accepted the source-stable 17-second native vehicle fixture (1,020 frames,
60 Hz): actual public Game firing, capture-enter, immediate range-loss reset,
full return and boss fan. Its initial damaged vehicles/Winch3/Splitter2 and
source-only display composition are explicitly synthetic; the jeep starts at
13 m to permit one shot before the close-range stop. This does not claim
natural capture coverage. All eight pairs preload and all 69 asset IDs load.

Initial native pixels rejected the fine shot readability despite correct
source geometry. One presentation-only correction preserves the exact finite
solids and adds the already accepted infantry optical trail/core widths
0.07/0.025 m plus one inscribed coral hit-footprint fill. Root accepted the
single 3.6-second/216-frame native 320-pixel shot proof. Centred pursuit still
projects the jeep shot over its own chassis and then the UFO; exact live-shot
age/position/screen coordinates explain that pose limitation. No false source
offset, gameplay retune, art or renderer change was introduced. The source-bound
before images remain beside the after proof, not overwritten.

Corrected compact/large-type UI and six default/largest Privacy reader frames
also passed native pixel review. Their View snapshots do not prove OS text
preference delivery or touch/Back routing.

The ordinary seed-514 native film contains exactly 11,400 frames/190 seconds,
seven natural drafts, natural death at 159.4 active seconds and automatic
bank/retry. The actual results frame shows score 108, 27 captures and +46 saved
credits. The terminal account's shipping read-only validator confirms one
completed run, 46 credits/46 banked base credits, no purchased ranks, and a valid
active retry run 2. Terminal/result screens occupy only two instrumented frames;
this is flow evidence, not human menu dwell or pacing. Peak observed counts were
19 actors, six projectiles, one beam, 65 world/44 shadow draws and 882,464 reported
triangles. Readback and concurrent platform builds exclude performance claims.

Exact full Game checkpoints retain first actual jeep fire (native frame 4395,
70.333 s) and boss fire (frame 9585, 153.9 s). Both cold canonical roundtrips pass;
two cold owners also remain equal over 120 identical neutral-control steps.
This is not a replay of the original weaving trajectory. No tank fire or vehicle
capture naturally occurred in the bounded film; no placeholder was generated.
See `build/evidence/vehicle-ui-native/ordinary-flow/ordinary-summary.json` and
the sibling `.frames.csv.game-checkpoints/` directory.

The actual current macOS release package passed its allowlist audit and a
112-file exact copy ran the Metal/280-frame scripted flow smoke outside the
checkout. The disposable outside copy was removed after preserving hashes/logs.
The retained package is `build/packages/macos/human_resources-0.1.0.zip`, SHA-256
`c9c057e9c586d408ec33efea7a0be4164fb93b33fe9779dc920c370903e0a492`.
Proof: `build/vehicle-assembly-audit/macos-outside-release-proof.json`.
These are current-host package checks, not signing/store or physical-phone
qualification. Platform closure remains the mobile owner's responsibility.

Final canonical source closure is
`build/vehicle-assembly-audit/vehicle-release-source-freeze.json` (556 inputs;
SHA-256 `3bc3dae5424a05cf24986cea1414238f18facffd742698fd55dbcc208662e490`).
It retains the prior epochs and rebinds only the approved `verify-ios.sh`
build-Python wrapper delta after the native freeze. All 101 staged runtime and
shader files remain unchanged; the generated runtime manifest itself contains
81 files. Final source/asset recheck reports zero changes. The final
`project.py evidence` record is
`build/evidence/vehicle-ui-native/macos-authority3-qualified.json` (SHA-256
`c8304cb588070077708f776cbb77f01c721aa463eb3abeaa1a56170485f346fc`), including
the actual GPU/package logs, root visual review and independent art-tool hashes.

## Consumed versus authoring content

Shipping static models increase from 55 to 58: existing head plus the two exact
mount subsets. The eight animated startup pairs remain unchanged. Each jeep
gains three rigid instances and 3,571 triangles; at the existing five-jeep cap
this is at most 15 additional instances and 17,855 triangles before culling.
Actual native/mobile timing still requires measurement.

The optical cue adds three effect instances per minigun round and one per
tank/boss missile compared with the initial fine-shot candidate, within the
unchanged global 96-projectile cap. It adds no mesh/texture/preload resource.

The three accepted equipped infantry bodies and their three weapons remain
consumed in their existing deterministic roles. Jeep chassis/four wheels,
original minigun head and every original mount triangle now have an active
runtime role; tank hull/turret and missile retain theirs. The original unsplit
mount GLB is offline source only. The older tiny soldier and alternate source
character exports remain authoring/reference alternatives and are not added
to shipping merely to inflate an asset-use tally. No seated crew is claimed;
that requires a genuinely accepted seated rig and fit proof.

Portable art tooling and real byte-identical cold export proof are recorded in
`build/evidence/art-toolchain-cold/report.json`. Include
`build/evidence/art-toolchain-cold/independent-tools.json` as an additional
`project.py evidence --smoke-log` input with final platform qualification.
