# Shared-world walking and vehicle film

The 24 October 2026 follow-up puts all six exported character rigs into one
farmland test area with the assembled jeep and tank. The 30-second landscape
film is an offline Blender visual fixture, independent of native gameplay and
the abstract balance simulator. No shipping content or source model is replaced.

Outputs under ignored `build/animation/test-field/`:

- `human-resources-test-field.mp4`: 1280×720, 30 fps, 900 frames, silent.
- `review-*.jpg`: labelled stills from the wide view, walking pass and vehicle pass.
- `test-field.blend`: packed scene setup snapshot, posed at time zero. The Python
  renderer owns path motion and camera choreography; the snapshot does not bake
  vehicle/placement curves onto the Blender timeline.
- `scene-report.json`: source hashes, producer hash and route validation.
- `film-validation.json`: decoded film properties, digest and route distances.
- `frames/`, `render-*.log`, `capture.log`: reproducible capture intermediates.

The film begins with a broad view of the complete cast, cuts closer to walking
animals/humanoids at ten seconds, then shows the patrol loop from twenty seconds.
All eight actors are labelled when their projected label lies inside the image.
The wide view establishes actual physical scale; bodies are not normalized to a
common display height as in the earlier individual animation showcase.

## Source ownership

`assets/animation/test-field.json` owns this review fixture's duration, actor
routes and vehicle speeds. `tools/animation/test_field.py` owns distance-based
paths, camera shots and spatial validation. `tools/render_test_field.py` composes
the existing exported walking clips, scenery, terrain and reviewed assemblies.
`tools/capture_test_field.py` owns resumable rendering, labels, encoding and
decoded-video checks. None is a runtime movement, collision or level schema.

Characters travel at the stride/duration speed recorded by the rig recipe;
world displacement and gait time advance together. Root orientation follows the
path tangent. Six nonintersecting inner routes keep characters apart. The jeep
and tank follow a separate outer oval at constant arc-length speed.

`assets/production/assemblies.json` supplies the exact vehicle and headwear
placements. Import wrappers preserve source metres/+Y-up/+Z-forward before the
single Blender coordinate conversion. Hat/cap transforms follow the head bone
using the reviewed rest fit. The jeep chassis sits 0.12m above its authoring root,
wheel axles are 0.30m above ground, left hubs face outward, wheels roll by travel
distance/0.30m, and front sockets steer with path curvature. The tank turret uses
its reviewed mount and scans independently. Its tracks remain the fused source
geometry; this pass does not animate individual tread links or add physics/crew.

The field uses the full canonical grass/soil tile masters with world-proportioned
repeat UVs. No crop, mirror, paint-over or material-source alteration is applied.
Existing barn, silo, hay, tree, bush, rock and grass-tuft assets establish a small
world around the open walking/vehicle area. Sparse fixture fence/road marks are
procedural review scenery; no new external content dependency is introduced.

## Reproduction and checks

Use the same Blender 4.5.15 LTS and existing NumPy/Pillow/FFmpeg authoring tools
recorded in `CHARACTER_ANIMATION.md`:

```sh
ART_PYTHON=/path/to/python-with-existing-art-dependencies
"$ART_PYTHON" tools/capture_test_field.py
```

`--blender /path/to/blender` selects another local installation. Rendering runs
in three 300-frame processes; a focused range can be resumed directly with
`render_test_field.py -- --start FRAME --end FRAME`, then assembled using
`capture_test_field.py --encode-only`. `--still FRAME` renders a review image.

The route validator samples all 901 simulation endpoints, rejects intersections
using actor footprint circles, checks finite headings, and compares travelled
distance with each character's stride speed. The capture helper decodes every
encoded frame with ffprobe and verifies 900 frames, dimensions, rate and duration.
Visual still checks cover clothing fit, walking poses, wheel/turret fit, framing
and label placement. `python3 tools/project.py verify` remains the repository gate.

This is motion and scale review evidence. Foot sliding, blends, steering/collision
rules, native skeletal playback, vehicle physics and device performance still
belong to the real game consumer and its tests. No platform/runtime qualification
is inferred from Blender rendering.
