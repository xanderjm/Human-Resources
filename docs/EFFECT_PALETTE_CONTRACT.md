# Procedural effect colour contract

The world shader consumes two distinct colour conventions. Imported albedo is
sRGB texture data, automatically decoded by its sRGB GPU texture format;
imported vertex/material factors and ordinary instance tint remain linear.
Explicit `StyleInstance::effect` instances use the UI's sRGB-authored RGB palette.
Their alpha is coverage and receives no colour transfer.

Current explicit effects use the white `cube`, `ring` and `beam` carriers. Do not
mark a painted GLB as an explicit effect to obtain coloured emission. Imported
material emission remains the existing vertex flag and retains its existing
approximation; proper RGB emissive materials are a separate capability.

`params.y` already carries the explicit effect flag. The vertex shader decodes
its palette exactly once, leaving linear vertex factors intact. The fragment
shader gives this path a unity unlit response and exact sRGB output. It returns
before shadow sampling. The second varying flag distinguishes this path from
the imported emission flag; no CPU uniform, resource binding or buffer layout
changes are required. Runtime and development-preview shader copies agree.

Ordinary albedo, normal lighting, shadows, terrain detail, alpha blend state,
4x MSAA, draw batching and render passes are unchanged. Source art is unchanged.
This correction adds no exposure/saturation grade or bloom. The existing UNORM
output still composites alpha in display space; this change preserves that
behavior and does not claim a fully linear compositing pipeline.

## Native evidence — 25 October 2026

`native/tests/effect_palette_smoke.cpp` is an optional native GPU fixture, like
the separate animation render smoke. It uses the production `StyleRenderer`
and cooked white cube, with six opaque effect swatches, six half-coverage
swatches and six lit opaque controls. Compile it against the optimized renderer
and its existing dependencies; it is intentionally outside the headless CTest
suite and requires native window-service access.

Run identical fixture binaries from separate directories containing before and
after cooked shader packages under `assets/shaders/`, so no shared application
or shader file is replaced during capture:

```sh
env SDL_ASSERT=abort build/evidence/effect-palette/before/fixture build/dev/assets/runtime/game build/evidence/effect-palette/before.rgba
env SDL_ASSERT=abort build/evidence/effect-palette/after/fixture build/dev/assets/runtime/game build/evidence/effect-palette/after.rgba
python3 tools/check_effect_palette.py build/evidence/effect-palette/before.rgba build/evidence/effect-palette/after.rgba --output build/evidence/effect-palette/palette-report.json
```

The isolated before shader is preserved in the evidence directory; it is not a
second source authority. The checker reads actual native RGBA, rather than
estimating the shader output. The current Mac Metal fixture reports:

| Palette | Before RGB8 | After RGB8 | Authored RGB8 |
| --- | --- | --- | --- |
| Capture ring | 227,255,255 | 127,255,194 | 128,255,194 |
| Capture trail | 177,255,255 | 74,245,181 | 74,245,181 |
| Coral warning | 255,174,160 | 255,71,59 | 255,71,59 |
| Missile trail | 255,163,123 | 255,61,33 | 255,61,33 |
| Laser core | 237,255,255 | 140,237,255 | 140,237,255 |
| Low values | 53,72,93 | 5,10,18 | 5,10,18 |

All six opaque palettes are within one code value; transparency within one.
The entire lit opaque control band is byte-identical, including MSAA edges.
Both draws report two world draws and 216 triangles. Shadow/AA/pass setup is
unchanged; the fixture intentionally has no shadow casters. Actual gameplay
shadows and combined effect geometry remain subject to the coordinated film.

Both shader packages compile for native macOS Metal and SPIR-V/reflection;
`python3 tools/project.py verify --static-only` passes. Exact source, binary,
shader and RGBA hashes are under `build/evidence/effect-palette/`. This is a colour
contract check, not gameplay reference acceptance, frame timing or iPhone
qualification. The release coordinator owns the combined aggregate checkpoint
and shared development-log entry.
