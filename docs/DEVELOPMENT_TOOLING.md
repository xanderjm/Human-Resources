# Development Tooling

`tools/project.py` is the portable user and AI interface. It reports selected profiles from `project.json` and delegates to CMake, CTest, platform package owners, and deterministic audit scripts without reproducing their target graphs.

## Common Commands

```bash
python3 tools/project.py status
python3 tools/project.py doctor
python3 tools/project.py bootstrap
python3 tools/project.py build --preset dev
python3 tools/project.py test --preset dev
python3 tools/project.py run --preset dev
python3 tools/project.py verify
python3 tools/project.py package --platform <selected-host-platform>
python3 tools/project.py evidence --output build/evidence/<platform>.json
```

Platform packaging uses `python3 tools/project.py package --platform <selected-platform>`. Unsupported host/target combinations fail with a clear prerequisite rather than silently skipping work.

`python3 tools/project.py publish --repository owner/name` is the explicit GitHub boundary. It requires a clean committed repository, authenticated GitHub CLI, no existing `origin`, and creates a private repository unless another visibility is supplied.

Runtime content under `assets/runtime` is copied by one deterministic cooker. Hidden files and symlinks are rejected; the output manifest records stable IDs, paths, sizes, and SHA-256 digests. Shader cooking emits the same exact-file contract. Desktop, iOS, and Android package audits reject missing, changed, or unlisted asset files.

`python3 tools/project.py evidence` writes a bounded JSON record under ignored `build/evidence`. Selected platform workflows upload it with smoke output so a CI pass retains exact runner, compiler, SDK/driver, dependency, host-tool, and artifact hashes.

Desktop smoke lanes set `FOUNDATION_SMOKE_REPORT` to an ignored evidence path. The client writes its actual SDL_GPU backend, device, and reported driver directly to that file, avoiding platform-specific console redirection; `--smoke-report <path>` provides the same behavior for local automation.
The workflow then audits the report against the profile's required backend, so an unexpected renderer cannot be archived as a passing platform result.

## Quality Policy

- Product-owned compiler warnings are errors in qualified lanes.
- Format and analysis use the exact LLVM version pinned in `.tool-versions`.
- Clang-tidy runs through real compile commands.
- ASan/UBSan and source coverage run where the selected compiler/platform supports them.
- Tests treat an empty suite as an error.
- Renderer/device smoke tests add evidence and do not replace headless capability tests.
- Release verification starts from clean disposable outputs and records dependency, compiler, SDK, shader-tool, and package manifests.

Thin scripts may delegate to `tools/project.py` for familiar entry points. New orchestration belongs in the portable tool, not parallel Bash and PowerShell implementations.

## Shared app/game backports in 2.1

`tools/game.py` is a compatibility alias for `tools/project.py`; `project.json` is the single identity/profile authority. The old generator command remains available, while new products should use `create_project.py --kind app|game`.

Bootstrap locks dependency/tool writes per repository and builds host compilers from fresh copies in a whitespace-free scratch directory. Only runnable tool bundles are installed. CMake caches and temporary source aliases are not published. `GAME_HOST_TOOLS_STAGE_ROOT` selects the scratch parent and `GAME_HOST_TOOLS_DIR` selects installed tools; the latter is passed through to host CMake builds.

Android commands discover JDK 17 from an explicit `JAVA_HOME`, PATH or standard macOS installations, and the SDK from explicit environment settings or standard SDK directories. Invalid explicit choices fail rather than silently changing tools. Only the Android subprocess environment receives these selections.

Every advertised build preset has a matching nonempty CTest preset. Workflow initialization selects a compatible Xcode before requesting Metal components and creates Android SDK environment variables at step scope.

Runtime cooking and staging remove retired files. Source inventories bind the final package to current authored content, so a self-consistent stale package is rejected. Desktop stage/archive publication restores the prior pair on ordinary replacement failures; any failed rollback retains recovery files. This is process-failure recovery, not a power-loss durability guarantee. Product data is never stored in these disposable build folders.

## Incremental shader and runtime output contracts

The shader custom command declares its reflection header alongside packaged shader outputs. Reflection consumers rebuild in the same invocation after shader changes, and deleted headers are regenerated. Runtime cooking declares every allowlisted cooked file as a byproduct, so file consumers and deleted cooked outputs have an explicit producer. SDK-free CMake/Ninja regressions exercise these dependency graphs; stub compilers/dependency targets in those fixtures do not qualify native compilation.

For iOS, `CMAKE_OSX_SYSROOT` selects device or Simulator shader compilation. The cooker accepts `--apple-ios-sdk iphoneos|iphonesimulator` (default `iphoneos`) and uses the corresponding `air64-apple-ios<deployment>` target, adding `-simulator` for Simulator, through both normal and fallback compiler paths. Cooked paths remain `metal/ios/` and require a matching iOS app build. Compile/package evidence does not establish that the pinned SDL_GPU backend can run on a particular Simulator.
