# Native Product Project Blueprint

Status: canonical technical contract for Human Resources.

Foundation version: 2.1.0-dev.

Selected platforms: macos, windows, linux, ios, android.

Selected optional features: none.

## 1. Purpose

Build a native app or game from a small portable foundation without importing genre, service, or middleware decisions before the product requires them. C++20 and CMake are source authority. SDL3 supplies common platform primitives and SDL_GPU. HLSL is compiled offline into target-native shader artifacts. Python standard-library tools provide one workflow on every development host.

This repository is independent. It does not resolve code or dependencies from the generator, another product, or a sibling checkout.

## 2. Baseline Decisions

- Language: C++20 with standard-library value types and RAII.
- Build: CMake; IDE projects are generated outputs.
- Platform primitives: SDL3 behind focused product-owned contracts.
- Rendering: SDL_GPU using Metal, Direct3D 12, or Vulkan.
- Shaders: HLSL source compiled offline with pinned SDL_shadercross tooling.
- Diagnostics: bounded dependency-free structured events.
- Input: raw SDL events become semantic, device-neutral actions before product behavior consumes them.
- UI: dependency-free geometry, focus, semantics, and renderer-neutral draw commands.
- Audio: semantic cues and bus/lifecycle policy; a null implementation is always valid.
- Assets and storage: validated identifiers and paths; runtime packages are allowlisted.
- Dependencies: exact source commits in `dependencies.lock.json`, filtered by selected features.
- Platform and product capabilities: selected profiles, not assumptions hidden in product code.

No baseline ECS, physics, scripting, network, logging, formatting, image, audio-mixing, service, or profiling library is admitted.

## 3. Platform Support Meaning

Platform selection creates build, packaging, documentation, and CI ownership. It does not by itself prove physical performance, signing, store acceptance, or certification.

| Platform | Common backend | Public foundation position |
| --- | --- | --- |
| macOS | Metal/metallib | Desktop build, test, package, and renderer-smoke profile |
| Windows | Direct3D 12/DXIL | Desktop build, test, package, and renderer-smoke profile |
| Linux | Vulkan/SPIR-V | Desktop build, test, package, and renderer-smoke profile |
| iOS | Metal/metallib | Generated Xcode device project and bundle-audit profile |
| Android | Vulkan/SPIR-V | Gradle/NDK SDLActivity and APK/AAB audit profile |
| Authorized consoles | Private SDL/platform adapter | Extension seam only in public code; SDK and certification evidence are private |
| Web | Separate renderer decision | Not a baseline SDL_GPU target |

Claims apply only to platforms listed in `project.json`. The product support matrix and dated evidence live in `docs/PLATFORM_SUPPORT.md`.

## 4. Dependency Admission

Before adding any library, record:

1. the capability and owner;
2. why SDL3, the standard library, or current product-owned code cannot own it simply;
3. exact version, source, and integrity pin;
4. licence and notice obligations;
5. behavior on every selected platform and private extension seam;
6. runtime memory, binary, package, and startup cost;
7. deterministic or realtime impact;
8. update and vulnerability owner; and
9. tests that make upgrade or replacement safe.

One product need does not automatically make a dependency part of the reusable foundation.

## 5. Repository Authority

```text
AGENTS.md                         AI execution contract
PRODUCT_BRIEF.md                     owner-approved product direction
project.json                 generated identity/platform/feature authority
foundation.lock.json              generation lineage and source digest
CMakeLists.txt / CMakePresets.json native build graph and supported presets
dependencies.lock.json            exact selected dependency commits
assets/runtime/                    allowlisted runtime inputs
content/ and data/sources/         schemas and licensed authoring inputs
native/                            product-owned C++ targets
shaders/                           authored HLSL and manifest
tools/project.py                      portable workflow
tools/generators/                  deterministic offline mechanisms
docs/                              architecture, evidence, decisions, AI memory
build/ and external/               ignored disposable outputs
```

Generated projects, shader binaries, protocol bindings, cooked packages, dependency source, and native IDE state are not hand-maintained authority.

## 6. Target Boundaries

The baseline target graph stays shallow:

```text
input   lifecycle   storage   assets   diagnostics   ui   audio
   \        |          |        |          |         |     /
                    platform + renderer
                            |
                          client
```

Independent contracts may use only the standard library and explicitly declared sibling value contracts. `platform` adapts SDL events and OS capabilities. `renderer` owns SDL_GPU resources. `client` is the composition root and does not become the product's universal implementation file.

Optional features add one-way subgraphs. The deterministic-authority profile adds:

```text
protocol -> deterministic core -> runtime -> optional authority service
```

Product modules sit behind the mutation and presentation boundaries chosen in `PRODUCT_BRIEF.md`. Circular targets, service locators, hidden singletons, and platform APIs in product logic are prohibited.

## 7. Platform Capability Rules

Use focused values and interfaces for:

- semantic input actions, bindings, contexts, users, and device changes;
- foreground, background, suspension, focus, memory, thermal, and power events;
- viewport, drawable size, display scale, safe areas, and TV-safe regions;
- application-support, save, cache, and temporary locations;
- atomic persistence and bounded reads;
- asset packages and stable asset identifiers;
- semantic audio cues, buses, interruption, and no-audio fallback;
- haptics and accessibility activation;
- optional users, achievements, cloud storage, commerce, and entitlements.

Do not combine these into a universal platform object. SDL-backed common code should be reused where behavior is actually common; OS or SDK behavior lives in narrow selected sources.

## 8. Input Contract

Raw devices never define product commands directly. The platform adapter produces normalized device events. The input module applies selected bindings and contexts to produce semantic actions with stable identifiers, phases, values, source device, and local user.

