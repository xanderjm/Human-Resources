# Human Resources — Archero-based first playable

Date: 24 October 2026. Status: progression model accepted; numerical tuning is a playable candidate.

## Decision and purpose

Build a mobile-first endless UFO runner using **temporary choices during a run and permanent ship improvements between runs**. Archero is the progression reference. Keep our moving, auto-firing saucer, biological abductions and military pressure; do not adopt its stationary-fire requirement or room-based mission structure.

The player loop is: **start → capture and fight → choose upgrades → survive escalating pressure → die → bank credits → optionally improve the ship → retry**. Aim for a regular fresh-account run lasting **5–10 active minutes**, with stronger builds and skilled players surviving longer. There is no forced ending, maximum level or required minimum session length. Menus, upgrade decisions and ads add wall-clock time.

The first playable includes all sixteen module concepts, with ten available initially and six earned through free achievements. Use one farmland setting and one saucer. Keep immediate retry available; Hangar purchases are optional. Defer equipment inventories, item merging, pets, additional currencies and a pre-run blueprint loadout.

This document is the implementation checklist. [The Archero configuration](../design/balance/archero-ufo-system.json) owns the current candidate values and overrides; [the upgrade catalogue](../design/balance/endless-upgrade-system.json) owns detailed rank effects and stacking rules. Resolve future changes in those authorities and update this plan together. The selected profile is `hangar_25`.

Implementation checkpoint, 25 October 2026: the installed client is the native portrait game, with three locations, portable gameplay/account rules, protected saves, skeletal animation, socket-mounted upgrade visuals and native feedback. Headless and recorded native evidence is linked from [release readiness](RELEASE_READINESS.md). Full active-run continuation is now an active release slice; live monetisation and physical-device/human qualification remain separate.

## During a run

1. Start with one laser and one biological abduction beam, plus purchased Hangar stat bonuses.
2. Earn XP through active survival, captures and combat. The next level costs `16 + 3 × current_level` XP; level starts at 1, so the first threshold is 19 XP.
3. At each earned level, pause gameplay and offer **three distinct eligible upgrade families**. Choose one. Preserve surplus XP and process consecutive earned levels one at a time.
4. Install up to **six module types**, each with three ranks. The first pick uses a slot; subsequent ranks improve that module in its existing slot. Remove maxed and ineligible choices. There is no guaranteed card category or guaranteed owned-module offer.
5. Allow **two free rerolls per run**, resetting on the next attempt.
6. Continue until death. Level choices, pause menus, backgrounding and ad interruptions advance neither encounters nor XP.

Cards must describe **this pick's gain and the resulting total**. For example: “Add 1 abduction beam: 2 → 3 simultaneous biological targets.” Never imply that the first Beam Splitter pick immediately grants four targets. Display actual current values, including Hangar bonuses, research and caps, rather than copying baseline examples blindly.

### Module catalogue and rarity

These are cumulative ranks I → II → III, earned through three separate picks. The final column highlights the sequence; linked catalogue rules still apply to targeting, cooldowns, healing and proc limits.

