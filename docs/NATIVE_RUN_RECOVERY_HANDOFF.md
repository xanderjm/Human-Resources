# Native atomic run recovery

25 October 2026. This document owns the executable/UI integration. The progression account and Game codec contracts have separate owners. Native/package results below distinguish diagnostic checks from actual process recovery.

## Player flow and durable boundary

The executable starts an actual Game with the account's permanent ranks and blueprint mask, then commits its initial bytes, run ID, location and receipt together through AccountSession before publishing the run. A failed begin retains those exact initial bytes; Retry Save publishes the committed state and never chooses another seed.

Full checkpoints use `continuation_for(Game, location)` only when the cheap AccountSession due check requests the next ten active seconds, or at pause, background, earned-choice entry, choice acceptance/reroll and death. Save failure pauses the simulation and disables gameplay feedback. The AccountSession retains the exact failed request until Retry Save succeeds. End & Bank first commits the current full snapshot; if that checkpoint fails, banking waits for its successful retry. The codec/storage policy stays outside app.cpp.

A compatible cold load restores the exact Game and saved location, then presents a paused Resume / End & Bank choice. An earned draft remains `choosing_upgrade`; Resume returns its same three offers. Input contacts, axes, pressed commands, wall delta, presentation event cache, queued audio and haptic policy are cleared. Renderer actor instances and GPU animation streams are cleared after waiting retained frame fences, while prepared animation assets remain loaded. Same-ID/same-model restarts therefore cannot inherit a capture/startle clip.

An incompatible inner continuation offers Retry or explicit Back Up & Bank. AccountSession verifies preservation before settlement from recorded credits. Outer damaged/future-account reset remains a separate existing action. Pending writes expose Retry only. Dead compatible cold loads are settled by progression and route to results, never to Resume. Unavailable ad/provider actions remain absent.

Android Back is trapped through the pinned SDL hint and mapped from `SDL_SCANCODE_AC_BACK` into the existing semantic pause action. It pauses run/choice, resumes pause, returns from settings/Hangar/library/results, and banks from death. Protected writes reject navigation until resolved. Actual Android Back behavior still requires emulator/device verification.

## Isolated native recovery diagnostic

`--qa-continuation prepare-choice|inspect|resume|bank --qa-continuation-slot ID` is a focused diagnostic transport. Slot IDs allow ASCII letters, digits, underscore and hyphen, at most 32 characters. It uses `SDL_GetPrefPath("HumanResources", "HumanResources-QA")/continuation/ID`, stable across launches and separate from the ordinary account. It rejects mixing ordinary save roots or other automation modes.

The helper owns no Game or storage mutation. Prepare dispatches ordinary Start on a fresh title, sends movement through semantic input and waits at a naturally earned choice using ordinary elapsed time. Inspect dispatches no actions. Resume or Bank each dispatch its explicit semantic action once from restored pause. The common account load/restore logs `product-run-detail` before any diagnostic action. A successful stable state logs `product-continuation-qa-held screen=N slot=ID`; exact draft titles/descriptions, modules/research, XP, evolution, run/theme and stored-state digest accompany it. No copied save fixture, accelerated clock, implicit fresh run or godmode exists. The Android Java adapter accepts scenario extras only in debug builds.

## Verification scope

- Focused recovery UI tests restore a naturally earned actual Game draft, verify exact offers and saved Desert location, and inspect Resume/Bank and protected recovery semantics at 320×568, 390×844 and 844×390.
- Renderer animation tests clear an actor while it is capturing, retain the prepared assets, and prove the same ID/model starts idle after the boundary.
- The isolated helper strict C++ contract verifies no implicit Start on recovery, inspect performs no action, protected states block actions, Resume/Bank fire once, draft hold and escaped JSON.
- Strict C++ diagnostics and a standalone optimized headless recovery UI test passed while the authoring producer was unavailable. These were explicitly diagnostic only, recorded under `build/evidence/native-resume/`; they did not qualify a build or package. The genuine embedded-Python/PIL import failure was handed to the producer owner, which subsequently published a validated frozen producer.

Final coherent native build, focused CTest, real process-kill recovery and ordinary playthrough results are appended after execution. Physical touch, actual haptic feel, audio listening quality, mobile thermals and store release qualification are not inferred from these checks.

## Final host evidence

The frozen optimized native player built successfully against the repaired/frozen content producer. The first coherent attempt found one strict signed-char conversion warning in the diagnostic JSON helper; that was corrected explicitly and the helper's standalone test now includes the same conversion warning flags. `native-build.log` and `native-build-retry.log` preserve both results.

Four focused CTests passed: product recovery, accessibility, portrait layout and animation instances. The native SDL/Metal ordinary playthrough passed 280 frames with required screen mask 239/239. This covers actual title, run, pause, settings, earned upgrade, results and Hangar navigation with durable checkpoints.

`build/evidence/native-resume/host-process-recovery.json` records executable SHA-256 `0da1601ce4be2f4d7014e7f802d4a73152629bb264bdeafb7325aa8790067e5b` and each explicitly spawned PID. Prepare naturally earned a level-2 Desert draft at 16.016667502 active seconds. After SIGKILL, cold Inspect logged the identical run, clock, XP, three exact offers, module/research ranks and stored-state digest `145bfebd06ad4639`, with unchanged bank. Explicit Resume returned the same draft. Explicit End & Bank cleared the active run; another cold launch retained results for that run without repeating settlement. Credits were zero because this first draft precedes the economy's minimum; this host proof does not independently establish a positive currency award. The progression tests cover that arithmetic.

Two diagnostic-runner limits are preserved honestly: the initial runner used an incorrect numeric screen expectation and timed out after a valid durable draft (owned PID killed; raw log retained). The successful runner uses the semantic held marker. Its initial Inspect raw log filename was reused by the final Inspect; the parsed observations survive in the JSON, and the Resume/Bank raw logs independently include the same common cold-restore state. No application source changed between these runs. `frozen-sources.json` records the final owned source hashes.

Native/GPU ownership is released. The mobile owner now has the frozen source and debug-only scenario contract for actual Android force-stop/Back qualification. Host success is not Android/iOS device, store or signing qualification.
