# Technical Decisions

## 25 October 2026 — Farmland frontage within existing clearance

Recover unused full-mesh margin rather than adding random dressing or changing
the camera. Move the existing barn/yard/hay group0.65m, silo1.90m and complete
windmill pair1.00m inward in their authored X coordinates. Preserve station,
identity, scale, counts, roads, trees and other themes. Consumed opaque geometry,
including the rotor's complete turn, remains beyond the existing9.7m clearing.
The ground-only apron may enter that clearing, retaining a quiet farmyard bay
and crop-circle separation. The barn remains partly cropped by design.

Only product presentation rotates the windmill's authored Z spindle, using
fmod(saved active_seconds*0.45,2pi); reduced motion selects zero. No extra timer,
state, RNG, save field or renderer-specific animation owner is needed. Model
transforms reach the existing visibility system before rendering. Native matched
states retain the same gameplay and scenery population; extra submitted work is
reported honestly for newly visible existing assets, separately from phone FPS.

## 25 October 2026 — Structured reward presentation

Keep exact projected benefit labels, units and secondary conditions beside the
existing before/after calculation in make_choice. UpgradeChoice adds derived
presentation strings; legacy complete descriptions remain byte-identical and
saved choice identities regenerate all metadata. UI consumes these values rather
than parsing prose or calculating upgrade rules again. Spoken mechanics retain
the full description, rank/status and evolution context.

Choose the three-card board only if its measured cards/footer fit; otherwise
reuse the existing selected-offer tabs/pages without shrinking text or controls.
Semantic source provenance owns keyboard hints, including meaningful partial
controller axes; synthetic diagnostics do not masquerade as keyboard use. Cosmetic
icon/receipt accents reuse existing PromotionFlow ages, preserving stationary
hit targets,250ms entrance,120ms confirmation and800ms nonblocking receipt. Reduced
motion settles immediately. No new assets, timing owner, dependency or save field.

## 25 October 2026 — Strict portable client compilation

Keep the selected Windows Visual Studio/SDK profile unchanged. An installed
MinGW GCC16.2 cross-build is supplemental evidence only. Fix actual strict
compiler diagnostics through explicit empty aggregate defaults, a bounded
size_t glyph index and standard %llu diagnostic arguments with matching unsigned
long long casts. Do not suppress warnings or relax project flags. Preserve
Windows UTF-8 reads through filesystem::path(std::u8string(...)), equivalent to
the deprecated u8path conversion; other platform branches are unchanged.

Set WIN32_EXECUTABLE only for the ordinary Windows desktop client, producing
GUI subsystem2 and the SDL WinMain entry. Game state, serialized data, layout,
assets and dependencies stay unchanged. Current host44suites and five native
before/after PNG pairs pass; a strict complete Windows link/package audit proves
compilation, not execution or selected-toolchain qualification. Static runtime
notice materials and unresolved upstream provenance remain separate from public
distribution clearance. Exact evidence is in windows-portability-integration and
windows-cross-qualification under ignored build/evidence.

## 25 October 2026 — Scoped portable offline art tools

Keep existing NumPy/Pillow source cooking and Blender recipe exports outside the
runtime. Pin their accepted versions and official release hashes in
`tools/art-toolchain.json` and `tools/art-requirements.txt`; normal bootstrap
prepares ignored repository-scoped tools. Strict explicit interpreters/Blender
paths, then checked platform discovery, replace private application-cache and
single-host path assumptions. Cold exports retain existing private staging,
source revalidation and publication locks. `OFFLINE_ART_TOOLCHAIN.md` owns
licences, host costs, update/replacement checks and unexecuted-host caveats.


## 25 October 2026 — Capture world transport and authored pose

Product presentation owns capture convergence, scale and cancellation return in
a fixed 128-actor active-time history. A surviving positive-to-zero capture edge
starts a 1.4-second return to the current authoritative ground root. Pause freezes
it; absence, retyped IDs and new/restored runs clear it. Reacquisition starts from
the current presented transform. Gameplay cancellation, collisions and rewards
remain immediate, and presentation history is excluded from saved continuations.

