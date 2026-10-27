# Earned abduction speed and quicker promotions

Date: 26 October 2026. Status: implemented native playtest candidate; human feel
and physical-device qualification remain open.

The owner reports slow abductions and diluted upgrade progress, and suggests
fewer upgrades, automatic level speed gains, or both. Keep the existing hardware
variety for this pass and ensure every earned promotion improves harvesting.

The live catalogue owns a 1.6-second base per lift-load unit, down from two.
The selected progression profile owns `12 + 2*current level` XP per promotion,
down from `16 + 3*current level`, and an additive 0.08 base-rate gain per level
after one. Survival/capture/combat XP retain their owners. Capture Speed flight
research changes from 4/6/10 to 12/18/25 percentage points by rarity, capped at
+100% research. This makes a dedicated capture pick stronger than the automatic
level gain. Six slots, sixteen module families and three hardware ranks remain.
Reducing hardware ranks would also change evolution recipes and save contracts;
that larger redesign awaits playtest evidence from this candidate.

The capture owner exposes one portable level-bonus query for rules and UI.
Biological lifts add level, Hangar, flight research and temporary bonuses before
the existing minimum and Overdrive. Vehicles add level and Winch bonuses while
retaining their equipment/hull gates, two-slot occupancy and tank floor. Species
weights, rewards, movement cost, committed beams and real sweep participation
remain. Level speed reaches the existing floors in strong builds; it does not
promise an unbounded reduction below them. Promotion, Current Build and spoken
values disclose the reward without inserting it ahead of enlarged card benefits.

The bonus derives from saved level rather than another mutable accumulator.
Rerolls, pause, card selection and cold restore cannot farm or double it. New
flights reset it. Codec6/account2 stay; gameplay authority advances14→15 and the
numeric fingerprint changes. Old Game14 continuations retain the existing
protected original-backup/once-bank path, preserving permanent account progress.
Rollback requires the matching old binary and protected original continuation;
never rewrite incompatible saved bytes to simulate compatibility.

Focused gameplay tests cover additive rates, every target class, minima,
temporary boosts, near-cap research, natural first promotion, cold choices,
rerolls and restart. The existing save/account suites cover incompatible rules.
Source-bound three-seed public-Game comparisons use unchanged pilots/card-zero
choices, no health overrides and a600second observation. They show substantially
higher capture/credit throughput and more developed builds. This may shorten
long-term Hangar progression even though costs and credit conversion are unchanged.
Do not treat these scripted runs as fun, economy acceptance or device budgets.
The abstract balance simulator and frozen studies remain separate historical
models; no simulator implementation is imported into runtime.

The shared public header and existing large UI/test owners gain only their
matching capture query, display text and regression wiring. No new unrelated
state machine, dependency, renderer state, asset or platform API is introduced.
Exact executed gates and evidence limitations belong in the newest development
log entry and `build/evidence/progression-pacing/`.
