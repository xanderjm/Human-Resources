# Historical schema-2 / Game revision-1 mobile qualification

This archived checkpoint is superseded by MOBILE_QUALIFICATION.md. Its results apply only to its recorded source/artifact hashes.

The recorded portrait candidate contains three locations, installed gear/effects, six preloaded skeletal pairs, optional haptics and schema-2 full-run continuations. Native builds and instrumented Android functionality pass the checks below. Physical mobile execution/performance, human touch and distribution/store acceptance remain open; this is not a release-ready declaration.

## Final candidate and gates

The final source epoch follows the lifecycle correction. `build/evidence/mobile-release/schema2-lifecycle-source-{start,final}.json` binds 401 native/build/content/tool inputs. Shipping runtime inputs remained unchanged throughout the final builds and Android checks. The sole later audit-wrapper change canonicalizes a caller-relative AAB path before invoking bundletool; it changes no application artifact. Root aggregate verification passes all 34 suites (`build/evidence/release-resume-final-verify.log`).

| Check | Actual result | Scope/limit |
| --- | --- | --- |
| iPhoneOS arm64 Release | Build and exact package audit pass | Unsigned canonical bundle; device execution blocked |
| iOS Simulator arm64 Release | Build and exact package audit pass | Actual launch rejects SDL_GPU Metal hardware capability |
| Existing iOS development signing | Separate final app copy signs; strict codesign and package audit pass | Existing installed identity/profile only, covering paired device; no credentials/settings changed |
| Paired iPhone installation | Fails before app delivery at locked Developer Disk Image mount | `0xe80000e2`, `kAMDMobileImageMounterDeviceLocked`; current lock API reports passcodeRequired=true |
| Android debug and unsigned release APKs | Both arm64-v8a/x86_64 builds and package audits pass | Product shaders/runtime/icon provenance, compiled launcher, actual portrait manifest |
| Strict Android lint | `lintDebug` and `lintRelease` pass | No new suppression or baseline |
| Native page alignment | All six APK/AAB ELF LOAD and RELRO layouts pass | Both APKs pass SDK `zipalign -c -P 16`; emulator itself uses 4 KiB pages |
| Unsigned release AAB | AGP build, official bundletool validation, manifest/config and exact source-bound assets pass | Requests PAGE_ALIGNMENT_16K; no signing blocks; no upload/split-install/store claim |
| Android full native UI flow | 280 frames, mask 239/239, pass | Existing accelerated semantic flow, software Vulkan, no human input/pacing claim |
| Android full continuation | Actual process death and cold restore pass | Fresh stable native QA slots, no copied checkpoint fixture |
| Actual Android OS background | Active and pending-draft cases pass after lifecycle fix | Real Settings activity launches invoke onPause/onStop; no touch/key injection |
| Positive native bank/Hangar | Eight credits banked and persisted; ranks remain zero | No funded purchase was affordable; separate portable tests cover funded purchases |

Exact paths, package sizes and hashes are in `schema2-lifecycle-artifacts.json`. The canonical device/Simulator bundles contain approximately 104.3 MB each; debug APK is 55,201,291 bytes, unsigned release APK 43,751,517 bytes and unsigned AAB 42,373,795 bytes. These are artifact sizes, not per-device Play download or resident-memory budgets. The current cooked runtime has 71 files / 99,374,549 bytes bound to 80 derived source inputs (`schema2-lifecycle-content-stats.json`). No source GLB/editor/host tools ship.

## Actual Android continuation evidence

`schema2-lifecycle-android-active/active-recovery.json` records a real elapsed-time run with a 10,016 ms periodic checkpoint. A real OS background transition commits the paused Game at 10,716 ms. After force-stopping the owned process, a new Android process restores the same run, exact saved bytes and active time, with bank/unlocks unchanged. Explicit Resume followed by three seconds of native running advances to 11,933 ms, then the actual background callback commits another paused checkpoint.

`schema2-lifecycle-android-recovery/process-recovery.json` records a naturally earned draft at 17,016 ms, including exact options/descriptions, complete Game digest, modules and location. After real force-stop, common AccountSession load/restore opens pause before any scenario action and preserves the original file/full state. Explicit Resume exposes the same draft. A later real background callback is required to log a new full checkpoint after onPause. Explicit End & Bank settles that run once; another cold load preserves the settled bytes and cannot award again. This short draft earned zero credits; completed-run/receipt/continuation checks establish once-only settlement, not a positive-credit continuation award.

`schema2-lifecycle-android-flow/flow-result.json` records the existing 280-frame native flow through title, movement, pause/settings, earned choices, results, Hangar and restart. It banks eight actual credits, makes no Hangar purchase and begins run 2. The final schema-2 file retains eight credits and one completed run after native exit. A fresh read-only shipping AccountStore/Game validator accepts that actual file unchanged. Its smoke backend is intentionally unique per launch, so this is durable native-file/portable cold-inspection evidence; actual Android positive-wallet cold launch is not asserted. Actual Android cold launch is separately demonstrated by the stable zero-reward continuation cases above.

