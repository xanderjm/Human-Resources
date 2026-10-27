MEASURED SCENERY MODULES — AUTHORING REVIEW

Rebuild with the bundled numpy/Pillow Python:
  python tools/build_measured_modules.py
  python tools/build_measured_modules_test.py

This tool creates twelve measured GLBs, isolated white-background native renders,
per-asset model/module audits, the labelled review contact sheet and assembled
surface QA images. No Tripo credit is consumed. These remain production-source
artifacts, outside assets/runtime and shipping allowlists.

All models use metres and +Y up. Geometry lies on Y=0 unless it is a solid
pavement/fence element. Exact local placement origins and connection mouths are
in each module-audit.json and embedded GLB asset.extras.ports. Use those mouths,
not an assumption that every mesh has a centred bounding box.

Surface placement: keep the authored plane atY0; place path/road overlays a
few millimetres above ground in the scene to avoid coplanar z-fighting.
Ground: X=-8..8, Z=0..16. Translate by16m on either ground axis. Default grass
is embedded. ground-tile-sand.glb is the exact same measured geometry with the
approved seamless sand PNG embedded; material-variants.json lists both files. Do not mirror, crop, hue-shift or rescale source pixels.
Road/path/soil straight: width centred on X; startZ0, endZ16 (soil4).
Quarter bends: centre of curvature is origin, start at+X and finish at+Z;
travel initially+Z and finally-X. Connect an incoming straight at X=centre
radius,Z=-16. Connect an outgoing straight at origin[0,0,centre radius] with
Y yaw=-90degrees. The path has centreR6,width3; road centreR9,width6.
Sidewalk: local X=0..1.5, Z=0..16; road-facing inner kerb atX0. Walking
surfaceY.14, kerbY.16, closed substrateY.11. Curved sidewalk spans R12..13.5.
Use sidewalk start at[12,0,-16] and outgoing yaw=-90degrees at[0,0,12].
Grooves are actual 6mm gaps in top slab/kerb geometry, over a closed substrate;
join endpoints each hold half a groove. The texture contains no painted mortar.
Pavement endcap attaches at localZ0; expose its bevelled end atZ.4.
Fence post: base-centre pivot; rail: X0..2, Y0...12, thicknessZ.09.
Place post centres every2m, and two rails at chosen local heights. Keep hats,
vegetation, crops, lights and other scenery as separate instances.

The T junction occupies a logical16x16m grid with6m mouths onX=-8,+8,Z=8.
Its occupied asphalt bounds are16x11m, leaving southern grid corners empty.

SURFACE ORIENTATION CONTRACT
All horizontal top faces use uniform planar UV=(X,Z), 1m repeat. No UV warp is
used in bends or the junction. Default surface files are yaw0. Each also has
-yaw90.glb, -yaw180.glb and -yaw270.glb variants with geometry, normals, ports
and planar UVs already baked into that orientation. Select the desired file;
apply integer-metre translation and identity rotation. Do not rotate a default
surface mesh and expect its UV frame to remain aligned to adjacent modules.
The orientation_variants list in each module/model audit records all files and
ports, and inventory-patch.json makes that list available to the gallery.
Default corner occupies+X,+Z. Outgoing leftward straight uses yaw270; eastward
straight uses yaw90. SourceGLB +Y up and metres are retained in every variant.

Texture wrapping: GLTF REPEAT bothS/T, linear/mip filters, source-identical
1024RGB embedded base-colour only. No normal/roughness/occlusion maps. Wood+U
grain runs along fence length/height; fence objects may be rotated normally.
Ground visual density remains a native-scene art choice. For sand use the
provided ground-tile-sand.glb (yaw0) or replace the approved base-colour texture
in an orientation variant, without changing its UVs.

Reviewed QA: isolated reference/contact sheet, actual decoded finite vertices,
indices and unit normals, nondegenerate triangles, exact expected dimensions,
source PNG identity, repeat modes, nine assembled connection pairs and texture
phase across4,097 samples per join, plus uniform planar UV density in all40
surface orientations. Port corner positions are checked against
actual geometry and top-surface UVs for road/path/junction. Pavement joins use
intentionally grooved closed substrate. Ground scroll/repeat and source-edge
checks pass. See assembled-connection-checks.json and seam-checks.json.

These establish offline authoring quality. Native GPU/mobile performance,
in-engine ground brush density, anti-aliasing and placement art review remain
open. No gameplay or product runtime acceptance is implied.
