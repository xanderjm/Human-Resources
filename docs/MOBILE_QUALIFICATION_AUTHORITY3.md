# Mobile qualification — authority revision 3

This records the 25 October 2026 final vehicle/Privacy checkpoint. It is a source-bound platform checkpoint, not a release-ready declaration. Physical mobile performance, human controls/feel, full accessibility interaction and distribution acceptance remain open. [Authority-2 results](MOBILE_QUALIFICATION_AUTHORITY2.md) and the original packaged binaries are preserved separately.

The canonical native freeze is `build/vehicle-assembly-audit/vehicle-release-source-freeze.json`, SHA-256 `3bc3dae5424a05cf24986cea1414238f18facffd742698fd55dbcc208662e490`: 556 source/build/content inputs, unchanged Game authority 3 fingerprint `13238298123916137634`. The final native aggregate passes 39 suites; the coordinator accepted the vehicle, optical effects, ordinary 190-second flow, revised UI and offline Privacy screens. This platform pass uses their final sources, including the manifest correction and audit-interpreter fix, rather than the earlier interim binaries.

Runtime contains 58 models, eight preloaded skeletal pairs and 69 loaded IDs. The runtime manifest lists 80 game files plus the 1,703-byte recipe, 111,309,132 bytes, 98 derived inputs; SHA-256 `3147c2f6598e99ffb95f9ae6cfa0c052f4868b5ed5a36d2cac9683020b37c5fa`. Its native staged runtime/shader set has 101 files. These are different counts. Source GLBs, editors and host tools do not ship.

## Build and package results

| Gate | Current result | Scope |
| --- | --- | --- |
| iPhoneOS arm64 Release | Build and exact package audits pass | SDK 26.4, iOS 17 minimum; portrait, content, shaders, icon and privacy |
| Simulator arm64 Release | Build and package audits pass | Actual final launch rejects SDL_GPU Metal hardware capability |
| Existing development signing | Separate final copy strict codesign and package audits pass | No identity/profile/security changes; no distribution-signing claim |
| Physical iPhone | Historical locked-DDI gap retained | No repeated attempt without external state change |
| Android debug/release | Both arm64-v8a and x86_64 build/package audits pass | Exact assets, compiled portrait/icon metadata |
| Android lint | Strict debug/release pass | No suppression or baseline added |
| Android alignment | All six ELF LOAD/RELRO checks pass per APK | Both APKs pass `zipalign -c -P 16`; emulator remains 4 KiB |
| Unsigned release AAB | Build, bundletool validate, source/config/manifest audit pass | Requests PAGE_ALIGNMENT_16K; no signing/split-install/store claim |

Final artifact ledger: `build/evidence/mobile-release/authority3-final-artifacts.json`. App-tree hashes and individual file inventories are included there; the signed copy is `build/ios-device-authority3-final-qualified-signed/human_resources_player.app`.

| Artifact | Bytes | SHA-256 |
| --- | ---: | --- |
| app-debug.apk | 61,731,887 | `59d83c27a0a02e8ee026416f5dc31788b467ffedb194d1e57ed828d4d5e34c3e` |
| app-release-unsigned.apk | 49,432,749 | `bb955cc44a26623fbd1f42898e1e34e12abc54b6ff63a8ca9ccaa839062c1a30` |
| app-release.aab | 48,058,343 | `a2f4e1300cb2c60ea785a6aab7ee6a74c46e9fc694ab1117fd6c6193991853da` |

Sizes describe artifacts, not per-device download or physical memory budgets. AGP 9.3.0, Gradle 9.8.2 and bundletool 1.18.3 remain pinned. Independent NumPy/Pillow/Blender evidence accompanies the final `project.py evidence` record.

The final privacy audit checks exact packaged/source manifest bytes, no tracking/collection declarations, only SystemBootTime `35F9.1` and FileTimestamp `C617.1`, and the actual executable's `_mach_absolute_time`/file-stat imports. It initially exposed the host's broken default Python 3.14 `pyexpat`; `verify-ios.sh` now selects the same healthy XML-capable project interpreter as native generation. The failing command was reproduced and retained; no system Python repair or privacy-gate weakening occurred. Store privacy declarations, a real public policy URL/contact and store acceptance remain separate distribution work.