For opted-in product actors, the renderer normalizes only capture/release root
translation against the source rest node before composing skin and attachments.
Relative joint translation, rotation, scale and raw/default source playback stay
unchanged. Release-pose blending uses 0.08 seconds. This avoids competing world
and source lift, product copies of source lift curves, or a renderer-to-product
skeleton dependency. The same resolved world pose feeds bodies, attachments and
visibility. Actual public-Game range loss, composed early/mature cases, all eight
source pairs and native trace replay qualify this boundary; see
`MILITARY_VARIANTS_HANDOFF.md` and the development log.

## 25 October 2026 — Preview viewport and Metal submission

Separate live window points, backing pixels and fixed capture pixels. Bound live
target size, adapt the camera to its actual aspect and preserve aspect at final
presentation. Keep camera scale continuous across portrait/landscape changes.

Adopt product-owned instancing, conservative culling, theme-scoped preloading
and retained completion fences, following inspected Midnight Menu patterns
without linking that checkout. Preserve two frames in flight and retain fences
through a subsequent submission's Metal command cleanup. Wait by yielding.
Use one preview frame deadline; avoid stacking a display throttle with a CPU
throttle where immediate presentation is supported. Keep GPU validation in Debug.
Preserve original mesh data, four-sample AA and 2048² shadows. No runtime
dependency or shipping-content promotion is needed. See
`SCENERY_PREVIEW_PERFORMANCE.md` for measurements, limits and integration APIs.

## 24 October 2026 — Offline character animation

Author the six existing character bodies through measured product-owned rig and
motion recipes. Use portable Blender 4.5.15 LTS solely for content authoring,
surface-aware skin solving, editable rig files, glTF export and the film renderer.
The exact download, licence, capability, package cost, owner and replacement gate
are recorded in `CHARACTER_ANIMATION.md`. No native dependency is added.

Keep source models and the production inventory intact. Generated animation
GLBs, packed Blender files and captures live in ignored build output, backed by
reproducible recipes and a handoff ZIP. Use standard four-influence linear glTF
skinning, baked named clips and an independent NumPy playback validator. Solve
humanoid weights on welded temporary geometry, transferring weights back without
altering source geometry/UVs, and reconcile all split-vertex skin influences.
Separate animation intent from runtime gameplay; the existing native client and
static style preview do not acquire a skeletal player through authoring work.

## 24 October 2026 — Native asset and level style preview

Keep visual choreography in the SDL-free `human_resources_presentation` target,
mesh decoding in assets, and native GPU resources/readback in the independent
`human_resources_style_renderer`. The explicit development viewer uses the
project's SDL_GPU/offline-HLSL stack and adds no native dependency. NumPy/Pillow
remain existing authoring dependencies; FFmpeg 7.1.1 is a host-only encoder with
zero game binary/package cost. Encoded captures are verified with ffprobe and
are not gameplay/performance evidence.

Use a bounded HRM1 cooked mesh/texture value contract instead of parsing GLB/JSON
or decoding images in the render loop. Resolve source node transforms offline,
retaining the modular rotor split around its authored rest pivot. Preserve raw
and normalized authoring assets; pending scenery is allowed only in the preview
without changing its acceptance status. This import/render boundary can be
promoted deliberately after asset and platform review.

Keep preview shaders/content out of ordinary product builds and packages. Reuse
the existing shader compiler with explicit separate roots; do not stage all art
or the model viewer. Use the final edge-to-edge terrain tiles with normal repeat
wrapping and world-space UV proportions. Do not mirror textures to manufacture
seams. The earlier mirrored-swatch experiment was removed after owner feedback.

See `STYLE_PREVIEW.md` for commands, framing, format, limitations and next visual
work. No camera/level data is an approved gameplay rule or final level schema.

## 24 October 2026 — Human Resources foundation adoption

