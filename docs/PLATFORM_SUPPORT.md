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

## Current product qualification,25October2026

Latest accepted host source includes authority8 boss/Pause and later single-report
natural death/measured header fit. It passes optimized dev/47suites, actual
native portrait motion/UI review and an accelerated SDL/Metal player smoke. Independent phase continuations and four-process
SDL storage banking pass; a120-simulated-minute CPU audit remains host-only.
The owner-requested fafd40be playtest was confirmed at launch PID52722; a later
read-only check finds that process absent, without automated quit/relaunch.
Thirteen user previews and31discovered bundles are protected. The exact741-filefafd40be mobile checkpoint
now passes both iOS Release builds and Android both ABIs/variants/lint/16KiB/AAB
and package/source audits, with independent review. Its five packages/227entries
retain all81runtime payloads/eight animations. See `boss-ui-mobile/` and
`boss-ui-mobile-independent/` evidence. Supplemental Windows remainsb99c373;
isolated Android runtime now covers frozen fafd40be rendering, earned draft,
exact force-stop/cold paused restore, Resume and early zero-credit settlement.
The owned API36/software-Vulkan VM exits cleanly; root accepts50bound records and
actual frames under `boss-android-runtime-attempt2/`. Pending death-flow edits,
active-run background flush, positive payout, boss phases, physical runtime,
human fun and store acceptance are outside that diagnostic.

### Qualified b99c373 clean/platform epoch

Sourceb99c373 adds cumulative Hangar values, actual-device onboarding and
conservative shadow work. Its exact738-file clean archive freshly cooks/builds,
passes45native suites and source-bound112-file package checks. Current iOS device/
Simulator Release and Android both-ABI compile/package/lint/alignment/AAB audits
bind554focused inputs and all738tracked files. All81runtime payloads remain exact;
source closure is103authorities. Supplemental MinGW compiles/links the current
Windows GUI/DXIL client with package/internal-notice audits. These reports are
under `clean-hangar-shadow/`, `hangar-shadow-mobile/` and
`hangar-shadow-windows/` in `build/evidence/`. Independent host/mobile review and
root Windows package/object/archive recheck accept their compile/package scope.
At this epoch the owner's separate macOS playtest ranb99c373; eleven user previews
and their saves were protected. No physical-mobile/Windows/Linux execution, human feel
or store qualification is inferred. Actual isolated Android runtime remains853169.

Later direct-flight/held-surface/sweep-charge source has separate host45suite and
native/App-semantic acceptance, recorded in `ui-sweep-independent-review-b99c373/`.
It does not change the frozen b99c373 mobile/Windows epoch or open user preview;
matching C++ projection headers/archives remain required despite unchanged saves.

## Historical853169 qualification

Source853169 reviewed crop/grass exports, reward UI and Farmland motion pass
current iOS device/Simulator Release and Android both-ABI compile/package audits.
Root checks550focused inputs/five artifacts; independent review checks exact
asset deltas and fresh visibility objects consuming current generated bounds.
A734-file clean archive freshly cooks/builds and passes44native suites plus
source-bound package checks. The owner's separate Latest Playtest runs this
source on macOS; ten user previews/accounts are protected.

An isolated software-Vulkan Android QA application additionally exercises the
precedingaa5975e original-geometry earned draft/active recovery, stock2x rendering,
early banking and accelerated positive settlement/retry flow. A later853169
isolated same-native-assets smoke renders current geometry after settled startup;
the original earlier Activity-recreation trigger remains unknown. These scopes do
not qualify physical production-app performance/input, iOS runtime or store delivery.

Supplemental installed MinGW GCC16.2 strictly links the complete853169 Windows
GUI client and audits its112-file/DXIL package plus separate notice companion.
It does not replace the selected Visual Studio/SDK gate or prove Windows execution,
D3D12, Unicode storage, input, accessibility or distribution readiness. Linux has
no current target execution. RELEASE_READINESS.md binds exact epochs and limits.

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
