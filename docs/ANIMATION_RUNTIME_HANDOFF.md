# Native animation integration handoff

Final ownership: this animation chat has no pending code changes. The
coordinating release owner reports `performance_qa` has completed required-game
animation preload in `style_animation`/`StyleRenderer::initialize`; startup/hitch
qualification belongs to that owner's evidence. The recorded six-character
moving proof verifies poses/uploads/sockets before that preload revision.
`combat_presentation` owns subsequent scene/capture-convergence/combat FX work
after exclusive profiling ends. Animation contracts remain frozen; this chat
will make no further scene/renderer edits. Preserve Android SDLIO, the static
preview's no-HRA mode, and lookup-only active rendering.

Final gate note: `animation_contracts`, `animation_instances_contracts` and
`product_scene_projection_contracts` all pass together in the integrated dev
CTest tree. The local animation CMake explicitly enables testing because the
parent enables it after `add_subdirectory`; without that call the portable
test executable built but was missing from aggregate CTest. This registration
fix was the final animation source change. Root still owns post-projection-fix
full playable capture, aggregate/platform qualifications and the shared log.

Current status: portable decoder/player/cooker is implemented and FROZEN; all six models
pass 5,622 source-GLB key/half-frame parity samples (maximum position/socket
errors below 1 micrometre). Renderer consumer owner is
`/root/animation_consumer`; its exact client header/build patch is at the TOP of
`docs/ANIMATION_RENDERER_HANDOFF.md`. It owns animation cache/skin/upload/socket
changes plus the explicitly handed-off 20.5m portrait camera and corrected
forward-axis mapping in `scene.cpp`. The client owns UI, app, root CMake and
`style_scene.h`; its required fields/build links have landed. Preserve Scene144 and 64×112-byte
instance UBO/retained-fence contracts. Root handles SaveAccountSession separately.

The public interface is `native/human_resources_animation/include/human_resources/animation/animation.h`.
This portable owner has no SDL, renderer, gameplay, filesystem or platform dependency.
Integration is implemented against this concrete interface; renderer/client edits remain with their owner.

Parallel consumer assignment: `/root/animation_consumer` owns new renderer helper
files and `docs/ANIMATION_RENDERER_HANDOFF.md`. The coordinating owner has now
handed `style_renderer.cpp`, `style_resources.cpp` and product `scene.cpp` to
that consumer after the client's Android SDLIO resource fix. App, product
interface, `style_scene.h` and root CMake remain with `playable_client`; relay
the renderer handoff's exact value additions there. Preserve product UI
SDF/atlas and occluder fade work. Portable player/cooker/tests remain here.

## Client and renderer seam

- Load immutable `animation::Asset` values from bounded HRA1 bytes using `animation::decode(bytes, asset)`; check `result.valid()` and report `issue/offset` on failure. Keep asset addresses stable while players reference them.
- Maintain one `animation::Player` per stable biological entity ID, separate from gameplay. On spawn `bind(asset,"walk")`, then `seek(stable_phase * clip_duration)` to desynchronize walkers. Remove the player when the entity disappears; do not key by changing vector position.
- Pass only active presentation delta to `advance(dt, rate)` (0 through 0.25 seconds, rate 0 through 256); pass zero while paused, upgrading, backgrounded or interrupted. Rate changes only clip phase; fades retain their active-second duration. Never use wall time since launch. Repeated `play("walk")` preserves phase.
- Choose clips from presentation state: ordinary movement `walk`; warning onset `startle` then `run` while escaping; capture onset `beam_lift`, `finished()` then `beam_struggle`; cancellation `beam_release`, `finished()` then locomotion. Use ~0.18s fades for locomotion, short ~0.08s startle/capture entry, and zero fade for the authored matching lift→struggle→release boundaries. Do not derive gameplay mutations from clip completion.
- Root movement remains the existing entity world transform. Clip speed 1 means `stride_m / seconds`; scale the animation delta by movement speed divided by that nominal speed when moving. At rest play `idle`, rather than moving feet in place. Do not add the temporary organic bob to skeletal root motion. The `beam_*` clips already include their 0.55m visual lift; avoid applying that visual offset twice.
- `skin_vertices(asset, player.pose(), scratch)` writes actor-local positions/normals into a preallocated span with exactly `asset.vertices().size()` records. Its ordering matches the companion `<id>.hrm`. Copy these two fields into the existing HRM1 vertices; keep UV, colour, emission, rotor membership, indices and texture unchanged. Upload only changed vertex data. Actor world matrix applies afterwards.
- `player.node_transform("socket_head")` gives the posed actor-local attachment matrix. Apply `actor_world * posed_socket * inverse(rest_socket) * reviewed_hat_rest`; reviewed hat placement is in `assets/production/assemblies.json`. This replaces a fixed world-space hat offset and follows head motion. Null means no socket (animals).
- Animation matrices are column major; the existing presentation `Matrix` is row major. Convert explicitly at this boundary before composing attachments. The native moving capture caught a misplaced hat when those storage conventions were mixed; the renderer helper now owns the conversion and its regression check.
- CPU skinning is a focused first consumer. Reuse a deformed mesh for identical asset/clip/phase cohorts where practical; do not perform one deformation per draw pass. Renderer owns GPU uploads and resources, presentation owns instance/phase cache. Profiling evidence will be appended below; physical-device budgets remain open.

