# Offline asset authoring and review

Status: accepted for authoring, 24 October 2026. No native runtime dependency admission.

The production collection needs reproducible transparent sprite rasterization and an interactive review of downloaded GLBs, independently of the shipping renderer. Keep this work in `tools/` and source assets outside the runtime allowlist.

- Sharp 0.35.5 (Apache-2.0) rasterizes original editable SVGs to transparent PNGs. The existing Pillow path has no SVG renderer; reimplementing gradient, mask and path rasterization would add an unrelated renderer. Exact versions and platform-specific optional binaries are recorded in `tools/model-viewer/package-lock.json`. The SVG/PNG hashes, alpha margins, independent XP fill and laser repeat checks are the replacement gate.
- Three.js 0.186.1 (MIT) and its bundled GLTFLoader power the existing local browser inspector and collection. They render the actual downloaded geometry and albedo. They are authoring-only browser modules; there is no WebView or JavaScript dependency in the native game. Model audits, offline render evidence and collection load/error counts provide the replacement gate.
- Existing NumPy/Pillow authoring tools handle measured geometry, periodic texture correction and deterministic export. No new game-linked image or mesh middleware is introduced. Native asset upload and packaging remain owned by the SDL_GPU/offline cooker path.

These tools are supported only as local authoring tools; no five-platform authoring qualification is claimed. npm chooses host-specific Sharp binaries from the exact lock. Update owner: asset-authoring tools. Installed host tooling size is disposable `node_modules`; game binary and shipping package cost is zero because these tools, references and raw responses are excluded from the runtime allowlist.

Canonical repeat masters are `assets/textures/texture-*/texture-*.png`. Preserve generated inputs as `raw-v1.png`; never cook a white-backed swatch or the old generated draft. The native development preview consumes the canonical masters directly and retains source hashes. Repeating meshes embed those exact PNGs. Horizontal modular surfaces use world-aligned planar UVs with explicit baked orientations and integer-metre placement, documented alongside the module audits. This avoids stretching a T-junction to force all mouths into a flowing UV frame.

Numerical equality is one gate. Inspect 3×3 repeats and assembled joins for banding or visibly terminated brushwork even when edge pixels and finite differences match. Material approval is separate from asset loading and from physical-device/runtime qualification.