| Module | Rarity | Rank progression |
| --- | --- | --- |
| Laser Array | Common | Add one laser each pick: 1 → 2 → 3 → 4 total. Each added laser deals 50% of original-laser damage. III also adds Focused Fire, up to +20% damage on a repeatedly hit target. |
| Beam Splitter | Common | Add one biological beam each pick: 1 → 2 → 3 → 4 targets. III adds a refreshing +25% capture-speed burst for other occupied beams after a biological capture. |
| Tractor Lens | Common | Add 10% of base radius per pick: +10% → +20% → +30%; range grace becomes 0.3 → 0.6 → 0.9 seconds. III adds a limited gentle pull on biological targets. |
| Shield Generator | Common | Add 20 capacity and 1/s recharge per pick. Recharge delay becomes 5.5 → 5 → 4 seconds. Fill only newly added capacity. |
| Repair Swarm | Common | Repair 2 → 3 → 4 hull per twelve biological captures. II adds 1 shield per capture; III raises it to 2, subject to the global 4/s restoration cap. |
| Vector Thrusters | Common | Movement bonus +6% → +12% → +18% of base; capture movement penalty 10% → 5% → 0%. III also adds a refreshing capture-triggered movement burst. |
| Bounty Scanner | Common | Specimen score +10% → +20% → +30%; capture streak steps/caps also improve. Hull damage resets the streak. Score bonuses never increase XP. |
| Rail Lens | Uncommon | Add one piercing target per pick: 1 → 2 → 3 additional enemies per laser hit, each receiving 40% of the original hit damage. |
| Orbit Drones | Uncommon | Add one drone per pick: 1 → 2 → 3. Each deals 2 base damage twice per second; no piercing or Arc Coil triggers. |
| EMP Ring | Uncommon | Pulse damage 10 → 16 → 22; radius 4 → 4.5 → 5m; cooldown 6 → 5 → 4 seconds. III also unlocks a limited capture-triggered non-boss disable. |
| Heavy Winch | Uncommon | Jeep capture threshold 40% → 60% → 75% hull. II/III add +20%/+40% vehicle capture speed; III unlocks tanks at ≤35% hull and a biological capture boost after vehicle destruction. Vehicles occupy two beam slots. |
| Overtime Reactor | Uncommon | Firing-rate bonus per active beam +4% → +7% → +10%, capped at +12% → +21% → +30%. A lifted vehicle counts as one active beam. |
| Reserve Battery | Uncommon | Weapon damage per idle beam +8% → +12% → +16%, capped at +16% → +24% → +32%. Occupied vehicle slots are not idle. |
| Arc Coil | Rare | Arc chance 15% → 25% → 35%, reaching 1 → 2 → 3 distinct secondary targets at 30% damage each. No recursive arcs or arcs from piercing hits. |
| Mirror Plating | Rare | Projectile reflection chance 5% → 10% → 15%, only while shield energy remains. No contact-damage or recursive reflection. |
| Phase Drive | Rare | A 3m activated dash; invulnerability 0.2 → 0.25 → 0.3 seconds; cooldown 16 → 13 → 10 seconds. No damage or chain invulnerability. |

For each card draw, the rarity bucket weights are **Common 60 / Uncommon 35 / Rare 5**, renormalised when a bucket has no eligible cards. Within a bucket, each hardware family has weight 4 and each research family weight 1. These are draw weights, not a promise that 5% of complete three-card offers contain a Rare card. An installed module's next rank draws through the Common bucket while retaining its original rarity label.

### Research keeps endless level-ups useful

Research is temporary for the current run. It does not occupy a hardware slot. “Permanent research” wording in older catalogue effects means lasting for that run; only Hangar purchases persist across attempts.

| Research | Common / Uncommon / Rare gain per unit | Limit |
| --- | --- | --- |
| Weapon Damage | +3% / +5% / +8% of fixed base damage | Uncapped |
| Hull Reinforcement | +3 / +5 / +8 maximum hull, restoring the same amount | Uncapped |
| Shield Capacity | +2 / +3 / +5 capacity, filling only the added capacity | Uncapped |
| Specimen Value | +2% / +3% / +5% biological score | Uncapped; no XP bonus |
| Capture Speed | +4 / +6 / +10 percentage points over base | Research bonus capped at +100% |
| Fire Rate | +3 / +5 / +8 percentage points over base | Research bonus capped at +50% |
| Emergency Repairs | Restore 10% / 15% / 25% of maximum hull | Immediate; offer only with damaged hull |

The first four families receive bundles: 1 unit for choices 1–15, 2 units for 16–30, 3 units for 31–45, and so on. Add gains against fixed bases; never multiply the accumulated research total. The other three families do not receive bundles. Show the actual remaining gain near a cap and stop offering capped research.

