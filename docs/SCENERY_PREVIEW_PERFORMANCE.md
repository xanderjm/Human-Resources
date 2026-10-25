# Scenery preview performance checkpoint — 25 October 2026

The owner prioritized optimization and viewport correctness over further art
iteration. This pass is finished editing renderer/client interfaces. Structural
reference matching remains below target; the scenery goal is not complete.

## Corrected behavior

- Live preview defaults to **390×844 logical points**, fitting smaller desktop
  work areas proportionally. Retina backing pixels are independent. Live targets
  fit the current drawable within 1280×1280; the camera uses its actual aspect.
  Resize updates the targets/camera, final presentation preserves aspect, and
  camera scale is continuous across square windows. Capture pixels stay separate.
- Opaque meshes batch in groups of 64. Model bounds are culled, with a larger
  margin for offscreen shadow casters. Alpha effects retain draw order. Existing
  four-sample antialiasing and 2048² shadows remain.
- Adopted Midnight Menu's retained completion-fence lifetime and cooperative
  waits for two frames in flight. The pinned Metal backend otherwise spins;
  releasing a capture fence before command cleanup can recycle its completion
  flag too early. No dependency pin or sibling build dependency was added.
- One application deadline paces live playback at 60 fps. Immediate presentation
  is requested where supported, with vsync fallback. The rejected intermediate
  combination of vsync and a CPU deadline produced 32–33 ms outliers.
- Optimized builds disable GPU validation; Debug keeps it. Only the selected
  theme loads: 17 assets for farmland/suburbia, 16 for desert. A 256-value sRGB
  table replaces per-channel/per-texel powers without changing the tile mean.
- Captures use a hidden offscreen GPU target, without acquiring a drawable or
  taking focus. Ordinary live playback performs no readback or encoding.

## Actual native evidence

macOS / Apple M2 Max / Metal, optimized dev, seed 514. Phone tests use 390×844
points and 591×1280 render pixels; short tests exclude 30 warm-up frames and
measure 300 further frames. These are process/driver timings, not GPU timestamps.

| Theme | Mean frame interval | 95th percentile interval | Mean world + shadow draws |
| --- | --- | --- | --- |
| Farmland | 16.667 ms | 16.722 ms | 36.46 |
| Suburbia | 16.667 ms | 16.721 ms | 30.51 |
| Desert | 16.668 ms | 16.826 ms | 27.00 |

Reports: `build/evidence/scenery-paced-{farmland,desert}.json` and
`scenery-paced-phone-final.json`. The longer live farmland test renders 1,800
frames over 30 seconds, crossing section joins: 1,770 measured intervals average
16.667 ms, with a 16.685 ms 95th percentile. Its native/time log records 30.21
wall seconds, 1.28 user + 0.59 system CPU seconds, 104,120,320 bytes maximum RSS
and 189,940,600 bytes peak memory footprint. This is one bounded run, not proof
of indefinite memory stability. See `scenery-long-live.json` and the adjacent
`build/style-preview/scenery-long-live.log`.

The original completed-frame offscreen benchmark averaged 1.456 ms. Instancing
alone measured 1.337 ms; the final diagnostic measures 1.713 ms including
cooperative polling and an explicit GPU drain every frame. The final diagnostic
trades polling latency for yielding CPU; do not claim improved raw throughput
from it. Ordinary live playback keeps two frames in flight. Warm-cache selected
theme initialization logs measure approximately 21–39 ms, with no cold-start or
device-loading guarantee.

Native resize smoke observes portrait, landscape and square sizes, then returns
to portrait (bitmask 7), including launch from `/private/tmp`. Headless tests cover
proportional fit, camera continuity, visibility/partial visibility, shadow margins
and exact per-theme instance/surface preloads. Aggregate verification passes all
12 suites; SDL-free ASan/UBSan passes all 11. Logs remain under
`build/style-preview/performance-final-*`.

The proof clip is `build/captures/scenery-phone-60fps.mp4`: eight seconds,
480 decoded frames, 780×1688 at 60 fps, visually inspected. Adjacent JSON/logs
record the exact producer/content hashes. It uses native readback without
interpolated frames. This is a macOS phone-shaped preview, not iPhone execution.

## Launch

```sh
scripts/run-scenery.command
scripts/run-scenery.command --scenery suburbia
scripts/run-scenery.command --scenery desert --window-width 844 --window-height 390
```

The launcher builds optimized dev. Missing cooked content produces an explicit
cook instruction. `--width`/`--height` select capture/benchmark pixels;
`--window-width`/`--window-height` select logical points. Escape closes the window.
The capture helper accepts `--phone --fps 60` and rejects a movie if its executable
changes during recording.

## Integration handoff

Owned files: `style_renderer.cpp/.h`, `style_resources.cpp/.h`,
`preview_viewport.h`, `style_world.hlsl`, preview-only `style_preview.cpp`,
`style_scene.cpp/.h` visibility math, `scenery.cpp/.h` preload/camera functions,
scenery tests, the capture helper and launch script. No new gameplay/progression
or other client files were edited. Renderer edits from this pass are complete.

`StyleRenderer::initialize` accepts an optional preload-name span; empty means
the legacy film/HUD set. Scenery passes its theme's list. A playable consumer
adding actors/UI must preload its full consumed set. `resize` changes GPU targets;
`render(frame,capture,present)` permits offscreen capture; `wait_idle` and `stats`
support diagnostics. The vertex shader now uses two uniform buffers: 144-byte
scene data at slot 0, and 64×112-byte instance data at slot 1. Preserve that
CPU/HLSL layout and the retained-fence lifetime during integration. No shipping
client, asset allowlist or physical-device qualification was changed by this pass.