Generate from Game Scaffold's current source and merge into the existing product
repository without reinitializing Git or replacing authoring files. Preserve the
generated provenance lock. Follow Midnight Menu's developer namespace with a
distinct app ID. Select its five build profiles, prioritizing macOS development
and the planned iPhone launch. Landscape is a working assumption in the brief.

Adopt Midnight Menu's optimized development preset and separate Debug preset.
Sanitizer, analysis and coverage explicitly remain Debug. Preserve matching test
presets and non-empty-suite gates. Hosted workflows are manual and retain their
complete validation and evidence jobs.

Keep SDL and SDL_shadercross as the baseline dependencies. No text middleware,
deterministic authority, model middleware, commerce SDK or editor is selected by
setup alone. Existing GLBs need a focused import/render slice. Runtime shipping
remains allowlisted and starts without product content.

Reference cache copies are independent ignored checkouts/bundles at exact pins.
No symlink, CMake path or runtime search path may require a sibling repository.
See `PROJECT_SETUP.md` and `setup.sources.json` for provenance and scope.

## 24 October 2026 — Midnight Menu maintenance backports

Adopt the reusable input, asset-read, generated-output, iOS shader-target and fixture-isolation fixes. Keep the dependency set, Debug development default, product brief and capability profiles unchanged. Midnight Menu's renderer, studios, combat/content systems and benchmark budgets remain product-owned.

Input metadata is additive; an explicit phase overrides value inference. SDL axis IDs move to a disjoint range, so existing axis bindings must migrate with the adapter. SDL streams remain the asset-reading owner for Android package access; failed opens require confirmed absence before returning `not_found`. Catalog validation rejects traversal, absolute/drive, embedded-NUL and ambiguous path forms before I/O. Compiled record sizes bound allocation, with stream-size and trailing-byte checks around the read.

Declare generated reflection and runtime files in the CMake graph instead of relying on a second build, executable relinking or filesystem timestamp discovery. Preserve the existing iOS package layout while selecting a distinct device/Simulator Metal target. Test fixtures reserve unique directories atomically and only remove their own paths.

## 24 October 2026 — Authored sections for scrolling scenery

Use coordinated authored road/landmark sections with seeded secondary detail in
the development scenery generator. Independent noise and regular prop spacing
did not reproduce the reference compositions. A fixed comparison section gives
visual calibration a stable target; compatible endpoint ports let sections join
without road discontinuities. Three variants per theme are an initial study,
not an accepted production variety budget.

Keep the generator SDL-free and query it by absolute distance. Return only a
bounded local presentation window with stable identities and world-anchored
surface UVs. GPU streaming and capture remain renderer concerns. Do not infer
gameplay collision, progression or shipping content acceptance from these visual
fixtures. See `SCENERY_GENERATION.md` for current evidence and remaining gaps.

## Generated Baseline

The current gameplay engagement candidate is documented in
[ADR 003](adr/003-active-harvest-and-starter-dash.md): Game authority 7 carries
close-sweep chains, temporary Overdrive and starter Dash/counter-volley forward
with expanded flight, varied harvest groups, local fleeing and power-linked
scrolling. Body-aware military approaches and ingress preserve distinct enemy
silhouettes without displacing committed attack sources. Actual boss/engaged
vehicle vitality and source-consistent stopped vehicle headings improve feedback.
The ship retains its original apparent size in the wider world view.
Unsupported earlier
continuations retain protected recovery. This does not change the outer account
schema, renderer authority or abstract simulator boundary.

