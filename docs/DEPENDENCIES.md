# Dependency And Toolchain Record

`dependencies.lock.json` is the machine-readable authority. It contains only dependencies required by the selected profiles and exact source commits. `python3 tools/project.py bootstrap` materializes them under ignored `external/` paths and rejects drift.

## Baseline

- SDL3: window/events, input and audio primitives, platform utilities, and SDL_GPU.
- SDL_shadercross: host-only offline HLSL translation and reflection; compiler components do not ship.

## Selected Optional Dependencies

- `deterministic-authority` selects FlatBuffers for versioned protocol/replay schemas.
- `dynamic-text` selects SDL_ttf and its pinned FreeType/HarfBuzz-related source for shaping/rasterization.
- No optional dependency is present merely because the generator knows how to use it.

Consult `dependencies.lock.json` for the exact list, revisions, source URLs, recursive-checkout policy, and licences in this repository.

## Toolchain

- Python: 3.11 or newer; generators use the standard library.
- CMake: 3.27 or newer for the common project.
- C++: C++20, compiler extensions off.
- Formatting, analysis, and coverage: the exact LLVM version in `.tool-versions` and `cmake/ProjectOptions.cmake`.
- Selected Apple, Windows, Linux, iOS, and Android toolchains are recorded in `docs/PLATFORM_SUPPORT.md` and CI.
- Shader compiler versions and generated artifact digests are build evidence.
- Android Gradle wrapper: 9.8.2, with independently verified binary ZIP and wrapper JAR SHA-256 pins enforced by the static audit. See [the Gradle toolchain decision](GRADLE_TOOLCHAIN_DECISION.md) for integrity, scope, update ownership and replacement checks.

A developer's ambient package manager, sibling checkout, global Gradle, floating branch, or unrecorded SDK is not a dependency lock.

## Admission And Upgrade

Record capability owner, alternatives, exact source/integrity, licences, all selected-platform behavior, deterministic/realtime effect, binary/package/startup cost, security/update owner, and replacement tests before adding a library. Upgrade one dependency at a time and rerun every affected platform, shader, package, and contract gate.

## Offline authoring tools

The existing Blender/NumPy/Pillow content path has a separate exact host-tool
contract in [OFFLINE_ART_TOOLCHAIN.md](OFFLINE_ART_TOOLCHAIN.md),
`tools/art-toolchain.json` and `tools/art-requirements.txt`. These tools are
installed only under ignored `build/tools/` and add no game runtime dependency.
