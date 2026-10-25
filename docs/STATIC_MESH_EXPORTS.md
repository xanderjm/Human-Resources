# Reviewed static foliage exports

The production crop-seedling and grass-tuft IDs consume the explicitly reviewed `*-runtime.glb` source exports. Their original normalized GLBs and raw Tripo downloads remain preserved. Bush-low remains the original mesh: its attempted reduction lost the rounded leaf character and was rejected. No runtime LOD or platform-specific geometry policy is involved.

`assets/production/static-mesh-optimizations.json` records the canonical and edited source hashes, expected cooked bytes, topology invariants, and exact offline method/tool owners. `tools/static_mesh_optimizations.py` validates these inputs before the cooker can fall back to another source. The ordinary content cooker consumes the same checked GLBs on all selected platforms; it does not run the mesh reduction.

## Verification and optional authoring replay

Run `python3 tools/static_mesh_optimizations.py` for cheap source/recipe checks. Normal `python3 tools/project.py verify` includes the contract tests. Normal content cooking also verifies the resulting HRM geometry and texture digest against the approved output.

To reproduce the authored exports using the existing pinned art tools:

```sh
python3 tools/static_mesh_optimizations.py --replay build/static-mesh-replay-001
```

Choose a fresh ignored build directory. The command never overwrites an original or reviewed export. It uses pinned Blender4.5.15 and art Python/NumPy, preserves per-corner UV/material/custom normals through exact-position welding, performs one25percent triangulated collapse, restores the reviewed source bounds with inverse-scale normal correction, and checks exact output GLB hashes. Changing smooth/custom-normal setup order alters shading and is invalid. A replay mismatch fails and requires investigation; it does not update approval or publish new geometry. Cross-host authoring bit identity is not implied, while ordinary cooks consume the identical checked exports.

Crop triangles1700→425; grass1814→453. Existing geometric components/open boundaries/nonmanifold incidence are retained, with no new fragments or seam holes in the accepted views. Node transforms, embedded images and materials are unchanged. AABB equality is a culling contract, not proof of silhouette equivalence.

The accepted two-mesh native comparisons at initial/earned400-second phone/tablet states reduced combined world+shadow triangle submissions by12.1–25.6percent. Those totals include shadow work and establish no frame-time, physical-phone FPS, thermal or battery result. Previous platform packages remain qualified only for their recorded source epoch; changed cooked content needs fresh platform qualification.

Authoring captures, recipes, scripts, source GLBs and raw downloads do not ship. The runtime allowlist continues to ship the same mesh IDs and existing content provenance.