## Cook/build wiring for coordinating owner

New target local CMake: `add_subdirectory(native/human_resources_animation)`;
link `human_resources_animation` to the consuming renderer/client and tests.
No shared root CMake or renderer files are edited by animation authoring.

The offline cooker will emit `<id>.hra` plus matching `<id>.hrm` for cow, pig,
sheep, farmer-body, resident-body and soldier-body into a caller-selected ignored
output directory. It uses the existing NumPy/Pillow authoring environment only.
No GLB parser, Blender or Python ships in the native player. Source models and
the checked-in animation recipes/scripts remain the rebuild authority; build
exports are generated inputs and will be rebuilt if absent or stale.

## HRA1 byte contract

All scalars are little endian. Matrix storage is column major; rotations are
glTF x,y,z,w quaternions. Names occupy 32 bytes (nonempty ASCII identifier,
zero terminated and zero padded). HRA1 stores no indices, materials or textures;
the paired HRM1 owns those and shares vertex ordering exactly.

1. `char[4] HRA1; u32 node_count, joint_count, vertex_count, clip_count`.
2. Each node: `i32 parent; char[32] name; float[10] rest_TRS` (translation3,
   rotation4, scale3). Parent is -1 or strictly before the current node.
3. Each joint: `u32 node_index; float[16] inverse_bind`.
4. Each vertex: `float[3] position, normal; u16[4] joints; float[4] weights`.
5. Each clip: `char[32] name; float seconds, stride_m; u32 loop, frame_count`,
   followed by `float[10] TRS[frame_count][node_count]`. Samples are uniform and
   include both endpoints. Loop endpoints must agree (quaternion sign may differ).

Decoder limits: 16MiB per file; 64 nodes/joints, 32,768 vertices, 16 clips,
256 frames per clip, duration up to 10s. Transforms must be finite, translations
within ±10,000m, positive scales in [.5,2], unit quaternions, affine nonsingular
bind matrices, valid joints and normalized influences. Unknown magic, unsafe
counts, duplicate names/joints, cycles, truncation, malformed loops and trailing
bytes reject with an empty output and a `DecodeIssue`/byte offset. Runtime owns
no GLB parsing, disk reads or allocations during playback/skinning.

## Reproduction and current evidence

```sh
python3 tools/cook_character_animation.py --output build/animation/native-cooked --validation-output build/animation/native-references
cmake -S native/human_resources_animation -B build/animation/native-tests -DCMAKE_BUILD_TYPE=Release
cmake --build build/animation/native-tests
build/animation/native-tests/human_resources_animation_tests build/animation/native-cooked build/animation/native-references
```

Use the existing NumPy/Pillow art Python for the cook. `--blender PATH` supplies
the authoring executable when a clean checkout has no exports; `--rebuild-exports`
forces a rebuild from source. Current validated exports need no Blender process.
Run this cooker **after** the static product cooker, into the same consumed
content root, to replace the six biological HRMs with exactly matching pairs.
Manifest entries retain original source-model and recipe/tool hashes as well as
generated GLB/HRA/HRM hashes. Root build integration must declare the recipes,
original models, authoring scripts and cooker as producer dependencies; generated
GLBs or a pre-existing build directory cannot be the sole source authority.

`build/animation/native-parity.log` records 5,622 independent source-GLB key and
half-frame samples across all 60 clips. Maximum component position error is
0.716 micrometres, normal error 0.000000835, head/socket matrix error 0.954
micrometres (translation components in metres). Includes clip looping, shortest
quaternion arcs, pause, invalid delta/rate rejection, one-shot completion,
interrupted fades, active-time fades under scaled locomotion and malformed-file
tests. `native-sanitized-tests.log` holds the ASan/UBSan run. Optimized per-model
CPU timings are in the parity log; they do not include GPU uploads or establish
mobile frame budgets. Renderer evidence and current ownership are in the
separate renderer handoff.

## Remaining artistic motion work

The owner rejected the original stiff walks; the revised shared walks are the
current candidate, not a recorded owner art acceptance. Numerical parity and
native playback prove faithful delivery, not animation quality.

- Human `idle` and `run` retain the older bind-relative arm posture. Idle only
  animates spine/head, and run pitches the original arm chains with a small
  outward roll; neither uses the revised lowered-arm walk solve. Switching
  between walk and idle/run can therefore visibly spread the arms again. This
  posture mismatch remains an authoring issue.
- All six `run` clips retain sinusoidal FK plus whole-pose floor correction.
  Their feet do not follow the walk's constant-velocity planted stance path,
  so run foot sliding/stiffness remains open, including warning-driven escape.
- The six walks have no runtime world-space foot locking through sharp turns
  or clip blends. Source-key ground/contact checks and subframe source parity
  do not establish contact quality for those situations.
- Beam clips provide local vertical lift/struggle/release only. Horizontal
  convergence toward the saucer belongs to `combat_presentation`; accurate
  skeletal playback alone does not make a complete abduction trajectory.

Other gestures/reactions remain first-pass. No new authoring changes, runtime
features or additional films are implied by this remaining-work list.