This leaves at least four eligible research families after all hardware and capped research are exhausted, so three distinct options remain possible indefinitely. Longer runs increase research bundle size while the director continues increasing pressure. This solves pool exhaustion; it does not establish that late-run decisions remain interesting.

### Earned evolutions

Both ingredients must reach III. Completion immediately grants the evolution without another pick or slot. Only the **first two** completed recipes activate per run. Simultaneous completions use catalogue order; preview which evolution a choice will activate before commitment. Legendary is an earned label, not another random rarity bucket.

| Evolution — Legendary, earned | Ingredients, both III | Result |
| --- | --- | --- |
| The Great Vacuum | Beam Splitter + Tractor Lens | Four beams become a six-target biological capture field; no extra radius. |
| Office Shredder | Laser Array + Rail Lens | Rotating sweep adds +20% total laser output, including against bosses. |
| Hostile Acquisition | Heavy Winch + Beam Splitter | Capture full-health jeeps and tanks at ≤55% hull, retaining vehicle slot/cooldown limits. |
| Mirrorball | Shield Generator + Mirror Plating | Release stored reflected damage every eight seconds, within the catalogue cap. |
| Employee Benefits | Orbit Drones + Repair Swarm | Captures temporarily boost drone damage; the squad can spend shield to intercept an ordinary projectile on cooldown. |
| Storm Drain | Tractor Lens + EMP Ring | Pulse radius becomes 6m and applies a brief slow, reduced against bosses. |

## Between runs: one optional Hangar

Research Credits buy four capped tracks. Each has five ranks costing **80, 140, 240, 400 and 640 credits** respectively: 1,500 per track, **6,000 total across twenty purchases**.

| Track | Each purchased rank | Maximum |
| --- | --- | --- |
| Weapon Calibration | +5% of each original weapon damage template; run modifiers apply afterwards | +25% |
| Hull | +6 starting and maximum hull | +30 hull |
| Shield | +4 starting and maximum shield | +20 shield; no faster recharge |
| Biological Capture | +3% of base biological capture rate | +15%; no additional beam or vehicle bonus |

Extra lasers, extra beams, reflection, regeneration improvements and invulnerability must be earned again during each run. Hangar progression improves the starting ship without granting a completed build. Display the next rank's exact benefit and cost, current balance, and an equally accessible Retry action.

| Reset for every attempt | Persist between attempts |
| --- | --- |
| Level, run XP, modules and ranks, research, evolutions, rerolls and revive availability | Credits, Hangar ranks, unlocked blueprints, achievement counters, records and cosmetic ownership |

### Free blueprint achievements

Initially available: Laser Array, Rail Lens, Orbit Drones, EMP Ring, Beam Splitter, Tractor Lens, Shield Generator, Repair Swarm, Vector Thrusters and Bounty Scanner.

The following cumulative achievements expand the pool **from the next attempt**. Unlocking a blueprint makes it eligible; it does not install the module or grant ranks. There is no manual blueprint selection screen.

| Blueprint | Account achievement | Additional requirement within the current run |
| --- | --- | --- |
| Heavy Winch | 300 biological captures | Beam Splitter II and Laser Array I |
| Overtime Reactor | 700 biological captures | Beam Splitter II |
| Reserve Battery | 800 base credits earned | Beam Splitter II |
| Arc Coil | 60 military kills | Laser Array I |
| Mirror Plating | 1,800 active seconds across runs | Shield Generator I |
| Phase Drive | 3,600 active seconds across runs | None |

Beam Splitter II means three biological beam slots; Laser Array I means one added laser. Military kills count soldiers, jeeps and tanks; vehicle captures do not count as kills. Ad bonuses do not directly advance achievement counters. Show unlock progress and prerequisites on unlock summaries and relevant previews. These requirements supersede older catalogue prerequisites for this adaptation.

## Endless pacing and rewards

Use **90-second sectors** for short goals and records. Completing a sector grants no extra currency in this candidate and does not heal, open a shop, reset difficulty or end the run.

