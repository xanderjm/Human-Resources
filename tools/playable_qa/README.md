# Native gameplay qualification helpers

These host tools exercise the product executable, not the scenery viewer or the
Python balance simulator. Use an optimized `dev` build. Coordinate an exclusive
foreground window before running; focus loss correctly pauses the game.

```
python3 tools/playable_qa/profile.py --output build/evidence/playable-30s --frames 1800 --seed 514
python3 tools/playable_qa/profile.py --output build/evidence/playable-combat --frames 6000 --step .2 --seed 514
python3 tools/playable_qa/profile.py --output build/evidence/playable-metal --frames 1800 --metal-hud
python3 tools/playable_qa/capture.py --output build/captures/playable-portrait.mp4
python3 tools/playable_qa/capture.py --output build/captures/playable-flow.mp4 --flow
```

Every profile uses a fresh save under its new output directory. The client starts
a seeded run, moves from semantic actions, chooses card zero, settles an actual
death and retries. It does not finish the run by a timer. A `.2` simulation step
accelerates combat coverage; use `1/60` (default) for ordinary animation speed.
The accelerated case is a stress/coverage test, not a human survival-time claim.

The CSV distinguishes actual frame intervals, game advance, presentation, CPU
render submission and total CPU frame time. CPU render submission includes
driver/present waits. It must never be reported as GPU time. The runner records
macOS `time -l`, sampled process RSS, source/content/executable hashes, scene
counts, warm-up exclusion and late-combat buckets. It exits nonzero if source
changes during the run, the application fails, or frame counts differ. Stable
hashes do not replace a clean rebuild: confirm the executable was built after
the source freeze. Compare equal seed, step, frames, account state, render size
and HUD state. A single hitch does not establish its cause.