- Foundation version: 2.1.0-dev.
- Selected platforms: macos, windows, linux, ios, android.
- Selected optional features: none.
- Native language/build: C++20 and CMake-generated native projects.
- Common platform/renderer: SDL3 and SDL_GPU.
- Shader source/build: HLSL compiled offline with SDL_shadercross into target-native formats.
- Tool orchestration: Python standard library through `tools/project.py`.
- Product UI: dependency-free geometry, semantics, focus, hit testing, and renderer-neutral draw commands.
- Input: semantic action values independent of SDL devices.
- Diagnostics: dependency-free bounded structured events with privacy and drop accounting.
- Audio: semantic product-owned interface with no mandatory mixer or middleware.
- Assets/storage/lifecycle: focused product-owned contracts with platform implementations outside gameplay.
- Dependency source: exact selected commits in `dependencies.lock.json`.
- Repository ownership: generation initializes a clean local `main` commit by default; GitHub creation and push are explicit owner actions.
- Profile ownership: unselected platform and optional-feature source, dependencies, workflows, scripts, and docs are absent rather than dormant.
- Private targets: public capability and CMake boundaries remain stable while licensed console entry points, SDKs, packaging, and certification stay in authorized product-owned profiles.
- Mobile orientation: the generated iOS and Android shells enforce the selected `project.json` orientation. Android's `LockedOrientationActivity` lint rule is suppressed only on that activity because rotation policy is an explicit game contract; large-screen support still requires per-product device evidence.
- Runtime assets: authored files stay in `assets/runtime`; one standard-library cooker creates a clean generated tree and exact ID/path/size/SHA-256 manifest. Shader packages use the same exact-inventory audit, and every shipping package rejects missing, changed, or unlisted cooked assets.
- Qualification evidence: selected CI lanes retain bounded JSON toolchain/dependency/artifact records and desktop smoke logs containing the actual SDL_GPU backend, device, and driver. Evidence is disposable build output, not source authority.

There is no baseline ECS, physics, scripting, networking, logging, formatting, profiling, image, audio-mixing, analytics, account, commerce, or service library. Familiarity is not an admission reason.

Durable changes to these decisions require a focused ADR describing need, alternatives, dependency/platform/package cost, migration, and verification. Product-specific additions may remain product decisions without expanding the reusable foundation.


## 25 October 2026 — Serialized animation publication

Use the Python standard library to serialize each animation producer/output and
recover interrupted multi-file publication before admitting a reader. Keep locks,
staging and rollback backups outside shipping asset directories. Blender owns
rig/export work; the configured NumPy/Pillow art Python owns independent
validation. Preserve exact source/tool/config/GLB freshness checks and hold the
export lock through native cooking. Direct authoring and cook-triggered authoring
use the same protocol with distinct private staging locks, avoiding recursive
acquisition. See ANIMATION_BUILD_CONCURRENCY.md for implementation, ten failure/
concurrency tests, actual cold/concurrent cook evidence and Windows limits.
No new library or native format was introduced; promoted immutable authoring
inputs remain a possible later build-boundary refinement.

## 25 October 2026 — Native-resolution product UI over a capped world

Keep the product's longest-side 1280-pixel 3D target, world MSAA, shadow map and
geometry budgets. Draw normalized screen quads in a separate single-sample
pass after the world blit, using the actual swapchain format and physical
viewport. The previous shared world/UI target enlarged text by about 2.05x on a
1206x2622 display. Raising the entire world resolution would spend GPU/memory
on a different problem; a separate UI pass corrects text/control rasterization
without changing semantic input, layout, simulation or world quality settings.

Normal presentation adds no native-sized color/depth/MSAA allocation. Explicit
captures use a cached RGBA8 composition target and transfer storage, because SDL
swapchain textures are write-only. Default capture dimensions retain the legacy
world extent; opt-in physical extents must fit the logical viewport aspect,
4096 pixels per side and eight million pixels total. At 1206x2622 the extra
capture-only texture plus readback costs about 24.1 MiB. Staging and validation
precede swapchain acquisition; acquired commands are submitted even on a later
UI failure. No shader, font atlas, runtime dependency or rendering-settings
framework is introduced.

Native Metal source-stable pixel proofs cover default/maximum text, HUD,
buttons, phone/Google/iPad composition sizes, channels/alpha, failure recovery,
resize and minimize/restore. World dimensions/draws/shadows/geometry and the
Game digest stay equal across the measured rendering paths. These are
functional/pixel checks, not a physical-phone performance pass. Final current
packages and native aggregate remain qualification gates for this source epoch.
