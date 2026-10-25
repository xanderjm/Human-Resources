# Native style study 01

This is a development-only visual level study requested on 24 October 2026.
It renders existing textured models with SDL_GPU and offline HLSL, then records
the native GPU color target. It is independent of the foundation UI proof and
contains no playable capture, combat, progression or save rules.

The 30-second sequence cuts every ten seconds between sage farmland, slate
suburbia and mauve desert. The saucer moves, its outer ring rotates independently
of the fixed hull and fitted attachments, a cow rises into the beam, and scripted
mint effects/coral tracers show the intended visual hierarchy. The reference
HUD uses the accepted Beam UI font rasterized offline; this does not introduce
runtime text rendering. Audio is absent.

## View the results

- `build/captures/human-resources-style-study-01.mp4`: 720 × 1280, 30 fps, 900 frames; portrait framing for comparison with the reference frames.
- `build/captures/human-resources-style-study-01-landscape.mp4`: 1280 × 720, 30 fps, 900 frames; the same level in the product's planned orientation.
- Adjacent PNGs show each location at seconds 4, 14 and 24. Adjacent JSON/log files record the producer hash, content-manifest hash, decoded video properties and actual native backend.
- `build/evidence/macos-style-preview.json` and `build/evidence/style-preview-content.json` retain host/GPU and exact cooked-content evidence. Build outputs stay ignored.

Portrait is a reference comparison, not a change to the mobile orientation
contract. The landscape view deliberately exposes more of the side boundaries.

## Reproduce

Use the existing art Python with NumPy and Pillow, also used by the model/font
authoring tools. FFmpeg/ffprobe are host-only capture tools, not linked game
libraries. This pass used Pillow 12.3.0, NumPy 2.2.6 and FFmpeg 7.1.1. System
`python3` can still run the standard-library project gates.

```sh
ART_PYTHON=/path/to/python-with-existing-art-dependencies
"$ART_PYTHON" tools/capture_style_preview.py
"$ART_PYTHON" tools/capture_style_preview.py --landscape \
  --output build/captures/human-resources-style-study-01-landscape.mp4
```

The helper cooks content, configures optimized `dev`, builds the explicit
`human_resources_style_preview` target, records GPU frames and validates the
encoded frame count and dimensions. On POSIX hosts it streams through a FIFO;
on Windows it uses an intermediate raw file. The native viewer supports raw
capture on all desktop build profiles; only macOS Metal was executed here.
A desktop session with window/GPU access is required.

For live looping playback after cooking/building:

```sh
build/dev/human_resources_style_preview --content build/style-preview/content
```

Escape or closing the window exits. Playback pauses its clock when unfocused;
offline capture advances exactly 1/30 second per recorded frame. The camera,
placements, assembly offsets and visual choreography live in
`native/human_resources_presentation/src/style_scene.cpp`. They are presentation
fixtures, not runtime balance rules or an approved level schema.

## Art and import decisions

The cooker consumes 44 source models and generates terrain/effect primitives and
HUD images. Some scenery sources are still pending production review. These
copies are preview candidates, with their status and source digests retained in
the cooked manifest; this pass does not approve or overwrite source models.
Raw scenery candidates receive measured preview scale/ground pivots. The
windmill rotor and roadside canopy need a quarter-turn before scaling. The
jeep wheel has a centered pivot and is positioned 0.3 metres above the ground.

The canonical corrected repeat masters are `assets/textures/texture-*/texture-*.png`.
The cooker reads these directly, checks the 1024px edges and records their hashes.
`art/material-studies/` holds matching review copies with provenance. Earlier
white-backed swatches and preserved `raw-v1.png` inputs are not cooking sources.
Repeat addressing uses the full corrected tile; there is no mirroring or cropping.
Separate U/V repeat counts preserve square world-space texels on long roads and
pavements. Exported pixel/gradient checks, 3×3 visual proofs and actual measured
module joins are recorded in `assets/production/records/texture-*/texture-audit.json`
and `assets/production/measured-modules/`. Native mip filtering, brush density and
physical-device appearance still need target-specific review.

For a fresh native three-location texture check, run the existing art Python on
`tools/capture_texture_check.py`. It cooks the current masters, builds the explicit
development viewer and saves actual SDL_GPU frames and source-bound evidence to
`build/captures/seamless-material-check/`.

Models are cooked into the bounded little-endian HRM1 preview mesh format:
position, normal, atlas UV, base-color factor, emission and rotor membership;
32-bit triangle indices; one RGBA8 atlas. Unsupported sparse/morph/animated
content is not a runtime feature. GLB hierarchy transforms are resolved offline;
rotor membership preserves the authored `Outer_Ring_Pivot` split. The shader
rotates it around its rest pivot `[0, 0.35, 0]`. Fitted hull modules stay fixed.

The native renderer uses sRGB albedo, a warm key/cool fill, a 2048² depth shadow
map with comparison sampling and a 3 × 3 filter, four-sample antialiasing, and
transparent mint effects. GPU resources and capture readback stay in the
renderer. No renderer-owned gameplay state or new native library is introduced.

## Ownership and next pass

The preview target and its shaders are excluded from ordinary builds/packages.
`assets/runtime/` is unchanged. The shared offline shader builder accepts separate
source/output roots, so development shaders do not enter the shipping shader
allowlist. Imported/cooked data lives under `build/`; no generated native project
or shader binary is a source authority.

This pass tests composition, scale and palettes. The next visual iteration should
review texture-edge continuity and ground contrast at gameplay scale, larger
warm target silhouettes, a less regular street/rock boundary, environment density,
more readable capture glow, modular vehicle fits and moving character poses.
Only after review should consumed assets enter the runtime allowlist and the
mesh path join the product client. Device budgets, real mobile execution, input,
accessibility, game rules and final art acceptance remain separate work.
