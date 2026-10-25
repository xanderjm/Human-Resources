# Character rigs and animation

The current walks supersede the original gallery: see
[`SHARED_WALKS.md`](SHARED_WALKS.md) for the shared human/animal performances,
contact checks and the revised close-up/test-area film.

For all six characters walking together with the jeep and tank in a shared
farmland area, see [`TEST_FIELD_VIDEO.md`](TEST_FIELD_VIDEO.md).

The 24 October 2026 owner request covers all six existing animal/humanoid bodies:
cow, pig, sheep, farmer, resident and soldier. The source GLBs and production
inventory remain the modelling authority. Separate straw hats and nightcaps are
accessories, not additional bodies; humanoid rigs include a `socket_head` joint.

`assets/animation/characters.json` owns measured rig landmarks, clip timing,
loop policy, stride distances and the per-character behaviour selection.
`tools/animation/rig.py` owns skin construction; `motion.py` owns the authored
motion curves. This is offline content authoring, separate from gameplay, the
balance simulator and the development style viewer. No runtime library, new
gameplay rule or shipping asset promotion is introduced.

## Delivered content

All generated artifacts live under ignored `build/animation/`:

- `exports/`: six self-contained, textured, skinned GLBs with ten named clips each.
- `blend/`: six packed, editable rigs with named actions and idle selected.
- `reports/`: source/tool hashes, clip durations/strides and independent playback checks.
- `human-resources-character-showcase.mp4`: silent 1280×720, 30 fps film, showing every clip on every actor.
- `showcase-poster.jpg`: all six characters; `showcase/`: labelled clip stills and rendered frames.
- `human-resources-character-animation.zip`: GLBs, editable files, reports, film and handoff documentation.

Generated exports are reproducible derivatives, not hand-edited authorities.
Keep the delivery ZIP outside disposable build storage when archiving the work.

| Character | Specific behaviour | Movement intent |
| --- | --- | --- |
| Cow | `graze`: slow head dip, ear twitch, tail swish | Heavy four-beat walk and diagonal escape trot |
| Pig | `sniff`: snout dip and searching head sweep | Short quick steps and urgent trot |
| Sheep | `nibble`: smaller repeated head nods and ear motion | Light four-beat walk and escape trot |
| Farmer | `shoo`: wave an empty hand toward the saucer | Relaxed walk and fleeing run |
| Resident | `yawn`: hand toward face and sleepy stretch | Slow, short shuffle and restrained escape run |
| Soldier | `signal`: raised arm and scanning head | Brisk patrol walk and fleeing run; no invented weapon |

Every actor also has `idle`, `walk`, `run`, `alert`, `startle`, `hit_react`,
`beam_lift`, `beam_struggle` and `beam_release`. `idle`, locomotion, specific
behaviour and struggle loop. Other clips are one-shots that return to neutral,
except lift, which ends at the suspended struggle pose. Hit reaction is a visual
flinch; it does not establish damage, stun or death rules.

## Integration contract

- Metres, +Y up and +Z forward in GLB. Grounded root at origin in bind pose.
  Blender uses +Z up/-Y forward internally; the exporter converts once.
- Humanoids have 21 joints including root and headwear socket; quadrupeds have
  20 including root, ears, tail and three joints per leg. Hands/hooves are rigid
  stylized masses; the source geometry has no articulated fingers or mouth.
- At most four normalized influences per vertex. Ordinary linear blend skinning;
  no Blender-only constraints, drivers, procedural modifiers or dual-quaternion
skinning required for playback. All curves are baked at 30 Hz, starting at zero.
- Locomotion is in place. Reports contain stride distance and exact duration;
  nominal translation speed is stride divided by duration. Actual gameplay
  movement and animation blending belong to future runtime integration.
- Beam lift raises the root by 0.55m and ends at struggle phase zero. Struggle
  loops at that height. Release begins in that pose, descends, cushions landing
  and returns to neutral. Actor placement remains owned by future presentation.
