# Independent native gameplay release review — 25 October 2026

Run `python3 tools/gameplay_qa/release_review.py`. This standalone optimized C++
audit uses production gameplay, progression and protected storage, never the
Python balance simulator. It does not edit CMake or production sources and does
not require a foreground app. Source hashes and exact observations are retained
in `build/gameplay-qa/release-review/{sources.json,review.log}`. The completed
run has stable hashes during compilation/execution and **18 checks pass, zero
fail**. Controlled fixtures relax C++ access checking only in this audit binary.

## Actionable findings

1. **P2 — warning audio arrives almost at the shot, and laser audio depends on
   render rate.** `native/human_resources_client/src/app.cpp` checks
   `warning_seconds > 0 && warning_seconds < .04F`; normal warning durations are
   .9–1.4 seconds. This consumes virtually all reaction time before the cue, and
   a .1-second render step can skip the condition. It also requests a laser cue
   on every frame while `shots` is nonempty. Flash lifetime is .12 seconds;
   the .065-second mixer cooldown permits two cues for one volley. Source
   inspection confirms the scheduling error; no speaker/listening qualification
   is claimed. Smallest durable fix: authoritative monotonic shot-volley and
   warning-lock events, consumed once independently of cached render frames.
   Regression: one volley plus eight repeated 60 Hz snapshots must emit one
   cue; a warning emits at lock time once, including .1-second observations,
   pause and repeated snapshots. Sent to combat presentation owner.
2. **P1 qualification gap — fresh-run pacing has not met the target across
   strategies.** See actual native observations below. This is a release
   playtest/tuning gap, not proof that a numeric parameter is wrong. The simple
   bots are imperfect and can make poor slot/recipe decisions. Do not change
   rewards or pressure solely to fit these bots.
3. **P2 — first-run explanation does not teach the committed-warning escape.**
   Portrait onboarding says drag anywhere, capture/fire automatically. Pause
   instructions say stay near specimens and avoid coral shots/warning circles.
   The gameplay commits `warning_aim` at warning start; changing course after
   that lock is the useful tactic. Suggested concise instruction in an
   appropriate first-threat moment: “When a coral target locks, move away.”
   Source review only; actual comprehension needs a first-time player.

## Production API pacing: 96 fresh/maxed runs

Twelve untouched seeds (21–32) per row; 60 Hz native steps, real damage/death,
no revive, no stat/clock intervention. Simple steering seeks nearby biological
actors, predicts bullets .18 seconds and avoids committed warning points.
Policy also gives different draft priorities. Automatic baseline holds still.

| Account | Policy | Median minutes | Range minutes | Mean level | Evolutions across 12 |
| --- | --- | ---: | ---: | ---: | ---: |
| Fresh | Automatic baseline | 3.10 | 2.59–5.57 | 9.92 | 0 |
| Fresh | Collector | 3.18 | 2.61–5.56 | 11.75 | 2 |
| Fresh | Offense | 4.40 | 2.62–7.84 | 12.08 | 0 |
| Fresh | Defense | 5.26 | 3.13–7.70 | 13.17 | 1 |
| Maxed/all unlocked | Automatic baseline | 4.76 | 3.20–7.82 | 13.25 | 2 |
| Maxed/all unlocked | Collector | 6.57 | 5.21–9.05 | 19.17 | 15 |
| Maxed/all unlocked | Offense | 5.60 | 3.11–8.62 | 15.58 | 0 |
| Maxed/all unlocked | Defense | 5.91 | 4.29–9.15 | 16.33 | 3 |

All 96 runs die before 20 minutes. Observed peak population is 32 actors and
14 projectiles. This is useful evidence against stationary farming and runaway
population in these samples, not proof against all exploits or strong human
builds. Fresh collector receives relatively few picks before the first major
pressure spike and rarely completes an evolution; candidate slot choice bias
also matters.