The director escalates using active run time, independent of player level, Hangar power, spending, ad decisions or losing streaks. The candidate introduces jeeps at 30 seconds, tanks at 100 seconds and a first boss at 150 seconds, then a boss every 180 seconds. After two minutes, enemy health gains 20% of base per minute and damage gains 25% of base per minute. Spawn rates also rise, within bounded active-enemy limits. Exact pressure, spawn and boss parameters remain in the configuration and encounter assumptions; native warning times, collision and escape routes need playtests.

For a run lasting at least thirty active seconds, award:

```text
floor(active_seconds / 8)
+ floor(score / 5)
+ 2 × (jeep_kills + tank_kills + vehicle_captures)
+ 5 × boss_kills
```

Death and voluntary ending bank the same earned rewards. Each target awards its applicable XP and rewards once; capture and kill cannot both settle the same target. Score bonuses may improve credit income, but never XP. There is no entry fee, energy meter or reward loss on death.

### Monetisation candidate

Use local placement test doubles for the first playable. Live SDKs and billing follow playable validation and the audience/age-rating decision still required in the product brief.

- **One optional revive**, eligible after ninety active seconds: restore half maximum hull, zero shield and three seconds of invulnerability. Clear ordinary active enemies/projectiles; retain bosses, build, XP and active clock. Difficulty continues from that clock.
- **One optional results credit reward:** a completed placement adds `floor(base_run_credits / 2)`, or +50%. Do not multiply an earlier bonus or grant extra XP/achievement progress. Hide this offer when the player can already afford every remaining Hangar rank.
- **Future cosmetic saucer packs:** no first-playable billing requirement or revenue forecast.

Decline, unavailable inventory, failure and incomplete viewing must preserve normal results/restart. Incomplete viewing earns no placement reward. Do not place ads during active play or level choices. Exclude paid entry, unlimited paid permanent power and paid random equipment boxes. Preserve surplus credits after the Hangar cap without promising a future use; twenty ranks are a finite prototype progression goal.

## Save and interruption contract

Persist a versioned, bounded account payload through atomic replacement. Treat a missing save, invalid data, an unsupported future version and an I/O failure as distinct outcomes. A failed read must not silently replace the player's account with a fresh one.

Checkpoint every ten active seconds and at background, choice/reroll/pick and terminal boundaries. The first playable's schema-1 reward-only checkpoint remains supported. The release continuation slice now stores the account and full bounded portable Game state atomically in schema 2, restoring compatible runs paused with exact choices/location/RNG. Unsupported continuation data remains protected until an explicit verified-backup-and-bank recovery. Portable/sanitizer tests pass; native process-kill/device evidence is pending. Up to ten seconds of ordinary uncheckpointed play may be lost on abrupt termination. There are no offline earnings.

Use a bounded pending/latest-settled run receipt to prevent duplicate base rewards, credit bonuses and revive consumption. Test duplicate callbacks, reloads and interruption at settlement boundaries. Platform callbacks enqueue outcomes for the portable owner instead of directly mutating gameplay. Existing in-memory model tests do not prove durable storage.

## Implementation checklist

Checked tasks have implementation/test evidence at the dated checkpoint, not blanket release approval. Mixed implementation/human-validation tasks stay open until both are established. Preserve source art, models, fonts and assembly conventions. Follow [project setup](PROJECT_SETUP.md) and [ownership boundaries](FOUNDATIONAL_ARCHITECTURE.md); visual previews alone do not qualify shipping integration.

### 1. Native playable loop

- [x] Integrate the consumed saucer, target and farmland assets through bounded cooking and native presentation; preserve ring and attachment transforms. Three locations and truthful modular equipment now have source-stable native films.
- [x] Implement portable movement, biological capture, automatic lasers, military attacks, hull/shield, death, pause and immediate restart.
- [ ] Translate touch, keyboard and controller input into semantic actions. Select the mobile gesture policy through playtests; include an explicit usable Phase Drive action.
- [ ] Verify an uninterrupted start → capture/fight → death → retry loop, active-time pausing and background/focus recovery on the native client.

