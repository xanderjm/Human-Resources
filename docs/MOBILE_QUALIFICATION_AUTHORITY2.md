# Mobile qualification — authority revision 2

This page records the 25 October 2026 `authority2` checkpoint. The native source/art freeze passed the coordinator's 37-suite aggregate, and 506 source/build/content inputs remained unchanged through this platform pass. Later vehicle or other source changes require a new coherent refresh; these results cannot qualify a different binary. Physical mobile performance, human touch/feel and distribution/store acceptance remain open. This is not a release-ready declaration.

The checkpoint contains three locations, 55 models, eight preloaded skeletal pairs, installed gear, rifle/shotgun/launcher infantry and the accepted capture/release presentation. Protected outer accounts remain schema 2; the gameplay authority is revision 2. The cooked runtime lists 77 game files plus the recipe, 107,935,058 bytes and 93 derived source inputs. Runtime manifest SHA-256: `759cc7f58567faa988a28914eef6cfb0f8dd372563175dd6f86d3054d2f9927c`. Raw GLB/editor/host tools do not ship.

## Build and package results

| Gate | Actual result | Scope |
| --- | --- | --- |
| iPhoneOS arm64 Release | Build and exact package audit pass | SDK 26.4, iOS 17 minimum; portrait, current shaders/runtime and compiled icon provenance |
| Simulator arm64 Release | Build and exact package audit pass | Actual launch rejects SDL_GPU Metal hardware capability |
| Existing development signing | Separate copy signs; strict codesign and package audit pass | Existing installed identity/profile; no credentials or security settings changed |
| Physical iPhone | Previous actual locked-DDI gap retained | No repeated install attempt because no external state change was known |
| Android debug/release APK | Both arm64-v8a and x86_64 build/audit pass | Exact assets, compiled icon, portrait and native libraries |
| Android lint | Strict debug/release lint pass | No new suppression or baseline |
| Native alignment | All six ELF LOAD/RELRO checks pass | Both APKs also pass SDK `zipalign -c -P 16`; actual emulator uses 4 KiB pages |
| Unsigned release AAB | Build, official bundletool validate and source/manifest/config audits pass | Requests PAGE_ALIGNMENT_16K; no signing, split-install or store-submission claim |
| Actual Android interruption | Active and pending-draft checkpoint/process-death/cold-restore pass | Fresh isolated native QA slots, ordinary Game clocks, no injected save |
| Actual instrumented Android UI | Back, earned cards, Bank, funded purchase and cold persistence pass | Standard OS key/tap dispatch at observed UIAutomator native widget labels/bounds; no human touch/feel claim |

Artifacts are bound by `build/evidence/mobile-release/authority2-artifacts.json`:

| Artifact | Bytes | SHA-256 |
| --- | ---: | --- |
| Debug APK | 58,068,955 | `ebcbb0adff10e72ecef4ea81690ba6d32635d62f45cbe0a84d114a0f5bd22f74` |
| Unsigned release APK | 47,582,555 | `f4daa279d9f5d61dc83347d65dd6f491bcda03f8c16c174c1599fa2e5a75ded9` |
| Unsigned release AAB | 46,204,471 | `d2996eadd646f1a495920e53cdaf7283420b9d8b6ae47f3939517b7d6f788ce2` |

These are artifact sizes, not per-device download, resident-memory or physical performance budgets. Device/Simulator/signed-copy tree sizes and hashes are in the same ledger. The signed copy is `build/ios-device-authority2-qualified-signed/human_resources_player.app`. Pinned AGP 9.3.0, Gradle 9.8.2 and existing bundletool 1.18.3 remain unchanged; see [Android package policy/tool evidence](ANDROID_RELEASE_PACKAGE.md).

## Actual Android recovery and UI

`authority2-android-active/active-recovery.json` records a periodic active checkpoint at 10,016 ms. Actual OS onPause/onStop saves paused time at 10,216 ms. Force-stop and a new process restore that exact full file/run. Explicit Resume advances ordinary native active time to 11,733 ms before another real OS suspension saves it. The final runner requires a checkpoint after onPause, rather than accepting a paused startup log.

`authority2-android-recovery/process-recovery.json` records a naturally earned pending draft, actual process death, common restore before scenario actions, identical full state/cards/location, explicit Resume, real OS background checkpoint, End & Bank once and byte-exact settled cold reopen. This early draft's recorded reward is zero; it does not prove a positive payout.

`authority2-android-ui/ui-flow-report.json` records the actual input flow in one stable isolated slot. Standard Android Back (`input keyevent 4`) pauses a live run; force-stop/cold restore preserves it; a native labelled Resume tap advances active time from 3,216 to 5,216 ms. A naturally earned draft at 15,800 ms survives Back, process death and actual UI Resume with identical cards and Game digest `2d1a0396c2b1418d`.

