# Infantry walk authoring handoff

The three new T-pose bodies have separate candidate rigs and baked animations at
`build/animation/infantry/exports`, with packed editable equipped scenes in
`build/animation/infantry/blend`. The latest revision adds one bounded chest recoil impulse and a focused aim/fire/capture review; see INFANTRY_AIM_FIRE_REVIEW.md for exact socket, elevated aiming and producer contracts.
They do not replace the native soldier or alter shipping content/cooker contracts.

`assets/animation/infantry-fit.json` binds measured landmarks, source hashes,
palms and equipment fits to the original body/weapon GLBs. The recipe is
`assets/animation/infantry.json`; `infantry_rig.py` and `infantry_motion.py` under
`tools/animation` own rig construction and baked motion. The same `SharedWalk`
solver and humanoid profile drive all six people. Infantry arms hold their
weapons using one weapon frame and two solved palm contacts, rather than swinging.

The source hands are open flat mittens. A broad distal fold preserves their
topology but does not create individual fingers or a closed trigger grip. This
remains a visible source limitation. Cross-body weapon yaw and explicit left
clavicle protraction allow the short source arms to reach both measured grips
without stretching bones. Rifle, shotgun and shoulder launcher retain distinct
carry fits. Consumer aiming must use the actual muzzle axes.

The current revision fixes shotgunner upper-shoulder vertices incorrectly
assigned to the head by a height-only mask. A spatial mask excludes the lateral
T-pose arms. Central strap weights exclude arm influences, and the measured
rocketeer backpack region stays rigid on the chest. Walk stills proving the
corrections live under `build/animation/infantry/walk-v3/fit-stills`, including
an actual rear view of the backpack.

Each body-only GLB has one source-preserving skin and fourteen clips. The raw
weapon remains separate: attach it with the exported socket delta
`posed_socket_world * inverse(rest_socket_world)`, in the rig's parent space.
Do not apply the fit transform again. `socket_primary`, `socket_support`,
`socket_stock` and muzzle/tube markers are already expressed in raw weapon local
coordinates. The editable .blend uses the same delta through a rigid skin group.

Independent validation passes all three exports: exact source topology/UV/albedo,
sub-micrometre source position preservation, closed UV seams, key/half-key linear
skinning, floor clearance, grip-marker coincidence, loop/transition endpoints and
current input hashes. Reports are in `build/animation/infantry/reports`.
These checks cover the other retained clips structurally; aiming, firing and
capture art have not received final visual acceptance in this walk-focused pass.

The normal-speed walking chapter is included in
`build/animation/walk-v3/human-resources-improved-walks.mp4`. All nine characters
appear with the jeep and tank in its final chapter. This is offline review
evidence. Native equipment attachments, runtime aiming, blend transitions,
shipping promotion and target-device performance remain a separate integration.

During the coordinated native gear/profile gate, keep the six existing export
inputs/outputs frozen. No native source, shared cooker or root CMake edits were
made in this pass. The implemented producer concurrency boundary is documented separately in
`ANIMATION_BUILD_CONCURRENCY.md`.


Current revision2 film: `build/animation/infantry/recoil-v2-review/infantry-equipped-review.mp4`.
The canonical `build/animation/infantry/producer-manifest.json` is the consumer
freeze contract. The producer and validator now participate in the shared
per-output transaction protocol and accept `--output-root`. Source weapon/body
meshes and the original six-character outputs remain unchanged.
