# Foundation Acceptance Ledger

Selected platforms: macos, windows, linux, ios, android.

Selected optional features: none.

This ledger separates repository mechanisms from product release evidence. An unchecked item is incomplete or unproved. Unselected platform/feature sections are not applicable and must not be checked merely because their source is absent.

## Generation, AI Operation, And Reproducibility

- [ ] `project.json`, generated files, dependency lock, docs, CMake options, and workflows agree.
- [ ] The repository is self-contained and does not resolve the generator, a sibling game, or floating system libraries.
- [ ] `python3 tools/project.py status` gives an AI agent accurate readiness and next-action guidance.
- [ ] `python3 tools/project.py doctor` reports exact actionable prerequisites for the current host and selected target.
- [ ] `python3 tools/project.py bootstrap` materializes only selected exact dependency commits and rejects drift.
- [ ] A clean build directory passes `python3 tools/project.py verify` with warnings-as-errors and non-empty tests.
- [ ] Generators are deterministic, generated artifacts live under ignored paths, and unresolved scaffold tokens are rejected.
- [ ] Runtime packaging contains only allowlisted cooked assets and required notices.
- [ ] Formatting, analysis, sanitizer, and coverage lanes run with their documented pinned tools.
- [ ] `AGENTS.md`, architecture documents, and the newest AI development log agree with current code.

## Baseline Capability Contracts

- [ ] Semantic input supports keyboard, pointer/touch, and SDL gamepad translation without SDL types in the input target.
- [ ] Bindings, contexts, phases, axes, device identity, local users, hot plug, and UI navigation have headless tests.
- [ ] Pointer contacts, drag/cancel phases, timestamps and modality survive semantic translation; synthetic duplicates and axis/button collisions are rejected.
- [ ] Lifecycle transitions cover focus, foreground/background, suspend/resume, memory pressure, thermal/power state, and wall-clock reset policy.
- [ ] Storage rejects traversal/absolute injection and proves bounded reads, version/digest validation, temporary write, and replacement failure behavior.
- [ ] Asset identifiers/manifests are validated, deterministic, digest checked, and independent of source-tree working directories.
- [ ] Packaged reads reject unsafe paths and oversized files before allocation; missing and access/I/O failures remain distinct.
- [ ] UI layout, clipping, hit testing, focus, semantics, interaction targets, text scale, direction, safe areas, and reduced motion have fixtures.
- [ ] Diagnostics prove bounded non-blocking producers, privacy redaction, drop accounting, history, and sink isolation.
- [ ] Semantic audio proves buses, interruption, suspend/resume, invalid values, and no-audio fallback without middleware.
- [ ] Independent capability targets build without SDL, GPU, platform SDKs, network, or gameplay dependencies.
- [ ] Direct SDL and platform branches remain confined to renderer, platform adapters, and the executable composition root.

## Renderer, Shaders, And Packages

- [ ] HLSL is the only authored shader language.
- [ ] Pinned SDL_shadercross produces reflection and exactly the native formats required by selected profiles.
- [ ] CPU/shader interfaces have explicit layout checks and generated binding counts.
- [ ] Shader edits rebuild reflection consumers on the first invocation; deleted reflection/runtime outputs regenerate from declared producers.
- [ ] Missing or incompatible assets fail with structured shader, stage, backend, path, and SDL diagnostics.
- [ ] The SDL_GPU client creates a pipeline and renders a smoke frame on each selected desktop backend.
- [ ] Shipping packages contain native shader artifacts but no authored shaders or host compiler toolchain.
- [ ] Asset/package manifests include stable identifiers, versions, sizes, and digests and reject unlisted files.

## Selected Desktop Platforms

- [ ] Every selected macOS, Windows, or Linux CI lane builds from a clean checkout, runs headless tests, compiles shaders, launches a smoke frame, and audits its package.
- [ ] Exact compiler, SDK/distribution, GPU driver/backend, dependency, and shader-tool versions are archived with evidence.
- [ ] Product release evidence covers the declared OS/hardware/input/package matrix in `docs/PLATFORM_SUPPORT.md`.

## Selected iOS Profile

- [ ] CMake generates and compiles the `iphoneos` app with no hand-maintained Xcode project.
- [ ] The bundle contains only iOS metallibs plus required metadata and passes the static package audit.
- [ ] Device and Simulator shader builds select their matching SDK and Metal target, including compiler fallback.
- [ ] Product physical-device evidence covers safe areas, touch/controller, lifecycle, audio, memory, thermal, performance, accessibility, signing, privacy, and store submission.

## Selected Android Profile

- [ ] Gradle/JDK delegates native compilation to root CMake and assembles audited debug/release packages.
- [ ] APK/AAB contains supported ABI libraries and SPIR-V, passes lint, and excludes host tools/source assets.
- [ ] Product physical-device evidence covers representative GPU/device tiers, cutouts, touch/controller, lifecycle/process death, audio, memory, thermal, performance, accessibility, signing, privacy, and store submission.

## Optional Deterministic-Authority Feature

- [ ] Core has no SDL, filesystem, thread, socket, wall-clock, locale, platform, or ambiguous floating-point dependency.
- [ ] One action follows intent, validation, ordered execution, replay, and named-field state hash boundaries.
- [ ] Invalid identity, sequence, tick, kind, values, bounds, and overflow reject without mutation.
- [ ] Live and replay execution produce the same golden hash across selected build lanes.
- [ ] Protocol/replay decoding rejects malformed, oversized, version-incompatible, or corrupt input before mutation.

## Optional Dynamic-Text Feature

