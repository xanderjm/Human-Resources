# Native visual assets

Rebuild with `node tools/build_native_graphics.cjs` using the existing authoring-only Sharp installation. SVGs are editable vector masters for this edition; preserve hand edits in the generator before rebuilding. PNGs are transparent sRGB/straight-alpha exports. All assets are original project-owned vectors without external images, text or font dependencies.

Each folder includes usage and sampling contracts in `asset.json`. Only the laser strip repeats its U axis. Its opposite edge RGBA values and first differences are exact matches. The other effects, icons and cards are isolated clamp sprites; road markings and capture dots repeat by instancing with transparent spacing. A terrain tile must never be substituted with one of these isolated sprites.

XP trough and mint fill are independent files. Fill is clipped to progress by the future UI owner. HUD/card corners require nine-slice handling. Crop-circle RGB is white and alpha is a tintable linear mask; its white-background reference alone is olive tinted for visibility. The shield includes an uncompiled HLSL material helper and unit-sphere GLB: its 2D image is only a billboard fallback, with no UFO baked in.

These are authoring/production inputs, outside the runtime shipping allowlist. Runtime colour, blend, mip filtering, depth/occlusion, HUD placement and physical-device readability remain integration checks.
