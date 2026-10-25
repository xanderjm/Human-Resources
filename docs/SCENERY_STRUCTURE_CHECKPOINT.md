# Farmland structure checkpoint — 25 October 2026

One broad scenery-only revision follows the completed preview performance pass.
Renderer, resources, product scene/client and build integration remain with their
integration owners. This checkpoint is not reference-layout acceptance.

The product scene was overriding the generator's 20.5m portrait camera with a
31m width, reducing the approved UFO from the reference's approximately 16–17%
screen width to roughly 9.4%. The coordinator accepted a coordinated candidate:
20.5m portrait camera and player field ±8.25m. Those changes are owned elsewhere;
scenery does not change gameplay limits.

`scenery_farmland.cpp` now uses eight asymmetric banks rather than twelve
isolated three-tree groves. Five overlapping crowns per bank occupy roughly
10–14m of scrolling length, with varied lateral depth and explicit barn/crop/
silo/windmill openings. The barn projects farther into view. Short fence runs
follow paddock/landmark openings rather than forming two long parallel lines.
Central decorative tree islands were removed.

Every rotated crown leaves the central 13m band clear, using measured cooked XZ
bounds when placing trees. Trees near a trail bend move outward if there is no
room on its inner verge, rather than being squeezed into the playable middle.
Understory, bales and the left crop plot move to the banks; the bottom-left fence
no longer projects into the centre. No asset or texture regeneration is involved.

The optimized native scenery suite passes 1,728 seeded windows over all themes,
including scrolling/identity/road joins and the independent rotated-crown
clearance check. Maximum observed population is 577 props and 4,608 streamed
surface vertices, compared with 589 props before this pass. Evidence:
`build/evidence/scenery-structure-windows.json` and
`build/style-preview/scenery-structure-build.log`.

Pending acceptance evidence: a native portrait comparison from the relinked
playable client, with actors at the accepted camera/player scale. Capture should
use a separate `build/captures/scenery-structure-game/` directory after the
coordinator's active gameplay profiling/build window. Do not accept the scene
from the earlier 31m-camera captures or from headless tests alone. No additional
detail iteration is planned before that broad comparison is reviewed.

The first playable capture completed 480 decoded frames at 588×1280/60 fps in
`build/captures/scenery-structure-game/farmland-banks.*`. It is diagnostic only:
the executable remained stable but unrelated client/interface/tool sources
changed during capture, and the coordinator then identified an actor/world Z
mapping defect in the product scene. Scenery uses `distance-along`; actors used
the opposite sign. The scene owner is fixing that mapping. Do not use this film
to judge actor distribution, motion, final bank density or release fidelity.
Scenery sources are frozen pending one recapture from the corrected binary.

The corrected capture now passes: `farmland-banks-axis-fixed.mp4`, 480 native
frames, 588×1280 at 60 fps, with unchanged executable and complete source snapshot
before/after. Visually inspected its middle and late run frames. The comparison
`farmland-structure-comparison.png` places the original reference beside the
native playable frame at equal displayed width, preserving both aspect ratios.
The actor coordinate fix is supplied by the product scene owner. Banks now
overlap into substantial edge silhouettes while the play field remains readable.

The capture uses a 0.05-second fixed simulation step, so its eight-second movie
shows approximately 24 seconds of simulation. It is accelerated structural
review, not live pacing evidence. Producer/source inventory and full decode are
in its adjacent JSON; `structure-checkpoint.json` records the frozen scenery
hashes. This is the single broad comparison requested by the coordinator; no
further detail tuning precedes review. Remaining observations for other owners:
target animals read smaller than the reference relative to the saucer, and
outer-field occlusion/telegraph readability still needs an explicit gameplay
check. No actor scale, renderer or gameplay changes are made here.