Actual card taps then earn and bank 80 Research Credits. The enabled Weapon Calibration control costs 80; an actual tap commits wallet 0 and rank `[1,0,0,0]`. A second ordinary run banks seven further credits. After real force-stop, common Android load in inspect mode retains **seven credits and rank I**, with the entire file byte-exact. The cold Hangar displays rank I, and actual Back returns to the menu. Two finished runs total 283.733 active seconds, within the approved three-attempt/ten-active-minute cap. No fixture, forced rank/offer, accelerated clock, new QA mode or paid service supplies the result. This flow does not claim actual UI Retry or human drag quality.

The existing `prepare-choice` helper supplies only its ordinary Start/movement commands and does not stop movement after the first card. Cold-loading with this mode leaves its automatic Start unused; returning to title after Bank can therefore start another run. The stable-menu assertion correctly caught that diagnostic-only effect. The real extra run was finished through normal UI, and final cold/menu proof uses inspect, which never dispatches. This is not an ordinary shipping auto-start defect. Release ignores these Android extras. Preserve `diagnostic-auto-start-observation/`; use inspect for action-free terminal/menu probes.

Other preserved method observations distinguish the first informational fullscreen guide (normally dismissed through its observed **Got it** node) and an initial restored-screen hierarchy query before renderer initialization. Corrected tests wait for actual native initialization. `initial-readiness-observation/`, the undismissed-guide active observation and the first fast-Resume observation remain in ignored evidence. No production change or failing-gate suppression was used.

Actual frames include `focused-game.png`, `back-paused.png`, earned choices, `late-live-run.png`, funded Hangar and `cold-visible-hangar.png`. Native accessibility nodes and pointer activation are exercised; this is not TalkBack, predictive-back-gesture or physical touch qualification. One late software-emulator process observation is 63,873 KiB PSS / 188,980 KiB RSS; it excludes host SwiftShader GPU memory, is not a peak and does not establish a device budget.

## Remaining platform limits

The actual refreshed Simulator launch reports `Device does not meet the hardware requirements for SDL_GPU Metal`. The locked SDL backend and [official SDL GPU documentation](https://wiki.libsdl.org/SDL3/CategoryGPU) exclude Simulator; [Apple's Simulator Metal guidance](https://developer.apple.com/documentation/metal/developing-metal-apps-that-run-in-simulator) describes its different GPU family. No backend check was bypassed.

The paired iPhone previously failed actual installation at DDI mount with `0xe80000e2` / `kAMDMobileImageMounterDeviceLocked`; the recorded lock API required the passcode. That evidence remains a gap, not a fresh device pass or a claim that unlocking alone guarantees success. Existing development signing works separately. Distribution signing and store acceptance remain unqualified.

Android evidence uses the owned API-36 arm64 AVD, software SwiftShader Vulkan and 390×844 framebuffer. Physical GPU/frame/thermal/energy/memory budgets, physical 16 KiB execution, device safe areas, human movement/feel, screen-reader interaction and haptic feel remain separate. Supported [Android input commands](https://android.googlesource.com/platform/frameworks/base/+/81f52b053da6/services/core/java/com/android/server/input/InputShellCommand.java), [UIAutomator inspection](https://developer.android.com/reference/androidx/test/uiautomator/UiDevice) and [Back key code](https://developer.android.com/reference/android/view/KeyEvent#KEYCODE_BACK) were checked against primary sources for this pass.

## Exact evidence and closure

- Native aggregate: `build/evidence/infantry-spatial-native/aggregate-canonical.log`, 37/37.
- Source: `authority2-source-{start,final}.json`, 506 inputs. Final SHA-256 `2eca4d496ef07b79144806615cc85f6eda5f7703894df9b7623ee98ec7c1562d`.
- UI report SHA-256: `aacf90a1b9b5058adb2f9bde095274b4f6fe9462f61b6207fc8e0f3d539366c2`.
- Required `project.py evidence`: `authority2-qualified.json`, SHA-256 `30c7635dcbf4ad18b464640c0a21e453676017d00cf9b7cdf4847987a40ef385`.
- Rechecked source/artifact closure: `authority2-qualification-closure.json`, SHA-256 `b886fd705b18e2dc1b221e7a0ab0d262de05519c08392ff417c48d9153a64b95`. Production freeze was explicitly released after these immutable snapshots; follow-on vehicle work needs another platform refresh.

All `authority2-*` evidence lives under ignored `build/evidence/mobile-release/`. [Previous revision-1 qualification](MOBILE_QUALIFICATION_SCHEMA2_REV1.md) and all original account files remain historical with their exact hashes. Independent prior-byte compatibility evidence is `build/evidence/revision2-account-compatibility/compatibility-report.json`: revision 2 protects the old Game continuation, preserves the compatible outer account/backup and banks its recorded checkpoint once. It is an isolated fixture test, not a new device recovery claim.

Reproduction uses `scripts/build-ios-device.sh`, `scripts/build-ios-simulator.sh`, `scripts/build-android.sh all`, each native audit, pinned `:app:lintDebug :app:lintRelease :app:bundleRelease`, then `scripts/verify-android-bundle.sh`. Select the existing host shadercross executable to avoid unnecessary tool rebuilds. The coordinator owns the shared development log and overall release matrix.
