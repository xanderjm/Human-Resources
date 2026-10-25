# Shared walk revision

The current walk review is `build/animation/walk-v3/human-resources-improved-walks.mp4`.
It supersedes the original stiff walks and the v2 review. The 24-second film has
six seconds each of the original people, animals, equipped infantry, and all nine
bodies together in a test field with the moving jeep and tank. Playback is normal
speed at 30 fps, with physical source scales preserved. The film imports the
exported GLBs, rather than rendering the original rigging scene.

One humanoid performance drives farmer, resident, soldier and the three newer
infantry bodies. One four-beat performance drives cow, pig and sheep. Profiles
live in `assets/animation/walk-profiles.json`; the infantry recipe retains the
same human profile. Measured limb chains retarget stride and clearance. Both
performances loop in place; straight-line world speed is stride / duration.
Equipped infantry retain their two-handed carry pose over the shared leg/torso
gait. They are separate authoring candidates; the installed soldier is unchanged
by this author's infantry work.

`tools/animation/walk.py` owns offline two-bone leg IK. A planted foot moves
backwards at root travel speed; a continuous recovery arc returns it forward.
The human body now lowers after contact and rises over the supporting leg.
Relaxed shoulder swing, varying elbow flexion, delayed forearm movement, small
wrist follow-through, pelvic weight shift and counter-rotating shoulders replace
the fixed wrist track. Human heel contact and toe departure remain explicit.
Animals use hind-left, front-left, hind-right, front-right footfalls; v3 lengthens
the shared stride and lowers the hoof recovery arc. No species-specific walk
branches are used. Source anatomy limits the visible articulation of their
short, fused legs.

Contact height uses the full rigid foot surface, including bevelled boot edges.
A constant 2 mm sole envelope covers measured between-key interpolation sag;
it is not a per-frame root correction. The source geometry, UVs, materials and
triangle topology remain preserved. Other clips remain available, but this pass
focuses on walks and does not approve their art quality.

The newer infantry use measured T-pose rigs. A spatial head mask prevents the
shotgunner's shoulder vertices from following the head. Central straps cannot
follow arms; the rocketeer's measured backpack region is rigid on the chest.
Actual front and rear walk stills verify these corrections. The source hands are
open mittens; broad skinning folds do not provide articulated trigger fingers.
That remains a visible equipment-grip limitation.

## Checks and limits

`validate_shared_walks.py` independently skins the six exported original GLBs at
all keys and halfway between them, with quaternion interpolation. It checks
floor clearance, sustained stance, planted-foot drift after straight-line world
travel, swing clearance, knee articulation and lowered/swinging human arms.
The current maximum per-sample flat-stance drift is 0.215 mm; minimum skin height
is 1.125 mm above the reference floor. The report is
`build/animation/walk-v3/walk-validation.json`.

`validate_character_animation.py` passes all 60 original clips / 2,841 authored
frames, checking source preservation, weights, UV seams, floor bounds, loop ends,
beam transitions and authoring hashes. `validate_infantry_animation.py` passes all
three new exports, including every key and half-key, grip sockets, floor bounds,
UV seams, source topology/UV/albedo preservation and transition endpoints.
Neither numerical checks nor pose stills establish final animation art acceptance.

The nine-body field fixture validates full 30-second routes before rendering its
six-second excerpt. Curved-path foot locking, runtime blend quality and mobile
performance remain separate work. The native consumer supports the original
six; see `ANIMATION_RUNTIME_HANDOFF.md` and `ANIMATION_RENDERER_HANDOFF.md`.
This film is an offline review, not a native/mobile gameplay qualification.

## Reproduce

Use the offline Blender 4.5.15 LTS and existing NumPy/Pillow/FFmpeg authoring tools
listed in `CHARACTER_ANIMATION.md`:

```sh
HR_BLENDER=build/tools/blender/Blender.app/Contents/MacOS/Blender
HR_ART_PYTHON=/path/to/python-with-numpy-and-pillow
"$HR_BLENDER" --background --factory-startup --python-exit-code 1 --python tools/build_character_animation.py -- --validator-python "$HR_ART_PYTHON"
"$HR_ART_PYTHON" tools/validate_character_animation.py
"$HR_ART_PYTHON" tools/validate_shared_walks.py
"$HR_BLENDER" --background --factory-startup --python-exit-code 1 --python tools/build_infantry_animation.py
python3 tools/validate_infantry_animation.py
"$HR_BLENDER" --background --factory-startup --python-exit-code 1 --python tools/render_walk_review.py -- --group humans
"$HR_BLENDER" --background --factory-startup --python-exit-code 1 --python tools/render_walk_review.py -- --group animals
"$HR_BLENDER" --background --factory-startup --python-exit-code 1 --python tools/render_walk_review.py -- --group infantry
"$HR_BLENDER" --background --factory-startup --python-exit-code 1 --python tools/render_test_field.py -- --start 1 --end 180 --include-infantry --output build/animation/walk-v3/world
python3 tools/finish_walk_review.py
```

The close renderer supports `--only ID --sheet`, resumable `--start / --end`, and
`--check-only` to evaluate all body/weapon bounds against the labelled content
rectangle. The encoder decodes/counts 720 frames at 1280×720/30 fps and records
exact GLB hashes. The ZIP contains all nine GLBs, editable packed rigs, reports,
recipes and the film, outside shipping allowlists. Shared producer locking and crash recovery are implemented in
`ANIMATION_BUILD_CONCURRENCY.md`. Exact final source/export hashes are recorded
in `build/animation/walk-v3/producer-freeze.json`.