### 2. Complete run upgrades

- [x] Implement three distinct cards, XP overflow, rerolls, six slots, three ranks, rarity weights, prerequisites, research bundles/caps and two earned evolutions.
- [x] Include all sixteen modules and all seven research families; expose unlock states correctly rather than granting everything on a fresh account.
- [x] Show per-pick gains, resulting totals, recipe progress and evolution consequences. Test Beam Splitter's 1 → 2 → 3 → 4 sequence explicitly.
- [x] Verify headlessly that deep runs always have three eligible families, maxed cards disappear and restart resets all temporary power.

### 3. Director and readable encounters

- [x] Implement the active-time director and ninety-second sector/record goals, with bounded entities and clear military warnings.
- [x] Verify identical encounter schedules for identical run-time inputs regardless of account purchases or ad decisions.
- [ ] Measure capture geometry, targeting, movement, collision, warning time and escape routes in the actual scrolling world before tuning survival duration.

### 4. Results and permanent progression

- [x] Settle base credits and achievement counters once; implement four Hangar tracks with exact caps/costs.
- [x] Add free blueprint milestones, automatic eligibility on later attempts and visible run prerequisites.
- [x] Verify play → die → bank → purchase/unlock → retry entirely offline, with optional Hangar visits and correct reset/persist behaviour. Portable full-flow tests and 360 saved/reopened native attempts provide account evidence; funded native-UI purchasing still needs actual input qualification.

### 5. Durable recovery and placement doubles

- [x] Implement atomic account saves, bounded receipts, checkpoints and process-death settlement through narrow storage adapters. The first-playable schema-1 checkpoint is verified; full run continuation/migration is the active next release slice.
- [x] Test unavailable storage, corrupt/future saves, duplicate settlement and interrupted writes without silently losing an existing account.
- [x] Implement local revive/credit placements covering complete, decline, unavailable, failure, duplicate callback and interruption. These remain portable test doubles, absent from shipping service UI.
- [x] Verify the +50% bonus settles once, incomplete placements give no reward, revive limits persist correctly and service failures never block retry. This is local rule evidence, not live advertising qualification.

### 6. Human and device validation

- [ ] Run fresh-account and progressed-account sessions on representative iPhones; record active duration separately from wall-clock duration.
- [ ] Observe understanding of beam/laser increments, meaningful card choices, recipe visibility, avoidable deaths and voluntary replay.
- [ ] Compare combat, collection and mixed builds; tune specialist rewards and pool dilution if unlocked cards reduce useful choices.
- [ ] Measure controls, safe areas, HUD readability, frame time, memory, startup, lifecycle and thermal behaviour. Keep unavailable audio/haptics playable.
- [ ] Resolve audience/rating, then qualify live reward callbacks/consent and cosmetic billing separately if proceeding to release.

Gameplay and account rules stay portable and independent of SDL, GPU, filesystem and advertising SDK APIs. Presentation and renderer consume owned state; platform services sit behind narrow adapters. Package only reviewed, consumed assets through the shipping allowlist. Introduce one validated runtime tuning path rather than maintaining another set of constants beside the design configuration.

## Simulation evidence behind this candidate

The final offline study contains **19,360 run records**: 2,400 six-profile comparisons, 4,000 confirmation runs, 2,160 sensitivity runs, 9,600 repeated-account runs and 1,200 prerequisite comparisons. Twenty traced replay examples are separate. Study phases use disjoint seed ranges. The repeated-account study comprises 240 forty-attempt trajectories paired across five reward scenarios, not 240 independent human players.

Final confirmation medians, in **active minutes**:

