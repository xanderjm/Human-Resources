# Platform Support Contract

Selected public platforms: macos, windows, linux, ios, android.

Only selected platforms are build/support targets for this repository. Adding or removing one changes `project.json`, build/package ownership, CI, runtime asset formats, and acceptance evidence together.

## Evidence Levels

- **Architected:** independent contracts and an adapter seam exist.
- **Build-qualified:** a clean selected toolchain compiles and static package audits pass.
- **Runtime-qualified:** the application launches and renders through the target backend.
- **Release-qualified:** product budgets, physical hardware, signing, store/certification, lifecycle, input, storage, accessibility, network, and recovery evidence pass.

Never report a higher level from lower-level evidence.

## Public Profiles

| Target | Toolchain/backend | Generic foundation gate | Product release evidence |
| --- | --- | --- | --- |
| macOS | pinned Xcode; Metal/metallib | build, headless tests, renderer smoke, package audit | supported Mac/GPU/OS matrix, signing/notarization, performance and input |
| Windows | pinned Visual Studio/SDK; D3D12/DXIL | build, headless tests, renderer smoke, package audit | supported CPU/GPU/OS matrix, installer/store, graphics/input/accessibility |
| Linux | pinned distribution/Clang; Vulkan/SPIR-V | build, headless tests, renderer smoke, package audit | selected distro/runtime/GPU matrix, packaging, display/input/audio |
| iOS | iOS 17.0; Metal/metallib | generated device Xcode project and bundle audit | physical devices, signing/store, lifecycle, safe area, touch/controller, thermal/memory |
| Android | API 26 / target 36; NDK 28.2.13676358; Vulkan/SPIR-V | Gradle/NDK build, lint, ABI and bundle audit | physical GPU/device tiers, signing/store, lifecycle, input, thermal/memory |

Rows not selected in `project.json` are informative extension capabilities, not repository claims.

## Console And Private Profiles

Console platform access, headers, libraries, shader compilers, packaging, users, storage, services, dev-kit control, certification rules, and evidence are platform-holder controlled. Public code defines capability seams and test doubles only.

An authorized private profile must provide:

- a selected CMake toolchain/build integration;
- SDL or equivalent narrow platform implementation;
- native shader cooking and package inventory;
- entry point, lifecycle, users/controllers, storage, network, audio, haptics, and accessibility adapters;
- dev-kit deploy/run/log commands behind `tools/project.py` extension hooks;
- confidential CI runners and secret-safe diagnostics; and
- platform certification and physical-device acceptance ledgers.

Do not commit NDA material to a public repository or expose SDK paths/content in logs.

## Product Matrix

Complete before a release claim:

| Platform/device tier | OS/firmware | CPU/GPU/RAM/input | Build/runtime evidence | Budgets | Signing/store/certification | Result/date |
| --- | --- | --- | --- | --- | --- | --- |
| TODO(REQUIRED) | TODO | TODO | TODO | TODO | TODO | TODO |

## Product Budgets

`PRODUCT_BRIEF.md` owns numeric targets for frame time, memory, launch/loading, package/download size, storage, network, thermal/energy behavior, and recovery deadlines. Tests and dated platform evidence enforce them; the generic scaffold does not invent acceptable values.
