# Native renderer animation handoff

Final ownership update: animation authoring/consumer work is frozen. The
coordinating owner reports `performance_qa` completed startup animation preload;
its timing/failure evidence is separate from the earlier native moving smoke.
`combat_presentation` owns subsequent product-scene convergence/FX changes after
exclusive profiling. No pending renderer/scene edits remain in this chat.
Artistic motion limits are listed in `ANIMATION_RUNTIME_HANDOFF.md`.

## P1 forward-axis correction — reopened by coordinating owner

Corrected ONLY product presentation mapping: render Z = runner station minus
world Z plus 2.3m; saucer uses runner-player Z; facing yaw uses
atan2(velocity X, -velocity Z). Actors, beams, warnings, shots and projectiles
share this mapping. Scenery already used the correct forward sign and was not
changed. Semantic up remains gameplay +Z. Camera stays20.5m portrait; physical
scale and gameplay bounds/rules are untouched.

Added `product_scene_projection_tests.cpp` and a local product CMake test target.
It exercises actual semantic Controls→Game→scene projection: up must move the
saucer screen UP, spawn ahead must project ABOVE the saucer, actor facing and
all target presentations must share the axis mapping. Both portrait and landscape
and station-translation invariance are covered. Host native player and new regression target rebuilt successfully. CTest
`product_scene_projection_contracts` passes (portrait and landscape). Scene
source is frozen again; root must review the full playable capture after this
correction. Build/test logs are in `build/animation/native-tests/projection-*.log`.
Portable animation/cooker and the earlier six-character native smoke are frozen.


## Current status — 2026-10-25

Implemented and integrated: portable per-actor consumer, SDL_GPU cycling vertex
uploads, posed culling bounds, stable-ID clip state, animated headwear and removal
of temporary organic bob. Both renderer targets now link the shared CPU consumer
and portable animation owner. Client header fields and active-delta producer are
on disk. Native Metal smoke passed 180 portrait frames through the game renderer.

Consumer-owned files are `animation_instances.{h,cpp}`, `style_animation.{h,cpp}`,
`actor_animation.h`, animation integration in `style_renderer.cpp` and product
`scene.cpp`, and the two new tests below. The coordinating owner explicitly
handed off scene camera width after the earlier hold: portrait is 20.5m,
landscape 31m, physical actor/UFO scale unchanged. UI layout and root CMake/app
remain client-owned. Scene144/Instance112 uniforms, 64-instance batches, product
UI atlas/SDF, occluder fading and retained-fence lifetime remain intact.

Only optional build wiring remains: add `EXCLUDE_FROM_ALL` executable
`human_resources_animation_render_smoke` from
`native/tests/animation_render_smoke.cpp`, linked to
`human_resources_game_renderer`. Its source already compiles with actual strict
host flags; the successful native evidence run was manually linked against those
same game renderer/library outputs. No additional authoring film was produced.

## Actual integration

- `human_resources_animation_instances` compiles
  `native/human_resources_renderer/src/animation_instances.cpp` and publicly
  links `human_resources_animation`, assets and presentation. Both renderer
  targets link this target and compile `style_animation.cpp`. This exposes the
  portable animation include directory on Android and desktop.
- `StyleInstance` carries `ActorAnimation` (enabled, speed, warning, capturing),
  nonzero stable ID and optional attachment parent/local matrix. `StyleFrame`
  carries `active_delta_seconds`. App supplies the actual active run delta, zero
  while paused, choosing upgrades or inactive. No wall-time animation clock.
- Scene sets biological actor speed from velocity and capture/warning flags from
  presentation snapshots. No animation completion mutates gameplay. The gameplay
  catalogue currently instantiates cow, pig, sheep, farmer and soldier; resident
  remains a supported cooked/validated sixth model, demonstrated by native smoke.
- Package HRA1 **and matching companion HRM1** beside each other. Runtime verifies
  companion vertex positions/order before accepting an asset. Existing static
  body HRMs cannot replace animation cooker companions. Original art is unchanged.
- CPU cache has six immutable assets and at most 128 actors. It allocates at asset
  load/spawn; steady updates retain vertex/skeleton scratch. IDs survive vector
  reorder; disappeared IDs are evicted; retry reuse for a different mesh creates
  a fresh player. Duplicate IDs in a frame fail rather than advance twice.
- Walk/run rate equals movement speed divided by clip stride/duration. One
  `Player.advance(active_dt, rate)` evaluates each actor, while fades keep active
  second duration. Paused actors neither advance nor change clips. Warning edges
  startle then run; capture lifts then struggles; cancellation releases then
  returns to locomotion. Capture decisions remain with gameplay.
- GPU cache retains a cycling upload buffer and vertex buffer per actor. CPU
  deformation, bounds and upload happen once before passes; colour and shadow
  bind the same buffer. UV, colour, emission, rotor data, indices and texture are
  retained. Upload revision commits only after GPU command submission succeeds.
- Hats use `actor_world * posed_socket * inverse(rest_socket) * reviewed_hat_rest`.
  Complete GLB hierarchy is included in the rest socket. Farmer reviewed rest is
  [0,1.30,0], yaw180; resident [0,1.32,-.04], yaw0 (assemblies.json). Scene removed
  the organic bob and subtracts authored .55m beam lift from remaining root travel.

## Verification and evidence

`native/tests/animation_instances_tests.cpp` passes CTest in `build/dev` and the
same contracts plus six actual cooked pairs pass ASan/UBSan. Covered: scratch
retention, non-pose attributes, socket rest removal, pause/transition freeze,
startle/run, lift/struggle/release, duplicate IDs, eviction, retry rebind, invalid
delta and 128-actor cap. Portable GLB subframe parity is separately owned by the
animation authoring agent.

Host CPU-only 64-actor/60-frame samples range about 7.3–10.4ms per frame while
other release work ran; six assets total 24,373 source vertices. This is evidence
for the bounded first consumer, not a stable benchmark or mobile qualification.
GPU upload/draw cost and physical-device budgets remain open.

`native/tests/animation_render_smoke.cpp` renders six walking actors, both posed
hats, soldier warning and farmer capture/release with the real native renderer:
180 frames at 540x960/30Hz, Metal, 9 world and 9 shadow draws per logged frame.
Inputs are six companions in `build/animation/native-cooked` plus existing
reviewed hats/ground, staged under `build/animation/native-gpu-content`.

Evidence (ignored generated outputs):

- `build/animation/native-tests/native-gpu-smoke.log`
- `build/animation/native-tests/native-animation-portrait.mp4`
- `build/animation/native-tests/native-animation-walk.png`
- `build/animation/native-tests/native-animation-capture.png`
- `build/animation/native-tests/consumer-evidence.log`

This smoke exercises the production renderer boundary. Root/client still owns
full playable client capture, aggregate gate, packaging and platform evidence.

Final visual review passed after fixing the column-major glTF to row-major
presentation attachment boundary. The regression covers nonidentity translation
and +90-degree rotation, and the corrected native capture shows farmer headwear
remaining attached during the beam lift. Final CTest passes; source hashes,
asset hashes, video hash and GPU log are recorded in
`build/animation/native-tests/native-consumer-evidence.json`. Consumer source is
frozen at this checkpoint pending a real defect from qualification.