| Simulated policy | Fresh account | Fully progressed account |
| --- | ---: | ---: |
| Hybrid / mixed | 6.16 | 9.22 |
| Collector | 4.97 | 7.11 |
| Gunship / combat | 6.62 | 9.09 |
| Defender | 6.03 | 8.70 |
| Random choices | 4.70 | 5.79 |

Fresh Hybrid's 10th–90th percentile range was 4.43–9.17 minutes; fully progressed was 6.10–12.04. The first choice arrived at a median fourteen seconds. Fresh Hybrid made a median seventeen choices and 47% earned an evolution; fully progressed made twenty-five choices and 78% evolved.

For twelve regular Hybrid account trajectories per reward scenario, the first purchase occurred at median attempt one and all free blueprints unlocked at median attempt eight without ads. Median attempt to cap the Hangar was:

| Reward scenario | Median cap attempt |
| --- | ---: |
| No ads | 22.5 |
| Complete every eligible +50% credit reward | 15 |
| Complete every eligible revive and +50% credit reward | 12 |
| Revive plus double-credit comparison | 9 |

Double credits is a comparison, not the chosen reward. All-ad scenarios assume every eligible placement completes and a thirty-second wall-clock cost per placement. They do not predict adoption, retention or revenue. No-ad Collector, Gunship and novice Hybrid cap medians were 32.5, 27.5 and 30 attempts respectively; progression speed depends on play style and modelled skill.

The separate prerequisite comparison improved fully progressed Collector median survival from 6.22 to 6.82 minutes on one hundred paired seeds. Final confirmation gives 7.11. This supports contextual eligibility, but does not prove every unlock improves every draft.

Five proposed model gates passed: fresh Hybrid median 5–10 minutes; fully progressed Hybrid median ≤13 minutes; at least one minute median paired improvement from Hangar stats alone; first no-ad purchase by median attempt two; and at least 90% of fresh Hybrid runs surviving one minute. Thirty-six focused model tests and repository static checks passed in the simulation pass. These are model checks, not acceptance of the native product.

### What still needs evidence

The Python encounter model does not simulate actual touch controls, screen readability, spatial collision/lanes, audio or human decisions. Movement, targeting, capture geometry, drones, arcs and pulses use proxies. Thrusters may be overvalued if real forward motion and lateral movement behave differently. Boss survival does not necessarily mean a boss kill. Vehicle capture score is an assumption.

The study cannot establish fun, retention, learning, conversion, ad fill, revenue or long-term content depth. Collector rewards, specialist usefulness, recipe guidance, unlock dilution, revive value and time spent in choice menus remain explicit playtest questions. Keep the accepted two-layer model while testing these issues; use observed gameplay to revise its numerical candidate.

## Evidence and reproduction

- [Human-readable simulation report](../design/balance/archero-ufo-approach.html)
- [Final results](../design/balance/archero-ufo-results.json), [profile comparisons](../design/balance/archero-comparison-results.json), [prerequisite follow-up](../design/balance/archero-unlock-followup.json), [model gates](../design/balance/archero-model-review.json) and [traced examples](../design/balance/archero-ufo-examples.json)
- [Frozen upgrade input](../design/balance/endless-simulation-input.json) and [encounter assumptions](../design/balance/endless-encounter-assumptions.json); corresponding compressed JSONL files retain run records.

Commands to reproduce the study, rather than a requirement for documentation-only edits:

```sh
python3 tools/simulate_archero_ufo.py --compare --runs 40 --workers 6
python3 tools/check_archero_unlocks.py --runs 100
python3 tools/simulate_archero_ufo.py --runs 200 --players 12 --attempts 40 --stress-runs 60 --workers 6
python3 tools/report_archero_ufo.py
python3 tools/test_archero_ufo.py
python3 tools/test_short_progression.py
python3 tools/test_endless_balance.py
```

This plan supersedes the manual ten-blueprint loadout in the earlier short-run proposal. Earlier finite-run, twenty-to-thirty-minute and unrestricted-unlock studies remain historical evidence; do not treat their settings as simultaneous runtime rules.