The optional [Apple Metal HUD](https://developer.apple.com/documentation/xcode/monitoring-your-metal-apps-graphics-performance)
emits separate GPU diagnostics. Preserve the raw native log. HUD logging adds
observer overhead and is kept separate from the primary no-HUD pacing case.
No samples means unavailable evidence, never a zero GPU cost. The current parser
summarizes reported log values without claiming unique frame IDs or converting
the HUD's units.

`capture.py` streams RGBA through a FIFO into FFmpeg, so no large raw file is
created. Defaults are 390x844 logical points and the observed 588x1280 render
target on this Retina host. Use explicit render dimensions on other hosts. It
checks decoded count/dimensions/rate and a complete error-on-decode pass, records
producer/content hashes, and extracts early/middle/late stills. Readback and
encoding alter timings; movie FPS is not live performance evidence. Recordings
are silent native visual evidence; native audio needs separate listening QA.

`encode_capture.py` preserves an existing raw capture as H.264. `--delete-raw`
only removes an ordinary `.rgba` file under ignored `build/`, after verifying
its exact byte count, raw hash stability, complete decoded movie and durable
provenance report. Source assets and archives are never eligible for removal.

All evidence remains host-specific. Mac portrait aspect does not establish
physical phone performance, thermal behavior, touch quality or release status.

`engagement_proof.py` builds an isolated public-Game review executable from the
existing optimized Ninja graph. It supports a fresh checkout after ordinary
bootstrap/build; historical previews and ignored qualification reports are not
prerequisites. On a host with the documented compiler/SDK, Python 3.11+, CMake and
Ninja, first run:

```
python3 tools/project.py bootstrap
python3 tools/project.py test --preset dev
python3 tools/playable_qa/engagement_proof.py --output build/evidence/engagement-build --build-only
```

The last command compiles only its private executable and runs CPU-only public
Game validation; it never initializes the GPU or changes production build
targets. For native captures, use the pinned art Python with Pillow, have FFmpeg
and FFprobe on PATH, coordinate the GPU slot, and select a new output directory:

```
build/tools/art-python/bin/python tools/playable_qa/engagement_proof.py --output build/evidence/engagement-composition --composition
build/tools/art-python/bin/python tools/playable_qa/engagement_proof.py --output build/evidence/engagement-ui --ui-only
```

Every mode inventories all existing `.app` bundles under `build/dev` and
`build/evidence` before work, then compares file hashes and symlink targets after
work, including failed captures. It does not execute or control protected apps,
or read their saves. Missing historical previews are valid; discovered bundles
are always protected. Add `--protected-app /absolute/path/Other.app` for bundles
outside those roots. Review output inside any app bundle is rejected. Newly
appearing, deleted or changed bundles fail protection rather than being ignored.

The generated `protected-apps-before.json` is also an optional canonical input
for a later run: pass `--protected-app-manifest PATH`. Schema 1 contains a
`bundles` array, each entry with `path`, relative `files` mapped to SHA-256 and
optional relative `symlinks` mapped to literal targets. Relative app paths resolve
against the manifest directory. The complete canonical inventory must match all
discovered/explicit bundles exactly; missing apps/manifests, omitted bundles,
unsafe entries and mismatched hashes fail before compiling. Never reuse an old
manifest as a fresh baseline without validating its intended bundle set.

`protection.json` and `qualified.json` retain actual inventory counts and selected
Python/C++ producer hashes before/after. A failed protection check stays explicit
even if capture already produced a report. These are file/symlink integrity
checks, not app signature, permission-mode or release qualification. The default
discovery roots are bounded to local build/evidence trees; use explicit paths for
other copies. Focused no-GPU tests use temporary fake bundles:

```
python3 -m unittest tools.tests.test_protected_apps -v
```

An explicit `--qa-metrics` native run now retains at most four exact Game blobs
beside the CSV, under `<metrics-path>.game-checkpoints/`: first actual jeep,
tank and boss projectile, plus first positive vehicle capture if encountered.
JSON sidecars identify native frame, run and source actor. These are read-only
review checkpoints, not account imports; absent events produce no placeholder.
Their bounded file writes are QA instrumentation, not performance evidence.

`visual_stress.py --vehicle-transition` records a labelled 17-second native
fixture: synthetic initial damaged vehicles/Winch3/Splitter2, then actual public
Game advancement through warning, firing, capture range loss and full return.
The displayed composition isolates the selected real actor/projectiles/beams.
The jeep starts 13 m away to permit one shot before close-range cancellation;
this is explicitly outside the ordinary 12.6 m ingress fixture. It does not
claim a naturally earned encounter. The native log checks actual outcomes and
the wrapper verifies all 1,020 encoded 60 Hz frames and complete decode.

`verify_account_authority_upgrade.py` tests untouched historical qualified
Android bytes privately against the current portable authority: real pending
80-credit backup/Bank-once recovery and settled seven-credit/rank-one no-write
load. It never installs an account into an app or changes original fixtures.

The standalone synthetic visual fixture uses the production libraries, scene,
renderer, shaders and cooked assets without changing the application or balance:

```
python3 tools/playable_qa/visual_stress.py --output build/evidence/visual-capacity-build --build-only
python3 tools/playable_qa/visual_stress.py --output build/evidence/visual-capacity-proof --visual-proof
python3 tools/playable_qa/visual_stress.py --output build/evidence/visual-capacity-profile
```

Use a freshly rebuilt, frozen `build/dev` graph. The helper compiles its own
object/executable in the evidence directory using the configured toolchain and
exact product archive linkage; it does not rebuild or modify the production
targets. `--build-only` validates portable starter and gear geometry without
initializing SDL video/GPU. Coordinate exclusive GPU access for either remaining
mode. The proof captures starter geometry plus separately labelled readable gear
and saturated compositions in all three worlds. It preserves pixel-verified PNGs
and is excluded from timing results.

The single timed run lasts 24 seconds: eight per world, including two seconds
with reduced motion. All frames retain six capture beams, 55 actors, 96 enemy
projectiles, the production shot-vector bound (currently 64), six rank-III
hardware slots, two compatible evolutions and 48 cached event beats. These
simultaneous capacities are synthetic, not evidence of naturally earned loadout,
survival, input quality or fun. Actor IDs change every two seconds to expose
bounded resource churn. Startup cost, each world's first 30 frames and remaining
steady frames are reported separately. Metal HUD adds observer overhead. The
ordinary C++ allocation probe excludes SDL C allocations, GPU private resources
and other threads; its allocation bytes are not live memory usage. No mobile or
indefinite-memory qualification follows from this bounded host fixture.

The infantry/spatial milestone has a separate proof mode in the same fixture:

```
python3 tools/playable_qa/visual_stress.py --output build/evidence/infantry-spatial-proof --milestone-proof
```

Wait for the coordinated source/art freeze and rebuilt optimized application.
This uses `product::scene` and its actual 17m portrait camera, rather than the
independent style viewer. Eighteen labelled synthetic PNGs cover seed 514 Farmland
stations 0/33.6, four maximum-gear corners in each location, legal candidate
threat/occlusion poses and a forced shotgun-edge cactus regression. The forced
case's aim is outside the current player band and is geometric evidence only.
Every PNG is decoded, pixel-hash matched and checked for a nonblank readback.
The native log retains actual draw/triangle counts and faded-prop counts.
These are visual review inputs, not naturally earned play, frame-time or art
acceptance. Review target and warning readability with the retained shadows.

Historical pre-commitment review: after the Farmland/capture-continuity correction, use the two targeted
modes instead of repeating the long normal film or the entire capacity set:

```
python3 tools/playable_qa/visual_stress.py --output build/evidence/farmland-overhang-proof --composition-proof
python3 tools/playable_qa/visual_stress.py --output build/evidence/infantry-transition-proof --infantry-transition
```

The second writes a labelled 12-second / 720-frame scripted native movie through
a FIFO. Three role bodies show travel, planted warning, one typed fire event,
actual launch-cache/recoil behavior and recovery. They then explicitly replay
one actual neutral-input public-Game seed 514 pig capture-loss sequence; no
capture progress is hand-ramped backward. The source .941666→0 reset and 90
post-loss ticks remain exact. This is a controlled animation/composition proof,
not a naturally occurring three-role capture. Native logs retain the source
actor/kind/time and consecutive body transforms around cancellation. Freeze the
production transport/renderer and scenery owners together before execution.

The capture-loss portion of `--infantry-transition` requires its historical
pre-26-October rules. Current committed beams cannot produce that reset; do not
run this historical cancellation proof against current gameplay or reinterpret
its old movie as current evidence. Current `capture_gameplay_contracts` and
`scene_effects_contracts` exercise actual range escape through successful pickup,
retaining separate synthetic presentation-release coverage.
