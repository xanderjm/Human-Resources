# Mobile qualification — native UI resolution checkpoint

Current coordination, 25 October 2026: the production source is now Game7, with
desktop accepted combat/world/Flight Report checkpoints. Current compile/package
qualification passes both iOS Release profiles and Android both-ABI debug/release,
strict lint, 16KiB APK and unsigned AAB audits. Evidence is
`build/evidence/game7-flight-report-mobile/REPORT.json`; root independently checks
all525focused/307native inputs and227files across five artifacts. No new physical
execution or signing is claimed. The Game6 terrain/threat record remains a
separate historical epoch. A fresh read-only audit finds the paired iPhone with developer mode
enabled, but tunnel/DDI services unavailable; lock state is unknown. No Android
target is attached. `build/evidence/current-mobile-delivery/device-readiness.json`
records this newer observation. The authority-3 checkpoint below is historical;
its uses of "current" describe that exact recorded epoch.

The qualified Game7 mobile source is `053d08aa` (Flight Report). Later actual-hit
cause feedback and the supported-aspect framing fix cannot inherit that exact
package qualification; their desktop/source evidence remains separate until a
coherent mobile refresh.

This 25 October 2026 checkpoint refreshes the qualified authority-3 vehicle/Privacy build with native-resolution UI composition. Game authority remains **3**, outer account schema remains **2**, and the rules fingerprint is unchanged: `13238298123916137634`. It is a source-bound platform checkpoint, not a release-ready declaration.

The canonical freeze is `build/evidence/native-ui-release/canonical-source-freeze.json`, SHA-256 `4101d9462ffe110b3c8c2c694ba25381448ad24650e9c51350772088dfcef796`: **559 source/build/content inputs**. The native aggregate passes 39 suites. The renderer draws single-sample native UI after the capped world blit. World cap 1280, game rules, layout, fonts, atlas, shaders, manifest and runtime assets are unchanged. The manual store-export producer is included in source provenance and does not ship. Native pixels/store captures and outside-checkout Metal smoke were independently accepted by the coordinator.

All six preceding authority-3 artifacts, source/evidence closure and the actual legacy account proof were copied and rehashed before rebuilding. Their preservation ledger is `build/evidence/mobile-release/authority3-preserved-artifacts/artifact-ledger.json`, SHA-256 `ff53a7782280ab6ff3a5b04e1074062b726741d28fec10f6d80c6757f9f92fdf`. See [the previous authority-3 checkpoint](MOBILE_QUALIFICATION_AUTHORITY3.md) and [authority-2 results](MOBILE_QUALIFICATION_AUTHORITY2.md) for historical scope.

Runtime remains 58 models, eight preloaded skeletal pairs and 69 loaded IDs. Its manifest lists 80 game files plus the 1,703-byte recipe, 111,309,132 bytes and 98 derived inputs; SHA-256 `3147c2f6598e99ffb95f9ae6cfa0c052f4868b5ed5a36d2cac9683020b37c5fa`. The staged native runtime/shader set has 101 files. Source GLBs, editors and host tools do not ship.

## Builds and packages

| Gate | Current result | Scope |
| --- | --- | --- |
| iPhoneOS arm64 Release | Build and exact package audits pass | SDK 26.4, iOS 17 minimum; portrait, content/shaders, icon and privacy |
| Simulator arm64 Release | Build and package audits pass | Actual current launch rejects SDL_GPU Metal hardware capability |
| Existing development signing | Separate signed copy strict verification and package audits pass | No identity/profile/security changes; no distribution-signing claim |
| Physical iPhone | Historical locked-DDI gap retained | No repeated attempt without external state change |
| Android debug/release | Both arm64-v8a and x86_64 build/package audits pass | Current renderer, exact assets and compiled portrait/icon metadata |
| Android lint | Strict debug/release pass | No suppression or baseline added |
| Android alignment | All six ELF LOAD/RELRO checks pass per APK | Both APKs pass `zipalign -c -P 16`; actual emulator uses 4 KiB pages |
| Unsigned release AAB | Build, bundletool validate and source/config/manifest audits pass | Requests PAGE_ALIGNMENT_16K; no signing/split-install/store claim |

Artifact ledger: `build/evidence/mobile-release/native-ui-artifacts.json`. It includes individual app-tree file hashes and all six exact artifacts. The signed copy is `build/ios-device-native-ui-qualified-signed/human_resources_player.app`.

| Artifact | Bytes | SHA-256 |
| --- | ---: | --- |
| app-debug.apk | 61,761,471 | `7e69f594742f32db49f24dc4193b2fb69f872b886487df250f80c3bc71d05c09` |
| app-release-unsigned.apk | 49,455,477 | `4f969f97524631fbf9436407a5982babd4fac4171348e1b0f294e19544537a56` |
| app-release.aab | 48,069,797 | `d762f2c251a62a17f1a5a999f9964f232be882b7626036dc641fd0a491e4a7a1` |

