# Migrating a 2.0 generated product

Generated repositories are independent snapshots. Apply these changes deliberately to the product; do not regenerate over its source, data, Git history or platform signing configuration.

## Project discovery and generation

- `create_project.py --kind app|game` is the canonical generator. `create_game.py` delegates with `game` as its default kind.
- New outputs use `project.json` schema 2 and `PRODUCT_BRIEF.md`. The contract adds `kind`; identity, selected platforms, features and platform settings retain their meanings.
- `tools/project.py` owns orchestration; `tools/game.py` delegates for command compatibility. When migrating an existing repository, rename the contract/brief and update their consumers together. Keeping two independently editable contracts is unsupported.
- Product kind does not select a simulation architecture or add gameplay systems. Deterministic authority and dynamic text remain explicit capabilities.
- App briefs contain no required game-loop, progression, multiplayer or simulation decisions.

## Build and package changes

Take the source-inventory, exact-staging and final-artifact audit changes together with their CMake/script consumers. Newly cooked manifests contain authored source records and a root digest. Old packages cannot satisfy the new current-source audit; rebuild them.

The staging target owns the complete desktop assets directory and the iOS bundle assets directory and removes retired files. Do not add another post-build copy over that directory. Mobile verification compares bundled assets to both current manifests and sources. iOS builds configure first so added/removed resources enter the current build graph. Xcode per-file resource copies must be removed when adopting exact staging; otherwise they compete with its ownership.

Bootstrap installs runnable host tool bundles from isolated builds. Old CMake caches are not authoritative or relocatable. An old cache without the new bootstrap stamp is rebuilt. Explicit supplied compiler bundles are checked separately and never satisfy an ordinary pinned-source bootstrap cache hit.

The generated core contracts can build without SDL when the client and dynamic-text feature are disabled. Platform adapters, native accessibility and rendering still require their selected implementation; this does not create a headless application entry point automatically.

## Persistence changes

Implement the new byte-bounded `Backend::read` method in every product storage backend. Return `not_found` only for confirmed absence. Permission, parent-path, device and I/O errors must remain failures. A default implementation that returns empty successful bytes would defeat data protection.

`ProtectedDocument` is an optional owner for the existing envelope format. After `load()`, a valid envelope still needs product schema validation and `accept_loaded_payload()` before saving. Confirmed absence permits the first save. Future schema, corruption, excessive size and failed reads stay protected. Explicit recovery copies the original to a new backup name and verifies it before permitting reset. Applications with concurrent writers need an adapter transaction or locking policy.

The envelope bytes/magic and digest format are unchanged. This is not an automatic migration of Rituals progress or Luck & Loot profiles.

## Presentation changes

Normalize window dimensions, safe areas, pointer input and native accessibility frames as one change. Layout operates in points; raster density and accessibility font scale have separate purposes. Mixed old/new coordinate policies cause incorrect touch targets on density-scaled displays.

Text insets and resolved widths now affect intrinsic measurement; grid rows measure at the cell width. Review intentional fixed-height text and check `LayoutResult.overflows` at maximum supported text scale and translations.

Use durable semantic IDs where numeric nodes are rebuilt or reordered. Reuse native elements by that identity and reject disabled/stale activations in the adapter. Numeric IDs remain a compatible identity only if the product keeps them stable.

Audio adapters now report `available()`. The baseline audio/haptic implementations report unavailable; a product implementation must retain its own voices, cue mapping, device/interruption behavior and native feedback policy.

## Requalification

### 24 October 2026 maintenance changes for existing 2.1 products

- Apply the semantic `DeviceEvent`/`Action` metadata and SDL translation together. Update axis bindings to `platform::gamepad_axis_control_base + SDL_GAMEPAD_AXIS_*`; button bindings retain native IDs. Preserve explicit cancellation/drag phases and original event timestamps in downstream gesture recognizers. Mouse motion maps to the left-button drag/hover control; multi-button capture policy belongs to the product.
- Apply catalog-path validation and bounded `SdlAssetSource` reads together with the asset/platform regressions. Permission and invalid-parent failures remain errors. Record paths containing drive prefixes, embedded NUL, repeated separators or dot segments are rejected; recook any such assets with portable paths.
- Declare `shader_manifest.generated.h` and every runtime cooked output as custom-command byproducts. Include the first-rebuild and deleted-output regressions. Preserve existing exact staging ownership.
- Carry the iOS SDK selection from `cmake/Shaders.cmake` through `compile_shaders.py`, including its normal and fallback Metal commands. Device and Simulator libraries share package paths and must be rebuilt for their actual SDK; they are not interchangeable.
- Replace the platform test's shared fixed temporary directory with atomic unique reservation. Never remove another concurrent test process's fixture.

Run the generated static, contract, format, analysis, sanitizer and selected-platform package gates. Test real product data upgrades and recovery copies before adopting persistence changes. The original source's 2.0 evidence does not qualify modified 2.1 products, and host tests do not prove physical-device accessibility behavior.
