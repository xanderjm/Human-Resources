# Native gameplay and progression QA

25 October 2026 — first structural review complete against the source snapshot recorded in `build/gameplay-qa/source-hashes.json`. Production owners are being edited concurrently; rerun probes after relevant fixes.

Scope: `native/human_resources_gameplay` and `native/human_resources_progression`, against `docs/ARCHERO_FIRST_PLAYABLE_PLAN.md`, `design/balance/archero-ufo-system.json` (selected `hangar_25`) and `design/balance/endless-upgrade-system.json`. The initial review changed only QA/report files. Narrow coordinator-authorised follow-ups added explicit jeep/tank gates and converter override, the portrait candidate movement half-width, focused QA assertions, and a delegated portable account-session repair. Root CMake, client code and historical encounter input were not edited by this pass or its delegated session owner.

## Current result

**52 native checks pass, zero failures. Eight separate account-session failure/recovery scenarios also pass. No reproduced gameplay/progression finding remains open in this review.** All historical defects below have been fixed and their native regressions pass. This is a bounded structural review, not exhaustive correctness or human balance acceptance.

Later coordinated body-clumping correction: [biological overlap review](../tools/gameplay_qa/biological_overlap.md) records the source-derived bounded fix, eight focused test groups, same-seed overlap reduction and latest post-fix native regression hashes. That follow-up supersedes the older completion hashes below for current native code; root owns its final visual acceptance.

### Current timing authority

`design/balance/archero-ufo-system.json` now explicitly owns `director.jeep_start_second = 30` and `director.tank_start_second = 100`. `generate_tuning.py` overrides the corresponding `enemy_start_second` entries with these current values rather than importing historical 45/120-second gates. The native QA fixture verifies both generated values and actual spawn budgets immediately before/at each threshold. These are spawn-budget start gates, not guaranteed first-enemy arrival times. Historical `endless-encounter-assumptions.json` is unchanged (SHA-256 `d1313643e62b61a848332f18e106383e35bd5486bf6aac31fcb17eba038e14d1`). Native geometry and pacing remain provisional; this change does not rerun or requalify historical offline studies.

### Portrait movement candidate

The coordinator selected a 20.5m portrait camera width with gameplay `field_half_width = 8.25m`. This pass changes only the gameplay half-width in `runtime-spatial.json`; client/animation owners handle camera integration. The focused public-API fixture reaches both ±8.25m edges and confirms an outward dash cannot cross the bound. This is a coordinated candidate under the owner's portrait direction, not final art or device acceptance.

### Portable account-session handoff

The delegated repair supplies `account_session.h` / `account_session.cpp` and eight scenario tests. `AccountSession` retains one pending mutation and the latest full monotonic receipt, protects Retry Save from stale checkpoint recovery, rejects foreign/missing established saves and permits reset only through verified backup. `AccountStore::load(CheckpointRecovery)` defaults to existing cold recovery; protected live retries use `preserve_active`. Failed checkpoints can be superseded by full settlement or completed revive without losing the counters/placement.

See [ACCOUNT_SESSION_HANDOFF.md](ACCOUNT_SESSION_HANDOFF.md) for the exact API and registration. Client must capture `pending_operation()` before retry, then apply the successful start/revive continuation once. Pause actual gameplay while non-checkpoint operations are pending; do not transition to a guest account. Tests cover protected full settlement, active checkpoint retry, failed cold recovery, foreign stale settlement, corrupt/future verified reset, completed revive/bonus retry, vanished established saves and failed fresh-save replacement after verified backup. An optimized warnings-as-errors production-source build passes; the final eight-scenario executable was also rerun by the parent with exit zero. Client/CMake integration remains the coordinator's ownership.

## Confirmed defects fixed during this review

- **P1, unrestricted retreat:** Initially all eight backward-hold fresh runs survived twenty minutes with maximum score six; after an intervening RNG change, four of eight still survived. The gameplay owner added an independent runner-distance anchor and bounded fore/aft steering. The latest public-API batch has zero twenty-minute survivors for backward, forward and stationary policies (eight seeds each). The original escape pattern no longer reproduces in this sample; this does not establish human pacing.
- **P2, simultaneous vehicle completions:** Great Vacuum plus three nearly completed jeeps initially settled three vehicle captures in one tick despite the shared eight-second limit. The owner now rechecks the completion gate inside the beam loop; the same fixture settles exactly one. Captured vehicles still produce no kill/biological rewards.
- **P2, lost dash press:** A dash on the first of two 120 Hz frames initially vanished because the first frame had no fixed step. The owner now latches the press; the fixture passes. Pause/resume clears the latch.
- **P2, Great Vacuum preview:** Completing Beam Splitter now previews the actual six-target capacity instead of four. The same native preview-versus-result fixture passes.
- **P2, Hostile Acquisition preview:** The earlier card advertised 75%/35% jeep/tank thresholds even when the pick immediately evolved to 100%/55%. The gameplay owner now projects choices through the actual `evolve`/`sync` rules; the native full-health jeep preview assertion passes and the description uses the evolved tank threshold. This also covers the first-two evolution ordering rather than relying on isolated card exceptions.
- **Candidate gate mismatch:** The current explicit 30/100-second override now matches the accepted local plan; historical 45/120 inputs are retained unchanged.