- The film normalizes body display heights to 1.5m for readable comparison.
  Exported physical scales are unchanged; height labels state actual metres.
- Hats remain separate. `socket_head` supplies an attachment point; fitting,
  detachment and capture choreography for those props remain a consumer concern.
- No facial blendshapes, finger articulation, lip sync, ragdolls, death animation,
  weapon handling or runtime animation player is implied by these files.

Humanoid heat weights are solved on a welded temporary proxy, transferred to
the unchanged source mesh, limited to four influences and reconciled across UV
splits. Disconnected trim uses anatomical binding when the surface solve has no
weight. Faces/helmets stay rigid on the head. Quadrupeds use measured anatomical
regions and fitted feet, with narrow blended joins. Walk contact is solved
before baking without moving the root afterwards; other clips sample the
deformed mesh and raise below-floor poses before baking.

## Reproduction

Use Blender 4.5.15 LTS, Python with the repository's existing NumPy/Pillow
authoring dependencies, and FFmpeg. Example with the local portable copy:

```sh
HR_BLENDER=build/tools/blender/Blender.app/Contents/MacOS/Blender
"$HR_BLENDER" --background --factory-startup --python-exit-code 1 --python tools/build_character_animation.py
python3 tools/validate_character_animation.py
"$HR_BLENDER" --background --factory-startup --python-exit-code 1 --python tools/render_character_showcase.py -- --group animals
"$HR_BLENDER" --background --factory-startup --python-exit-code 1 --python tools/render_character_showcase.py -- --group humans
python3 tools/finish_character_showcase.py
ffprobe -v error -count_frames -show_entries stream=width,height,r_frame_rate,nb_read_frames -show_entries format=duration -of json build/animation/human-resources-character-showcase.mp4
python3 tools/project.py verify
```

`--only ID` rebuilds one body. Renderer options `--still FRAME` or
`--start FRAME --end FRAME` support focused review/resuming a capture. The final
render uses re-imported exported GLBs, not the authoring scene. Frame numbering
is shared with `tools/animation/showcase_timeline.py`. Re-render both groups
after modifying source, weights, motion, lighting or framing.

Portable Blender is an **authoring-only** dependency: official 4.5.15 arm64 DMG
from the [Blender release directory](https://download.blender.org/release/Blender4.5/),
SHA-256 `bfbc0bda099d1ab7aef0992a584f872f58bed9bb491210ad45f9ca071aa2e688`,
311,921,946 download bytes, GPL licensed. It is stored in ignored build tools,
adds zero bytes to game packages and is replaceable by another glTF exporter
passing the independent skin/clip checks. The repository's content-tool owner
owns updates. Blender supplies bone-heat solving, editable rig files, glTF
import/export and rendering; the native stack does not provide those authoring
capabilities. No sibling checkout or active Blender MCP connection is needed.

## Verification and limits

`validate_character_animation.py` independently decodes GLB animation channels,
joint hierarchies, inverse bind matrices and vertex influences with NumPy. It
checks all 2,841 authored character frames across 60 clips: finite skinning,
normalized influences, valid joints/indices, preserved source geometry and
triangle counts, self-contained textures, floor contact, matching loop ends,
closed UV seams and both beam transition boundaries. Input hashes reject stale
exports. Rendered stills additionally check silhouette and deformation.

These are stylized first-pass animation assets. Structural checks and the film
do not establish native skinned rendering, gameplay timing, collision/IK,
mobile performance, target-device readiness or final production art acceptance.
Review animation blends and foot sliding at actual runtime movement speeds.
The native consumer now plays these skeletal clips; see
`ANIMATION_RUNTIME_HANDOFF.md` and `ANIMATION_RENDERER_HANDOFF.md` for source-bound
evidence, ownership and remaining idle/run art issues. The static-only preview
mode remains available without required HRA files.