## Current Android runtime scope

The final debug APK runs on the owned API-36 arm64 AVD using SwiftShader Vulkan and the existing 390×844 display override. Native diagnostic scenarios use isolated preference subdirectories, ordinary Game clocks and naturally earned choices. They use common save/restore and semantic Resume/End & Bank commands; no save fixture, forced offer, clock acceleration or new QA mode is installed. Release ignores the diagnostic extras.

`authority3-final-android-active/active-recovery.json` records active periodic time 10,066 ms, actual OS background pause at 10,566 ms, byte-exact cold paused restore in a new process and explicit Resume followed by another OS background checkpoint at 12,083 ms. The runner requires the checkpoint after actual onPause and strictly advancing resumed Game time.

`authority3-final-android-recovery/process-recovery.json` records a naturally earned level-2 draft at 15,016 ms, Game digest `cc2e6a5a6611018b`. Its three exact offers, module ranks, XP, location and full file survive process death and action-free cold inspect, then explicit Resume. Actual OS background persists the pending draft; explicit End & Bank settles once and the next cold load is byte-exact. The early reward is zero credits; this case does not claim a positive payout.

`authority3-final-legacy-cold/legacy-cold.json` records the updated APK's actual common cold load of the preserved authority-2 slot `a2-ui-701051285000`. It retains **7 credits, ranks `[1,0,0,0]`**, prior receipts and no active checkpoint. All 408 original bytes remain exact, SHA-256 `81ab922a552a5d5f05c89e9ea4b73aef47954001826856a87c08fe1ebfe598f9`. Inspect dispatches no Start. This proves update/cold preservation of the existing actual account, not a newly earned authority-3 positive award or purchase. Earlier `authority3-android-*` process results belong to the interim pre-Privacy/optics binaries and are retained as historical evidence; only `authority3-final-*` results qualify this checkpoint.

## Unexecuted UI and physical gates

An earlier ADB touchscreen-injection plan was rejected by automatic approval review. It was not executed or bypassed. The approved bounded fallback restarted the same data-preserving AVD visibly, queried supported CUA inventory and attempted documented binding to the observed emulator executable. The inventory exposes no Android emulator app and `cua.getApp` returns **Invalid app**. `authority3-final-ui-method.json` records this limit.

Therefore actual Android font-scale 1.3/2 preference/recreation, held-pointer stale-input cancellation, Current Build pagination/OS Back and Privacy pagination/OS Back remain **unexecuted on the final Android build**. Native host pixels and portable accessibility tests cover their own scope; they do not fill these mobile UI gaps. The original owned-AVD font scale remains exactly `1.0`; no font setting was changed. Authority-2 instrumented Back/card/purchase results remain historical and are not credited to authority 3.

Actual final Simulator launch reports `Device does not meet the hardware requirements for SDL_GPU Metal`; no backend check is bypassed. The paired iPhone's prior actual DDI lock/passcode-required failure remains open; existing development signing is a separate pass. No new unlocked-device evidence is claimed.

Physical GPU/frame/thermal/energy/memory budgets, physical 16 KiB execution, safe areas, human movement/feel, TalkBack/VoiceOver, predictive-back gestures and haptic feel are unqualified. Software-emulator observations cannot establish those budgets. No store submission or release-ready claim is made.

## Evidence

All disposable evidence is under `build/evidence/mobile-release/`. `authority3-source-start.json` binds the canonical freeze and the explicit audit-only interpreter rebaseline. `authority3-source-final.json` confirms every one of the 556 inputs still matches. `authority3-final-qualified.json` is the required project tool/package/backend evidence; `authority3-final-independent-tools.json` accompanies it. `authority3-final-qualification-closure.json` binds exact source, artifact, runtime and evidence hashes and the explicit production-freeze release. The coordinator owns shared release readiness and development history.

Reproduce with the existing `GAME_SHADERCROSS_EXECUTABLE`, iOS/Android build scripts and exact package audits, strict `:app:lintDebug :app:lintRelease :app:bundleRelease`, bundle audit and `project.py evidence`. New source changes require a coherent refresh.
