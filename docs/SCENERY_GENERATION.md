# Scrolling scenery study

This development-only study generates scenery for farmland, suburbia and desert
in the native SDL_GPU viewer. It has no gameplay actors, HUD, progression or
runtime packaging changes. Visual reference matching is still in progress.

## Composition authority

`native/human_resources_presentation/src/scenery_compositions.cpp` owns authored
64-metre sections. Each section coordinates road shape and primary landmarks;
the theme owners construct fences, gardens, vegetation and cliff groups around
those anchors. Section zero is a fixed reference-comparison fixture. Other
sections choose one of three compositions per theme from the seed and absolute
section index. This small library is an initial calibration set; it is not yet
enough variety for a finished endless environment.

The earlier independent noise/regular-lot approach produced evenly spaced edge
decoration. It was rejected during visual review because it did not reproduce
the references' asymmetric masses, clearings and composed street frontages.
Authored sections provide direct control over these forms. Randomness is kept
to secondary details rather than moving the main landmarks independently.

Current native captures in `build/captures/scenery-authored-1/` remain below the
reference target: houses are mostly cropped outside the frame, tree masses are
too small, the suburban road occupies too much screen width and desert cliffs
need stronger inward projections. Next, calibrate the fixed camera and measure
reference silhouettes/anchor positions in screen space before extending the
composition library. Passing placement tests does not establish visual fidelity.

## Scrolling boundary

`generate_scenery` consumes a theme, seed and double-precision absolute distance.
It constructs only the nearby sections and rebases prop positions around the
camera. Stable identities derive from section, site and part, before culling.
The scene does not accumulate previous sections or wrap a short animation.
Roads are triangle ribbons sampled on an absolute half-metre grid. Authored
road endpoints agree between sections; world-anchored UVs rebase by whole tile
repeats. Source tiles use ordinary repeat wrapping, with no mirroring/cropping.
The scenery shader reduces sampled texture contrast toward the tile's linear
mean without modifying the source image.

The SDL-free presentation owner returns instances and surface triangles. The
development renderer owns bounded GPU uploads, preloaded meshes/materials,
shadows, antialiasing and readback. Captures now use a hidden offscreen GPU target
without a display drawable; a 480-frame phone capture passes. See
`SCENERY_PREVIEW_PERFORMANCE.md` for corrected sizing, batching, pacing and native
measurements. Reference-layout acceptance remains separate.

## Review and verification

Use the existing art Python (Pillow/NumPy) and host FFmpeg tools:

```sh
cmake --build --preset dev --target human_resources_style_preview
"$ART_PYTHON" tools/review_scenery.py --output build/captures/scenery-review --times 0 18 37
"$ART_PYTHON" tools/capture_style_preview.py --scenery farmland --seed 514 --seconds 30 --skip-build --skip-cook
build/dev/human_resources_scenery_tests build/evidence/scenery-generation.json
python3 tools/project.py verify
```

The viewer scrolls at 2.4 metres/second. Review accepts explicit seeds, stations
and landscape framing. The film helper publishes a movie only after native
success and decoded frame-count/dimension verification. Portrait is a reference
comparison; it does not change the product's landscape orientation contract.

The headless scenery suite exercises 1,728 windows over 48 seeds and all three
themes, including section seams, UV rebasing and 100-million-metre positions.
It checks bounded geometry/population, stable scrolling identities, finite
transforms, preloaded materials and road clearance for tree trunks/buildings.
Only macOS Metal has rendered this study. Device performance, other platforms,
landmark overlap, visual seam acceptance and final art acceptance remain open.

## Lightweight prefab assembly experiment

`tools/scenery_study/` is a separate offline composition experiment for reviewing
farmland prefab shapes before changing the native generator. It uses Python's
standard library and existing source-asset dimension proxies; it does not load
GLBs, change asset approval, package content or consume balance data. It is not
a second runtime tuning authority. Its metres and camera scale are proposals.

```sh
python3 tools/test_scenery_study.py
python3 tools/stress_scenery_study.py
```

The default batch checks 256 seeds across 32 consecutive absolute sections each,
including negative indices. Six authored arrangements place two to four groups
per section from barn, crops, grove, silo, windmill and fence families. Each
six-section block uses all six arrangements, with shuffled interior ordering
and no adjacent duplicate. Landmarks keep restricted orientations; secondary
footprints accept small rotations. Right-hand groves use shallower authored
footprints rather than scaling source meshes. Track ports share position,
tangent and width, and conservative curve bounds constrain nearby groups.

Checks cover conservative convex prefab-envelope overlap, protected-field
intrusion, source-sized prop proxies exceeding envelopes, road clearance,
section/world bounds and continuous path joins. A one-metre cross-section scan
records the narrowest observed clearing; the protected 24-metre central band is
also checked against entire envelopes. Focused tests inject broken seams,
overlaps, field/path intrusions and overhangs to check that the validators fail.
They check stable identities/transforms between overlapping camera queries,
negative positions, bounded windows and 100-million-metre streaming.

Results and exact source hashes are written to
`build/evidence/scenery-prefab-study.json`. A bounded visual snapshot at
`build/scenery-prefab-study/gallery.json` contains sixteen six-section runs,
including the narrowest batch case, with one buffer section at either end for
scrubbing. The conversation diagram consumes those same checked snapshots.
Its overview compresses section lengths; its landscape window preserves equal
scale on both axes. Shapes and circles are schematic asset proxies.

Passing these geometric checks does not establish reference matching. The
small arrangement library, two-metre end margins, six-section block structure,
fence endcaps and landmark cadence can still reveal repetition. Native camera
projection, canopy/shadow occlusion, real GLB bounds, curved fence connections,
scene density, mobile budgets and final visual acceptance remain to be tested
when an approved layout is translated into the existing presentation owner.