Gameplay behaviour/description fixes above were made by the implementation owner. Only the final configuration/converter gate correction was made in this narrowly authorised QA follow-up.

## Checks executed

- Independently compiled the actual native progression save tests: pass, including process-death settlement, duplicate recovery prevention, failed-write rollback and corrupt/future save protection.
- Native QA probe: all sixteen module cards install and advance to III; all seven research families mutate native state; Beam Splitter increments 1→2→3→4; ten thousand exhausted-hardware drafts retain three distinct eligible research families.
- All six recipe ingredient pairs trigger their native evolution; the two-evolution cap/catalogue tie order passes.
- Direct native combat probes pass for half-strength added lasers, third-target piercing, drone damage, EMP III radius/damage, Heavy Winch tank eligibility/minimum duration and Mirrorball release.
- Conditional probes pass for Overtime/Reserve vehicle beam accounting, Repair Swarm hull/shield limits, Bounty score versus XP, Tractor Lens bounded pull, Thrusters movement, Phase Drive III, three-target Arc Coil damage and seeded Mirror Plating projectile reflection. Employee Benefits intercept/capture damage boost, Storm Drain slow/radius, Hostile Acquisition thresholds and Office Shredder boss damage all execute real native effects.
- Captured soldiers/vehicles do not also settle kill rewards; repeated lethal hits award once. Base credit formula, floor-half credit bonus and duplicate settlement/bonus rejection pass.
- Director fixture produces identical enemies for fresh/maxed Hangar states; paused gameplay does not advance active time.
- Jeep/tank spawn budgets remain zero just before 30/100 active seconds and begin at those accepted gates; generated tuning carries those same values.
- Portrait movement/dash remains bounded to ±8.25m in the current native candidate.
- Sixteen paired five-minute native runs have identical director attempt counts and schedule hashes at every compared step despite different account power, movement, drafts and combat. Death alone is suppressed in these schedule fixtures so both runs reach the same active-time horizon; public survival runs remain unmodified.
- XP overflow processes four consecutive earned levels individually without elapsed play time; choice sixteen doubles eligible research; near-cap research clips to the remaining gain and disappears; Heavy Winch uses the contextual Archero prerequisite; capped Hangar grants its intended stats without extra beams/lasers.

These checks execute production C++ functions. Controlled fixtures relax C++ access checking only in the independent QA build; production access and state ownership are unchanged. Public survival runs do not mutate internal state. No abstract simulator was used as runtime proof.

## Reproduction and evidence

Run `python3 tools/gameplay_qa/run_native_probe.py`. The probe is independently owned under `tools/gameplay_qa/`; root CMake/test registration remains the coordinator's decision. Evidence is written under ignored `build/gameplay-qa/`: `native-probe.log`, `source-hashes.json` and the test executable. The latest full build/run exits zero with stable source hashes and **52 passing checks, zero failures**, including the portrait bound. Sixteen paired five-minute director schedules match at every compared step. All twenty-four public-API runs (eight seeds each, stationary/forward/backward) die before the twenty-minute observation horizon after the runner anchor fix. These survival runs are observational and do not define a forced duration requirement. Repository static verification also passes.

Evidence SHA-256:

- `native-probe.log`: `a1468750e91ad05306f1544beecbb6c652e00f1d0d87c670897ceccf9b5c66b6`
- `source-hashes.json`: `cd77dcf89a1d2b05983684681600a341b9fdd9a1becaa7e9edf7dcc0bed7ccec`
- `native/tests/account_session_tests.cpp`: `6d8e7d45359d063227626f22956524d2c6b01f44b7ed2e908d6bee1bfccaa920`

### Test registration guidance for the coordinator/client owner

Prefer a separate host-only QA executable built from `tools/gameplay_qa/native_probe.cpp`, linked against the normal `human_resources_progression`/`human_resources_gameplay` libraries. Supply private fixture include paths for `native/human_resources_gameplay/src` and `native/tests`. On Clang/GNU, apply `-fno-access-control` **only to this QA executable's compilation**, then register it with CTest. Production libraries keep their normal compilation and public access boundaries; no new production accessor or mutable snapshot API is needed. This host fixture is not a shipping target or a mandatory MSVC/mobile gate. A future portable fixture seam should be coordinated with the gameplay owner rather than weakening its public interface. The current standalone script remains an independent reproduction path and changes no root build registration.

This is headless native logic evidence, not rendered client interaction, physical-device pacing, warning readability, performance, live SDK integration or release acceptance. Every module and research family has executed native behaviour in the fixtures, but not every rank/combination/geometry edge is exhaustively covered. Numeric balance remains provisional. This review does not qualify the separately evolving client/service integration or certify the game fun.