- [ ] SDL_ttf is present only when selected and pinned with complete transitive notices.
- [ ] Measurement and rendering use the same font, locale, script, wrap width, scale, fallback, and missing-glyph policy.
- [ ] Runtime packages contain only licensed required fonts and no unused font/compiler source.

## Product Readiness

- [ ] Every `TODO(REQUIRED)` in `PRODUCT_BRIEF.md` is resolved and owner approval is dated.
- [ ] The first vertical slice uses the selected production boundaries rather than demo-state shortcuts.
- [ ] Numeric performance, memory, launch/loading, package, storage, network, thermal, and energy budgets have platform evidence.
- [ ] Accounts, online services, commerce, analytics, privacy, moderation, and platform services are absent or explicitly designed, tested, and disclosed.
- [ ] The evidence log below identifies commits, tools, platforms/devices, artifacts, results, and caveats.

## Evidence Log

| Date | Commit | Gate | Platform/device/toolchain | Artifact or command | Result and caveats |
| --- | --- | --- | --- | --- | --- |
| TODO | uncommitted | Generated foundation | Current host | `python3 tools/project.py verify` | Not yet recorded |

## Human Resources setup evidence — 24 October 2026

These historical results apply to the 24 October setup checkpoint. They do not
describe later product implementation; the 25 October checkpoint below supersedes
their current-status interpretation. Generator results above are not product qualification.

| Gate | Result | Evidence and limits |
| --- | --- | --- |
| Static profile, dependency, boundary, workflow and preset audit | Pass | `python3 tools/project.py verify --static-only` |
| Exact local dependency checkouts | Pass | `bootstrap_dependencies.py --verify-only`; cache reuse, not fresh network bootstrap |
| Native macOS dev/release build | Pass | AppleClang, native Metal shaders; warnings-as-errors on project sources |
| macOS headless and adapter contracts | 10/10 pass | CTest dev preset |
| SDL-free ASan/UBSan contracts | 9/9 pass | `python3 tools/project.py test --preset asan-ubsan` |
| macOS package and source-bound allowlist audit | Pass | `build/packages/macos/human_resources-0.1.0.zip`; exact host/package evidence in `build/evidence/macos-setup.json` |
| Unsigned iPhoneOS arm64 Release build and bundle audit | Pass | Xcode 26.4.1 / SDK 26.4, iOS 17 floor; `scripts/build-ios-device.sh`, `scripts/verify-ios.sh`; `build/evidence/ios-setup.json` |
| Android prerequisites | Pass | JDK, SDK, build-tools, NDK and CMake discovery only; app build not executed |
| Product runtime, device, performance and store acceptance | Open | Game not implemented; no physical-device or native GPU smoke execution claimed |

## Human Resources infantry/spatial checkpoint — 25 October 2026

| Gate | Result | Evidence and limits |
| --- | --- | --- |
| Native product and repository aggregate | Pass, 37 suites | Static audit, optimized build and actual native GPU/flow/motion evidence: `build/evidence/infantry-spatial-native/review.md` |
| Infantry/spatial iPhoneOS/Simulator packaging and existing development signing | Pass | Frozen 506-input checkpoint; actual Simulator launch fails SDL_GPU capability, and physical installation remains blocked by the locked paired phone |
| Infantry/spatial Android APK/AAB/lint/alignment/runtime package | Pass | `build/evidence/mobile-release/authority2-qualified.json`; software-Vulkan emulator execution, not physical performance or 16KiB runtime qualification |
| Android interruption, OS Back and funded progression | Pass, instrumented | Real process death/cold restore, earned 80-credit purchase, then cold seven-credit wallet/rank-1 retention: `authority2-android-ui/ui-flow-report.json` |
| Exact checkpoint closure | Recorded | `authority2-qualification-closure.json` releases its source freeze; subsequent vehicle/minigun work requires a new coherent qualification |
| Physical devices, human feel and release distribution | Open | Sustained phone budgets, touch/audio/haptics, human pacing, selected desktop qualification and store/signing evidence remain separate; no inherited foundation checkbox is converted to a product pass |

See `RELEASE_READINESS.md` and `MOBILE_QUALIFICATION.md` for current scope and
limits. The unexecuted platform/product checklist items above remain open.

## Human Resources current native-UI checkpoint — 25 October 2026

| Gate | Result | Evidence and limits |
| --- | --- | --- |
| Current canonical source/runtime | Exact, independently rehashed | 559 inputs / 101 runtime-shader files; `build/evidence/native-ui-release/canonical-source-freeze.json` |
| Current native aggregate and original pixels | Pass, 39 suites | Native-resolution UI over unchanged capped world, accepted normal/maximum text; `macos-native-ui-qualified.json` |
| Current macOS ZIP and launchable app | Pass | Outside-checkout Metal/280-frame flow, strict ad-hoc bundle signature/code identity and actual LaunchServices three-frame resource smoke; no notarization |
| Current iPhoneOS/Simulator and Android packages | Pass | Both iOS builds, existing development signing, Android ABIs/variants/lint/AAB/alignment/privacy allowlists; `build/evidence/mobile-release/native-ui-qualification-closure.json` |
| Current Android UI renderer and recovery | Pass, instrumented | Native Vulkan single-sample UI, actual active/draft cold restore/bank-once, original seven-credit/rank-one exact account; physical density/performance is separate |
| Local store materials | Prepared, source-bound | Thirteen exact native source PNGs, final selection, accepted existing-art feature/icon derivatives; `build/evidence/store-capture-preparation/FINAL_INDEX.md`; no publication |
| Physical devices and remaining release gates | Open | Fresh audit still finds iPhone passcode locked and no physical Android; actual font/Privacy GUI interaction, sustained budgets, human feel, Windows/Linux and distribution/store evidence remain unqualified |