The unsigned release APK grows by 22,728 bytes from the preserved baseline; packaged assets are unchanged. Artifact sizes do not establish per-device download or physical memory budgets. AGP 9.3.0, Gradle 9.8.2 and bundletool 1.18.3 remain pinned. Independent NumPy/Pillow/Blender evidence accompanies the required project evidence.

The iOS privacy audit still verifies exact packaged/source bytes, no tracking/collection declarations, only SystemBootTime `35F9.1` and FileTimestamp `C617.1`, and actual native imports. It runs through the project's XML-capable interpreter; no host repair or weakened gate is used. Store privacy declarations, public policy URL/contact and store acceptance remain separate.

## Actual current Android runtime

The final debug APK runs on the owned API-36 arm64 AVD, SwiftShader Vulkan, 390×844 framebuffer. Actual logs report **world=388×844, UI=390×844, UI samples=1**. The current `native-ui-android-active/active-run.png` framebuffer was inspected: the HUD, text and pause control render over the world without missing overlay/composition artifacts. This proves execution of the changed mobile render path; it does not establish physical high-density sharpness or performance.

`native-ui-android-active/active-recovery.json` records a real periodic checkpoint at 10,033 ms, actual OS background pause at 11,033 ms, byte-exact cold paused restore in a new process, and explicit Resume followed by another actual background checkpoint at 12,550 ms. The runner requires the checkpoint after actual onPause and strictly advancing resumed Game time.

`native-ui-android-recovery/process-recovery.json` records a naturally earned level-2 draft at 14,216 ms, digest `d3eab3896c5ef915`. All three exact offers, module ranks, XP, location and full file survive process death and action-free cold inspect. Explicit Resume retains the draft; real OS background persists it; End & Bank settles once and the following cold load is byte-exact. The early reward is zero credits, so this does not prove a positive payout.

`native-ui-legacy-cold/legacy-cold.json` records the current APK's common cold load of the actual preceding slot `a2-ui-701051285000`. It retains **7 credits and ranks `[1,0,0,0]`**, prior receipts and no active checkpoint. All 408 original bytes remain exact, SHA-256 `81ab922a552a5d5f05c89e9ea4b73aef47954001826856a87c08fe1ebfe598f9`. No fixture, new farming or purchase supplies this result. Inspect dispatches no Start. Font scale is confirmed `1.0` and the owned app is stopped at closure.

These native diagnostic scenarios use isolated preference directories, ordinary Game clocks and existing semantic actions; no injected save, forced offer, accelerated clock or new QA mode is installed. Release ignores their Android extras. This is instrumented native flow, not human touch/feel proof.

The first launch immediately after AVD boot encountered an Android AssetManager asset-path configuration update (`as.2`→`as.3`) during native initialization. Its Activity paused/destroyed/recreated, SDL finished and the VM exited zero before Game Start/checkpoint. No native fatal signal was found. Complete logs and the explicitly terminated waiting runner observation remain under `native-ui-android-early-boot-observation/`; it is not a passing recovery case. One normal fresh-process retry after boot settled then passed all active/draft/cold tests without source, system or save changes. This observation remains part of the startup robustness evidence rather than being erased by the retry.

## Remaining limits

Actual Android font-scale 1.3/2/recreation, held-pointer stale-input cancellation, Current Build pagination/OS Back and Privacy pagination/OS Back remain unexecuted. In the previous bounded method attempt, automatic approval review rejected ADB touchscreen injection; it was not used or bypassed. Supported CUA inventory exposes no Qt emulator app and documented binding returns **Invalid app**. No new input workaround or CUA retry was attempted in this pass. Native host pixels and portable tests cover their own scope only; historical authority-2 Back/card/purchase results are not credited to this renderer checkpoint.

Actual current Simulator launch reports `Device does not meet the hardware requirements for SDL_GPU Metal`; no backend check is bypassed. The physical iPhone's prior locked-DDI/passcode-required gap remains open. Existing development signing is separate from physical execution or distribution acceptance.

Physical GPU/frame/thermal/energy/memory budgets, physical 16 KiB execution, safe areas, human movement/feel, TalkBack/VoiceOver, predictive-back gestures and haptic feel remain unqualified. No store submission or release-ready claim is made.

## Exact closure

All disposable evidence is under `build/evidence/mobile-release/`. `native-ui-source-start.json` binds the canonical freeze; `native-ui-source-final.json` confirms all 559 inputs remain exact. `native-ui-qualified.json` is the required project tool/package/backend evidence, accompanied by `native-ui-independent-tools.json` and the shipping account-inspector build hashes. `native-ui-qualification-closure.json` binds current source, artifacts, runtime proofs, limits and the explicit production-freeze release.

Reproduce with the existing `GAME_SHADERCROSS_EXECUTABLE`, mobile build scripts, exact package audits, strict `:app:lintDebug :app:lintRelease :app:bundleRelease`, bundle audit and `project.py evidence`. New source changes require another coherent platform refresh. The coordinator owns shared development history and release readiness.
