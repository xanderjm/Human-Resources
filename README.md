# Human Resources

Human Resources is an overhead UFO abduction survival game in development. This repository combines its existing art, models, fonts and balance work with a self-contained C++20/SDL3 foundation generated from Game Scaffold's current Native Foundation 2.1.0-dev source.

Midnight Menu is the implementation reference. Its optimized development presets and manual build workflow policy are adopted here. The current scaffold already includes its reusable input, asset safety and mobile shader/build fixes.

- Build profiles: macos, windows, linux, ios, android.
- Optional foundation features: none.
- App ID: `com.xanderjm.humanresources`; version: `0.1.0`.
- iPhone is the initial mobile priority; portrait is the primary orientation.
- The executable is the native playable game: three locations, movement/capture/combat, run upgrades, protected account progression and skeletal animation. Release qualification and visual equipment integration remain in progress; see [the readiness ledger](docs/RELEASE_READINESS.md).

## Local development

Run commands from this repository root. Python 3.11+, CMake 3.27+, Ninja and the target compiler/SDK are required.

```sh
python3 tools/project.py status
python3 tools/project.py doctor
python3 tools/project.py bootstrap
python3 tools/project.py test --preset dev
python3 tools/project.py run --preset dev
```

`dev` uses RelWithDebInfo, following Midnight Menu. Use `debug` for unoptimized debugging, `release` for release builds and `asan-ubsan` for SDL-free sanitizer contracts. Each has a matching test preset.

```sh
python3 tools/project.py test --preset asan-ubsan
python3 tools/project.py verify --static-only
python3 tools/project.py package --platform macos
```

The full `verify` gate, pinned formatting/analysis, native smoke checks and device qualification remain available. GitHub workflows run manually through Actions; pushing does not start hosted builds.

## Mobile builds

```sh
python3 tools/project.py doctor --platform ios
scripts/configure-ios-simulator.sh
scripts/build-ios-device.sh
scripts/verify-ios.sh

python3 tools/project.py doctor --platform android
scripts/build-android.sh all
scripts/verify-android.sh
```

Generated Xcode projects, Gradle output, dependency checkouts and cooked assets are ignored build material. Edit source-controlled CMake, manifests and source assets. Signing, store setup and physical-device acceptance are separate work.

Start with [the product brief](PRODUCT_BRIEF.md), [the setup guide](docs/PROJECT_SETUP.md) and [the development log](docs/AI_DEVELOPMENT_LOG.md). `AGENTS.md` and `project.json` define development boundaries and selected profiles.