Required baseline behavior:

- keyboard, mouse/pointer, touch, and SDL gamepad sources;
- hot-plug-safe device identity;
- press, release, repeat, axis, and pointer positions;
- dead-zone and normalization policy;
- separate UI and product contexts;
- deterministic test fixtures independent of SDL;
- controller-first focus navigation where the product supports it; and
- bindings stored as product configuration rather than scattered key checks.

Platform account/user APIs remain optional adapters; an input device is not silently treated as an authenticated platform user.

## 9. Lifecycle, Storage, And Assets

Lifecycle events are values consumed at the composition boundary. Background or suspension stops presentation work, resets wall-clock accumulation, pauses product systems at an explicit safe boundary, and requests bounded persistence. Memory pressure releases disposable caches only. Thermal or power adaptation changes presentation quality, never authoritative rules.

Storage rules:

- callers use validated relative paths and named storage scopes;
- traversal, absolute-path injection, unbounded allocation, and raw struct persistence are rejected;
- save formats have magic, schema version, payload length, compatibility identifiers, and digest;
- writes use a sibling temporary file, flush/close, and atomic replacement where available;
- platform adapters own real directories, quotas, users, and suspend deadlines; and
- failure is explicit and cannot silently reset player progress.

Asset rules:

- authored assets have stable identifiers, provenance, licences, and source digests;
- cooking is deterministic and selected per target profile;
- runtime reads a cooked manifest/package rather than scanning source directories;
- shader manifests own stage, entry point, format, reflection counts, and digest;
- package audits reject development inputs and host-only tools; and
- asset caches are disposable and do not become product state.

## 10. Renderer And Shader Contract

The renderer consumes presentation state and renderer-neutral UI commands. It owns device/swapchain, pipelines, resources, upload staging, draw submission, and disposable caches. It does not validate or mutate product state.

HLSL under `shaders/` is the only authored shader source. SDL_shadercross produces checked reflection and the native artifact selected by the platform profile:

- Apple: MSL intermediate compiled to `metallib`;
- Windows: DXIL;
- Linux and Android: SPIR-V;
- authorized consoles: private profile format and compiler.

Runtime shader compilation is development-only. Shipping packages do not contain compiler libraries or authored shader source. CPU/shader layouts use explicit sizes, alignment assertions, dense bindings, and generated reflection data.

Avoid building a second universal rendering abstraction over SDL_GPU without a real selected platform that SDL_GPU cannot serve. Isolate backend artifact policy and platform-private extensions first.

## 11. UI, Accessibility, Audio, And Diagnostics

The UI module owns layout geometry, design tokens, clipping, hit testing, focus order, semantic roles, accessibility-tree extraction, and renderer-neutral draw commands. Interaction target policy is modality and product dependent; mobile touch size is not imposed on desktop or console UI.

Dynamic font shaping/rasterization is enabled only by the `dynamic-text` feature. Measurement and rendering must use the same font, locale, script, wrapping width, and scale. Native accessibility adapters map the same semantic tree to the selected OS without leaking SDK types into UI.

Audio receives semantic cues and owns buses, voices, devices, focus, interruption, streaming/static policy, and no-audio fallback. Product state never depends on whether a cue played.

Diagnostics producers are bounded and non-blocking. Sensitive values are redacted before sinks. Logging, sink failure, or telemetry availability cannot affect startup or product outcomes.

## 12. Optional Deterministic Authority

When selected, `docs/DETERMINISTIC_SIMULATION_RULES.md` is binding. Human input, bots, replay, and remote clients submit intents. Validation and ordering produce the only executable game operations. Fixed integer ticks, explicit RNG, stable IDs/order, replay logs, and named-field hashes provide cross-platform proof.

Without this profile, the product still defines a clear state owner and update model in `PRODUCT_BRIEF.md`; it does not inherit networked turn-bundle ceremony by default.

## 13. Portable Tooling

`tools/project.py` is the user and AI entry point:

- `status`: selected profiles, brief readiness, repository state, and next commands;
- `doctor`: exact host prerequisites and actionable gaps;
- `bootstrap`: materialize and verify selected dependencies and host tools;
- `build`, `test`, and `run`: delegate to named CMake presets;
- `package`: invoke the selected platform package owner;
- `verify`: run static audits, tests, generated/package checks, and repository hygiene.
- `evidence`: record exact host/toolchain/dependency/backend/package evidence for CI archival;
- `publish`: explicitly create and push the independent product-owned GitHub repository.

Thin shell or PowerShell wrappers may delegate to it. They do not duplicate orchestration or target graphs.

## 14. Verification Matrix

Every generated product requires:

- self-contained clean generation with no unresolved tokens;
- exact dependency verification and no sibling fallbacks;
- warnings-as-errors for product-owned code;
- headless tests for each independent contract;
- shader compilation/reflection for each selected backend;
- renderer smoke evidence on every claimed desktop backend;
- selected mobile project compilation and bundle audits;
- generated/project-contract agreement;
- runtime package allowlist and licence audit;
- sanitizer, analysis, formatting, and coverage lanes where supported; and
- a dated evidence ledger that distinguishes CI, simulator, emulator, and physical device proof.

Per-product signing, physical performance, stores, online services, accounts, and console certification are completed by the game before its release claim.

## 15. Product Development Rule

Complete `PRODUCT_BRIEF.md`, then implement the first vertical slice through its real state, input, persistence, presentation, asset, audio, and platform boundaries. Add only capabilities the slice requires. If a second game later proves the same stable mechanism is broadly reusable, it may become a future opt-in foundation profile after a separate dependency and maintenance review.
