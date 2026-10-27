Painterly repeating albedo materials
==================================

Seven canonical final files:
  texture-grass/texture-grass.png
  texture-dirt/texture-dirt.png
  texture-soil/texture-soil.png
  texture-asphalt/texture-asphalt.png
  texture-pavement/texture-pavement.png
  texture-sand/texture-sand.png
  texture-wood/texture-wood.png

Each is 1024 x 1024, RGB, sRGB albedo only. Use REPEAT in U and V.
The wood grain runs along +U. Pavement deliberately has no mortar joints;
measured module geometry supplies those. The other materials have broad
brushwork and no required direction. Repeat density is an authoring choice;
integral endpoint UVs let adjacent measured modules meet in phase.

raw-v1.png is the preserved generated source, not a runtime texture.
tiled-3x3-qa.png and tiled-3x3-close-qa.png are authoring previews.
texture-repeat-overview.png collects the seven tiled previews.

The authoring export removes the image's nonperiodic low-frequency boundary
field with a periodic Poisson solve, applies compact cubic value/slope
correction, and enforces exact matching exported integer RGB edge values and
one-sided first differences. It does not mirror or paste a centre cross.
Asphalt additionally quilts genuine interior brushwork over the old Y-wrap
termination, using two closed minimum-error paths and a local feather. This
removes the visible horizontal hairline that edge equality alone missed; it
does not mirror paint or flatten the whole texture. The asphalt-only repair
is integrated in seamless_textures.py through repair_asphalt_tile.py. Its
pre-quilt master and provenance remain preserved.

All seven exported files have 0 maximum/RMS edge-value and first-difference
mismatch on both axes. The 3x3 previews were also visually reviewed at 256 px
and 512 px per tile, since numerical equality alone cannot establish quality.

Reproduce with the existing bundled Pillow/NumPy Python environment:
  python tools/seamless_textures.py
Focused regressions:
  python tools/test_seamless_textures.py

Per-material source hashes, final hashes, quantitative seam checks, downsample
diagnostics, corrections and visual notes are stored at:
  assets/production/records/texture-*/texture-audit.json

Authoring approval by itself does not qualify shipping or physical-device
rendering. The native development preview reads these canonical masters directly;
actual three-location GPU review captures are in
build/captures/seamless-material-check/. Device mip filtering, physical scale
and final owner art approval remain integration checks. These
authoring inputs/previews are excluded from shipping by the runtime allowlist;
only reviewed consumed files should be promoted to assets/runtime/.
