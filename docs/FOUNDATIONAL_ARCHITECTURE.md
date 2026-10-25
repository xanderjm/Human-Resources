# Human Resources Foundational Architecture

Foundation version: 2.1.0-dev.

Selected public platforms: macos, windows, linux, ios, android.

Selected optional features: none.

## Ownership

- `human_resources_diagnostics`: bounded structured events, privacy redaction, history, and sink contracts; no SDL or product logic dependency.
- `human_resources_input`: device-neutral semantic actions, bindings, input contexts, users, and test fixtures; no SDL dependency.
- `human_resources_lifecycle`: device-neutral foreground, suspension, memory, thermal, power, and focus values.
- `human_resources_storage`: validated relative paths, atomic save operations, and storage-result contracts; concrete OS paths remain in platform adapters.
- `human_resources_assets`: stable asset identifiers, manifest validation, package lookup, and content digests.
- `human_resources_ui`: dependency-free layout, design tokens, hit testing, focus, semantics, and renderer-neutral draw commands.
- `human_resources_accessibility`: owned semantic snapshots, bounded native-action queues, stable identity and stale-action rejection; no SDL dependency.
- `human_resources_audio`: semantic audio commands and bus/interruption policy. Middleware is not selected by the foundation.
- `human_resources_renderer`: SDL_GPU resources, pipelines, cooked shader assets, and consumption of renderer-neutral presentation data.
- `human_resources_platform`: SDL event/window adapters and narrow OS capability implementations.
- `human_resources_client`: executable composition root; it translates platform events to foundation contracts and coordinates presentation.
- `native/tests`: headless capability tests. Renderer and device tests add evidence rather than replacing these contracts.

Optional profiles add their own one-way targets. The deterministic-authority profile adds protocol, deterministic core, runtime coordination, replay, and optional hosted-authority contracts.

## Dependency Direction

Independent value contracts do not depend on SDL. Platform and renderer adapt those contracts for a selected target. The client composes them. Product product logic depends inward on stable contracts; platform and presentation never flow back into product logic truth.

There is no global engine singleton or service locator. Long-lived resources have explicit owners, and optional platform services are focused capabilities rather than a universal interface.

## Initial Proof Application

The generated client displays a semantic, safe-area-aware UI surface and routes keyboard, pointer, touch, and controller-shaped actions through the input boundary before updating a disposable demonstration value. It exists to prove platform, input, UI, renderer, assets, audio fallback, diagnostics, and lifecycle composition. It is not the product's architecture.

Product product logic begins only after `PRODUCT_BRIEF.md` is approved. Replace the demonstration behavior vertically through the chosen state model; do not turn the composition root into a growing product implementation.

## Input samples and packaged reads

Semantic input preserves source control, modality, contact identity and the original monotonic nanosecond timestamp (zero means unavailable). Explicit pointer phases survive action mapping; events without an explicit phase retain value-derived behavior. The SDL adapter translates real mouse motion as left-button drag/hover, translates every touch phase, and filters touch-generated mouse and mouse-generated touch duplicates. SDL gamepad buttons retain their native control numbers; axes use `platform::gamepad_axis_control_base + SDL_GAMEPAD_AXIS_*`. Product gesture policy and per-contact capture remain product-owned.

`SdlAssetSource` validates the complete record before opening its package path. It checks the opened stream's size before allocating the compiled byte budget, then rejects truncation, growth and digest mismatch. SDL streams retain Android packaged-asset access. A failed open is `not_found` only when a filesystem probe confirms absence; access, invalid-parent and probe errors remain `io_failure`. Package roots are trusted installed/cooked trees; lexical record validation is not a sandbox for arbitrary symlink-containing directories.

## Platform Extension Rule

SDL owns the common open-platform surface. Missing OS capabilities use narrow source files selected by the build. Authorized console profiles may provide platform-holder SDK entry points, storage, users, services, shader packaging, and certification checks without changing the independent contracts. Private headers and binaries never enter public source or logs.

## Persistence and optional feedback

`Backend::read` is byte bounded and distinguishes confirmed absence, unavailable storage, excessive size and I/O failure. Permission or metadata failure must never turn into a fresh empty document. `ProtectedDocument` loads and validates the storage envelope, waits for the product to accept its payload, and protects unknown/future schemas. Explicit reset is allowed only after copying and reading back a new backup. The object assumes one writer; applications needing concurrent document writers must add a transactional adapter/locking policy.

Audio and haptic capabilities are optional. `NullAudioService` reports unavailable playback while maintaining bus/interruption policy; `NullHapticService` reports unavailable and returns false. Products must remain operable without feedback. A selected implementation owns its cue map, decoding, streams, mixing limits, device loss, consent/preferences and platform interruption handling. No Rituals or Luck & Loot sounds, voice counts or feedback policy is part of the core.

With the client disabled, the independent contracts build without configuring SDL unless `dynamic-text` is explicitly selected. Another native composition root can reuse these modules. The foundation does not generate an alternative UI toolkit or browser renderer.