`tools/native_account_inspect.cpp` uses the actual shipping account and gameplay validators, supports explicit legacy schema 1 and current schema 2, and requires exact complete Game round-trip preservation. It cannot write, migrate or settle an input file. `scripts/build-account-inspector.sh` records tool/library hashes under `build/mobile-qa-tools/`. Its 2,642-byte portable fixture is only an inspector test and was never installed for the native process-death cases.

## Defects found and fixed

Strict lint first rejected redundant API-26 icon qualifiers, obsolete flattened fallback/round icons and a base adaptive variant without monochrome. The producer now emits one API-26-minimum adaptive resource with all three layers. The first qualifier migration also exposed an actual generated-resource dependency defect; AndroidComponents generated-source task registration now survives deletion of the entire generated icon tree on its first invocation. Actual native build/lint pass; no checker was suppressed. See [launcher packaging](APP_ICON_PACKAGING.md).

The first active Android interruption found that a focus loss before a run could consume a shared background flag, causing a later real OS suspension to skip saving. That APK, native logs and pulled bytes remain in `schema2-android-active/`. The corrected adapter applies the portable lifecycle transition once, independently handles real presence/suspension persistence, and deduplicates repeated focus/WILL/DID events. Pending-save and settlement guards remain. The bounded regression, root aggregate and actual Android retest pass. See `build/evidence/android-lifecycle-fix-handoff.json`.

## iOS and physical limits

The final Simulator app actually reaches the renderer and reports `Device does not meet the hardware requirements for SDL_GPU Metal`. The locked SDL source and [official SDL GPU documentation](https://wiki.libsdl.org/SDL3/CategoryGPU) exclude iOS Simulator; [Apple's Simulator Metal guidance](https://developer.apple.com/documentation/metal/developing-metal-apps-that-run-in-simulator) describes Apple family 2 while the backend requires Apple family 3. Native logs, rather than the launcher exit code, establish the failure. No vendor feature check was bypassed.

The paired iPhone 17 Pro reports iOS 26.6.1 and enabled developer mode. Existing matching development signing works, but current lock-state and actual installation show the DDI lock blocker. No unlock/security/certificate changes were attempted. Final signed app copy: `build/ios-device-schema2-qualified-signed/human_resources_player.app`; signed app audit: `schema2-lifecycle-ios-signed-audit.log`. Distribution signing remains separate.

The isolated Android API-36 arm64 AVD uses software Vulkan and a 390×844 test framebuffer. Its first-use fullscreen tutorial obscures screenshots; CUA could not bind the Qt emulator, and no ADB touch/key injection or system-warning bypass was used. Human mobile input, actual Android Back gesture, device safe areas/accessibility, haptic feel, thermal/energy/memory/frame budgets and physical 16 KiB execution remain unqualified. Existing portable controls/Back tests and compiled SDL trap binding are not physical input proof.

## Reproduction and retained evidence

```sh
GAME_SHADERCROSS_EXECUTABLE="$PWD/build/host-tools/shadercross/shadercross" scripts/build-ios-device.sh
scripts/verify-ios.sh
GAME_SHADERCROSS_EXECUTABLE="$PWD/build/host-tools/shadercross/shadercross" scripts/build-ios-simulator.sh
scripts/verify-ios.sh build/ios-simulator/Release-iphonesimulator/human_resources_player.app
GAME_SHADERCROSS_EXECUTABLE="$PWD/build/host-tools/shadercross/shadercross" scripts/build-android.sh all
scripts/verify-android.sh android/app/build/outputs/apk/debug/app-debug.apk
scripts/verify-android.sh android/app/build/outputs/apk/release/app-release-unsigned.apk
scripts/verify-android-bundle.sh
```

Build the unsigned AAB using the pinned Gradle `:app:bundleRelease` task in the same scoped Android environment. AGP is 9.3.0, Gradle 9.8.2 and its existing bundletool 1.18.3. [Current Play/API and page-size requirements](ANDROID_RELEASE_PACKAGE.md) were checked against official sources; no signing credentials were invented and nothing was submitted.

All generated logs/captures/reports remain ignored under `build/evidence/mobile-release/`. Final `schema2-lifecycle-*` files supersede earlier candidate qualification; `candidate-before-preload/`, older schema-1 recovery fixtures and the reproducing lifecycle-defect APK remain historical with their original limits. The coordinator owns the shared development-log entry and overall release matrix.

Required `project.py evidence` output: `schema2-qualified.json`, SHA-256 `fda105a35476f0e153adeab2b3d734643e1f222c11361c511d05eaa427442be4`. `schema2-qualification-closure.json` records the final source/artifact recheck and explicit shipping-freeze release; its source snapshot SHA-256 is `3b1c262385aca3ba8a7f7188140441fa5a6a49162ac15eb49c9be257bead6233`. Subsequent production changes require a new coherent qualification pass and cannot inherit these exact-artifact results.