A **counterfactual diagnostic** retires bosses or tanks without rewards using
test-only private access, keeping all other rules and bot policy the same.
Across the same twelve fresh collector seeds, removing bosses changes median
190.78 → 314.72 seconds; removing tanks changes it to 321.01 seconds. Production
first boss is at 150 seconds and tanks start at 100. This isolates those pressure
families as a meaningful structural cause of the short collector runs. These
modified runs are not real survival evidence or a proposal to remove threats.
Review clearer committed-warning escape, avoidable contact damage and the
first tank/boss encounter before globally raising hull or delaying every spawn.

## Native progression journeys: 360 complete attempts

Twelve independent accounts (four per collector/offense/defense policy), thirty
sequential attempts each, native begin/settle/purchase/reopen after every attempt.
No ad bonuses, revives, fake currency or offline balance model. All 360 receipts,
purchases and byte-identical reopened accounts succeed.

- First meaningful earned choice: 12.9–14.52 active seconds.
- First 80-credit permanent purchase: attempt one for all twelve accounts.
- First free blueprint: attempt one for eleven accounts, attempt two for one.
- All sixteen blueprints: attempt 11–15.
- Collector buys all twenty permanent ranks by attempt 22–26, earning
  7,648–8,831 total base credits in thirty attempts.
- Offense/defense earn 4,199–5,265 credits in thirty attempts and are still
  below the 6,000-credit cap. This payout gap is a tuning observation; the
  collector does materially more of the game's primary capture activity.

This is a reasonable working account loop; it does not establish replay appeal
or human skill progression. Purchase policy prioritizes relevant tracks but
buys another affordable track rather than waiting, so cap times depend on it.

## Adversarial and long-tail checks

- Public start clamps 255-valued Hangar ranks to accepted caps.
- Nonfinite/negative elapsed time does not advance clocks.
- Maximum finite control vectors preserve finite player state.
- Leaving the base capture radius resets capture progress.
- Paused advance emits no new presentation events.
- A reachable six-slot rank-III build (Laser Array, Rail Lens, Orbit Drones,
  EMP Ring, Beam Splitter, Tractor Lens), capped capture/fire research and
  **explicit test-only invulnerability** runs for sixty active minutes.
  Level 79, finite damage 89.76, peak 18 actors/9 projectiles, and every earned
  late draft retains three distinct options. No immortality/fun claim follows
  from that fixture.

Existing 52-check native probes already cover sixteen module effects, seven
research families, six recipes, slot/prerequisite/cap rules and XP/reward
exclusion. Existing eight account-session scenarios cover failed settlement,
checkpoint/live retry, cold recovery, foreign modification, explicit verified
reset, pending placements and established-save deletion. Those suites were
reviewed rather than duplicated or rerun without value. The new journeys add
repeated exact save/reopen through the operational account loop.

No foreground app, screenshot, new footage, device touch, accessibility reader,
thermal, real speaker, live monetisation, store or release qualification was
performed in this pass. Event consumer code is under another owner's active
implementation; snapshot pause behavior is checked, but this audit does not
replace that owner's focused cache/event tests.

## Proposed development-log entry

25 October 2026 — Independent native gameplay and account journey release review.
Scope: standalone production-C++ audit, no production/CMake edits or Python
simulation claims. Added `tools/gameplay_qa/release_review.{cpp,py,md}`. Eighteen
checks pass with stable source hashes; 96 natural runs, 360 saved/reopened
sequential attempts, thirty-six diagnostic pressure ablations and a controlled
one-hour six-slot stress fixture. Fresh medians 3.10–5.26 minutes, maxed
4.76–6.57; fresh collector tank/boss pressure merits human review. First choice
12.9–14.52 seconds, first purchase attempt one, all blueprints attempts 11–15.
Found render-driven duplicate laser cues and warning audio only .04 seconds
before firing; concrete event-based fix/regressions sent to combat owner.
Caveats: bots do not prove fun, fixture invulnerability does not prove natural
survival, existing failed-save suites reviewed rather than newly executed, no
listening/foreground/device evidence. Next: owner fixes audio scheduling and
reviews combined capture, then fresh human playthrough checks warning escape,
first tank/boss pressure and evolution discovery before retuning numbers.
