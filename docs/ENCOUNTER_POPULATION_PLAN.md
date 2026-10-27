# Abduction weights and encounter progression

Implemented native candidate, 26 October 2026. Start with a farmer and small
livestock, then overlap heavier livestock, more people and increasingly
organised military response. Easy targets remain available throughout the
endless run. Native tuning owns the values below; human pacing and physical
phone qualification remain separate. Existing owner playtest apps are unchanged.

The later owner-requested action-pacing pass supersedes the numerical durations,
arrival times and formation allowances in this original population study. See
`adr/011-action-pacing-and-military-escalation.md` and `runtime-spatial.json` for
the live candidate. Species, group placement and physical movement contracts
below remain applicable.

## Baseline before this change

The native roster has four civilian/animal gameplay types, three infantry roles,
two ordinary vehicles and one boss. The Suburbia resident is the farmer type's
location-specific appearance, with the same rules. The old soldier model is not
an additional equipped infantry class. No additional animal or civilian class
is needed for this first progression.

| Previous target | Base capture score | Ordinary lift time | Current acquisition difficulty |
| --- | ---: | --- | --- |
| Sheep | 2 | 2.0 s | Unarmed; locally flees |
| Pig | 3 | 2.0 s | Unarmed; locally flees |
| Farmer or resident | 3 | 2.0 s | Unarmed; locally flees |
| Cow | 5 | 2.0 s | Unarmed; larger footprint and slower travel |
| Rifleman | 3 | 2.4 s | Holds rifle standoff and commits a warned shot |
| Shotgunner | 3 | 2.4 s | Closes towards 3.5 m and fires a three-round fan |
| Rocketeer | 3 | 2.4 s | Longer standoff and a slower warned rocket |
| Jeep | 5 | 3.0 s at Winch I | Heavy Winch, damage threshold and two beam slots |
| Tank | 5 | About 2.14 s at Winch III | Winch III, damage threshold and two beam slots |
| Boss | Cannot capture | — | Combat encounter; destruction currently scores 10 |

Times exclude permanent/research gains, sweep acceleration and Overdrive.
The tank's previous time was the same 3 s vehicle base divided by Winch III's
1.4 speed factor, with a 2 s floor. Its harder acquisition gate does not make
its lift slower than the jeep. Infantry roles shared capture rules.
There was no per-type lift-weight rule.

The previous director started with 24 specimens in six four-body pockets, permitted
every livestock species immediately, introduced two soldiers at 1.2 active
seconds, a jeep at 12, a tank at 40 and a boss at 90. Infantry roles were random
from their first spawn. Previous formations chose left, right or split ingress;
actors subsequently pursued their own tactical destinations. Civilian movement
shared one local-fear behaviour with different speeds. These baseline rules
provided grouping and spacing without teaching the roster in stages.

Current authorities: [entity and role contracts](../native/human_resources_gameplay/include/human_resources/gameplay/gameplay.h),
[native spatial tuning](../native/human_resources_gameplay/runtime-spatial.json),
[capture rules](../native/human_resources_gameplay/src/capture.cpp),
[population director](../native/human_resources_gameplay/src/population.cpp),
[formation admission](../native/human_resources_gameplay/src/formations.cpp),
[civilian movement](../native/human_resources_gameplay/src/civilian_movement.cpp) and
[ground separation](../native/human_resources_gameplay/src/body_separation.cpp).
The [tuning generator](../native/human_resources_gameplay/tools/generate_tuning.py)
also consumes the live upgrade catalogue and selected progression profile;
native introduction overrides differ from the historical encounter study.

## Capture points and difficulty ranking

Use a dimensionless **lift load**, rather than kilograms. It represents mass,
equipment and resistance to lifting. Keep approach danger separate: a cow weighs
more than a rifleman, while the rifleman threatens the ship. Scores below are
base points before existing eligible score bonuses.

| Encounter difficulty | Target | Lift load | Base lift time | Capture points | Main challenge |
| ---: | --- | ---: | ---: | ---: | --- |
| 1 | Sheep | 1.00 | 1.6 s | 2 | Easy flock pickups and sweep chains |
| 2 | Pig | 1.15 | 1.84 s | 3 | Breaks from the group in a committed escape direction |
| 3 | Farmer or resident | 1.35 | 2.16 s | 4 | Reacts earlier and chooses an open escape route |
| 4 | Cow | 1.75 | 2.8 s | 6 | Occupies a beam longer; broad, slower turns |
| 5 | Rifleman | 1.55 | 2.48 s | 7 | Standoff and partner covering fire |
| 6 | Shotgunner | 1.70 | 2.72 s | 9 | Close fan while the ship's beams are occupied |
| 7 | Rocketeer | 1.90 | 3.04 s | 12 | Rear support protected by a screen of other infantry |
| 8 | Jeep | 2.25 | 3.6 s before Winch speed gains | 18 | Damage, equipment and two-slot commitment |
| 9 | Tank | 3.00 | 4.8 s before Winch speed gains | 35 | Winch III, low hull and a long two-slot lift |
| 10 | Boss | Immune | — | No capture reward | Keep the existing 10-point destruction reward in this slice |

This ranks complete encounters, not every isolated duel. A lone shotgunner can
be baited into the existing pickup radius more easily than a standoff rifleman.
That is a useful weakness; introduce it with a rifle partner rather than trying
to hide the weakness with artificial capture immunity. The rocketeer's reward
reflects reaching the rear position, not a claim that its slower rocket is
always harder to dodge.

For level-one unmodified lifting, use `1.6 seconds × lift load`. Each earned
level after the first adds eight percentage points of base abduction rate. Biological speed
bonuses, the existing minimum duration, real close sweeps and Overdrive continue
through the existing capture calculation. Vehicles add level and Winch speed gains, preserving the existing tank floor.
At level one with Winch III, jeep/tank times become approximately 2.57/3.43 s. All biological targets still use one beam slot;
vehicles use two. Weight does not add an equipment gate for livestock or people,
or an additional movement penalty on top of the existing beam penalty.

Preserve [beam commitment](TRACTOR_BEAM_RULES.md): once acquired, a target cannot
flee, break formation, exceed its lifetime or scroll away to cancel the lift.
Difficulty comes before acquisition and from occupied capacity afterwards.
No random escape roll, repeated beam tapping or post-acquisition tug-of-war.

Keep biological capture XP at 1 and jeep/tank XP at 2/4 initially. Keep existing
destruction rewards and the distinction between capture and kill procs. Vehicle
points remain outside biological specimen/streak bonuses, as they are today.
A lifted actor cannot also pay a destruction reward. The boss and decorative
scenery remain outside ordinary abduction; trees, buildings and fences are not
new capture types.

Higher points affect Research Credits, currently partly derived from score at
5 points per credit. Longer lifts also change capture frequency, XP, repairs,
sweep chains and capture-driven scroll growth. Compare whole-run earnings and
upgrade timing before promoting these values; do not multiply score by ten for
display or silently retune permanent costs to absorb an unmeasured increase.

## Reveal through the opening three minutes

These are **earliest arrival gates** in active gameplay. Introductions wait for
a legal complete group and the available formation allowance; full populations
cannot lose an introduction or force overlapping bodies. The first cattle also
wait for a full civilian pocket. Visibility follows actual world ingress and the
current camera, rather than an assumed equivalence between spawn and display.
Pauses, upgrades and background time freeze the schedule. Existing species remain
eligible after each reveal. The reviewed native phone sequence reaches all
military introductions and the boss during the opening three active minutes.

| Active time | Civilian and animal composition | Military reveal and pressure |
| --- | --- | --- |
| 0–20 s | First reachable pocket: one farmer, two sheep and one pig. Further small-livestock pockets fill both sides with depth between them. | No military; learn acquisition, steering and moving sweeps. |
| 20–40 s | Introduce one cow with two sheep and a farmer; later pockets add cow pairs and more farmers. | Establish the contrast between a slow valuable lift and quick small targets. |
| 40–60 s | Keep the mixed farmland supply, including easy sheep pockets. | First rifleman eligible at 40 s, initially alone; later a spaced patrol pair. |
| 60–80 s | More two-person groups alongside livestock; retain cow pairs. | First shotgunner eligible at 60 s with one rifleman. Teach closing pressure and covering fire. |
| 80–110 s | Alternate open grazing pockets and people near decorative farm frontage. | First jeep eligible at 80 s, initially without escorts; introduce the escorted version in a later encounter. |
| 110–140 s | Maintain available easy pickups between military groups. | First rocketeer eligible at 110 s, with two riflemen as a staggered screen. No simultaneous new heavy reveal. |
| 140–180 s | Cows and people provide valuable optional routes; small animals remain common. | First tank eligible at 140 s; later tank escort. Stop new ordinary military ingress for the final 4 s before the boss. |
| 180 s onward | Continue mixed rural pockets during combat and recovery. | First boss at 180 s; later cycles remix learned formations without ending or resetting the run. |

Only the first reachable pocket is fixed. Seeded later pockets vary species,
side, depth and spacing within the unlocked pool. Keep the opening populated,
with at least two immediately reachable specimens and useful routes on both
sides; do not replace the current 24-specimen opening with four isolated actors
and an empty horizon. Start by replacing the opening's species choices rather
than reducing its total population budget.

Before the easy-pocket guarantee and placement admission, later nonmilitary
specimen weights are 20% sheep, 15% pigs, 35% farmers/residents and 30% cows.
These are intended proportions
across social pockets, not independent per-body random rolls: deliver a flock,
pair or farmer-with-livestock group that reads as a place. Keep at least one
small-animal pocket in every three successfully delivered civilian pockets
after the opening. A missed delivery at population capacity cannot satisfy that
guarantee. Recovery offers access to those easy pickups, including late in a run.

Do not scale military strength to player level, purchases or capture success.
Use active time and authored encounter windows. Strong builds earn breathing
room. Increase later pressure through learned combinations and bounded arrival
rates, retaining current warning lengths and physical movement constraints.

## Different civilian and animal behaviour

| Target | Normal movement | Reaction before acquisition | Group spread |
| --- | --- | --- | --- |
| Sheep | Loose flock, mild shared forward drift | The first surviving local flock member leads a gradual turn away; neighbours follow with a short delay | Stay loosely together, with body clearance and enough separation to select individuals |
| Pig | Loose pairs or small farm groups | Pick one open diagonal away from the UFO and commit briefly before reassessing | Split into distinct escape routes; avoid frame-by-frame zigzags |
| Cow | Broadly spaced pairs, slow forward travel | Brief reaction delay, then a steady wide turn | Use the measured 1.158 m ground envelope and wider cattle delivery spacing |
| Farmer or resident | Walk alone, in pairs or beside livestock | React earlier than animals; choose a clear side and continue running below scroll speed | Social groups separate under local pressure rather than every person fleeing to the same edge |

Fear remains local to each group. Distant animals do not all turn when one is
approached. Farmers can turn towards nearby livestock as a readable reaction,
but they do not introduce a separate alarm meter or spawn soldiers when abducted.
All escape speeds remain below the minimum runner scroll speed, preserving the
existing chance to catch up. Use gradual turning and committed routes so actors
look intentional; physical separation can still interrupt an obstructed route.

Frontage, grazing areas and roadside placement create visual habitats with the
existing assets. Scenery does not become simulated cover, a navigation mesh or
an animal enclosure merely because a group appears near a fence or barn. Supply
safe placement candidates within the real gameplay field and existing collision
contracts. Suburbia keeps its resident appearance and shared numeric balance;
use equivalent open pockets rather than requiring a farm prop in every theme.

## Military formations and conduct

Formation shapes retain persistent relative positions and role assignments during
approach, then transition into existing individual attack behaviour.
Choose the formation's side and anchor at ingress. Assign separate destinations
with actual body clearance; do not send every member to the same player-relative
flank or continually mirror the entire formation when the player crosses centre.

| Formation | Composition and shape | Conduct and player opportunity |
| --- | --- | --- |
| Patrol pair | Two riflemen in a loose diagonal on one side | Front rifleman prepares first; partner follows after the shared attack slot is available. Approach the exposed end or cross the open side. |
| Closing echelon | One shotgunner ahead, one or two riflemen behind and offset | Shotgunner closes while rifles hold standoff. Bait the closer into a free beam, then sweep around the screen. |
| Rocket screen | Rocketeer behind two spaced riflemen, with a gap between the front bodies | Rear actor holds rocket range; front actors create approach risk. Flank the screen to acquire the valuable support unit. No invulnerable shield wall. |
| Jeep escort | Jeep with two riflemen offset behind its flanks | Keep a real vehicle-width corridor. Escorts pursue individually once the jeep plants; jeep retains bounded turning, braking and recovery. Pick off escorts or damage and winch the jeep. |
| Tank advance | One tank followed by two offset infantry | Tank commits its warned shot; infantry attack on staggered opportunities and leave an escape side. Capture escorts or use the tank's recovery to approach it. |
| Late pincer | Two small groups at staggered depths on opposite sides | One side engages before the other. The second cannot seal the same safe lane during the first committed attack. Choose a flank before converging pressure grows. |

Use two to four members per formation. Before 110 s, allow only one formation
to engage at a time; existing survivors must be accounted for before admitting
the next. Later, permit a second small group only when spacing and the shared
threat budget allow it. Pair opposite-side groups at different depths so their
silhouettes and escape routes remain readable in portrait.

Prepare, warn, fire and recover through the existing actual-heading and muzzle
rules. Preparing infantry reserve the same threat slots as committed warnings.
Start introductions with one shared slot; use at most two during the first
military learning phase, then the existing build/assault/recovery caps of 2/4/1.
These limits include surviving threats from older groups, rather than granting
every new formation a fresh firing allowance. Once warned, shots keep their
committed aim. All existing individual warning durations remain intact.

When a member is abducted or destroyed, retain the other members' assignments
until their current attacks finish; do not instantly shuffle into its space or
transfer its warning. Any later regroup uses ordinary physical travel. Acquired
infantry stop participating immediately. Acquired vehicles retain the current
no-firing rule. Existing projectiles remain real hazards after their source dies.

Preserve the existing 28 s cadence after the introductory phase: 10 s build,
12 s assault and 6 s recovery. Recovery reduces new arrivals and attacks without
freezing survivors. Boss ingress suppresses new ordinary arrivals, preserves
full warnings already underway and uses the existing shared boss budget.
Retain the current rake/fan behaviour and 180 s repeat interval for the first
candidate; a boss reveal does not simultaneously introduce another mechanic.

## Spawn admission and persistence

Civilian supply and military pressure keep separate bounded budgets. Spend a
formation's military budget as one authored group; do not add escorts free on
top of the existing rate. If the whole formation cannot fit its population and
placement allowances, defer it or select a smaller legal template. Do not expose
a rocket support actor without its intended screen as an accidental partial
spawn. Quiet/recovery windows must not accumulate a reinforcement burst.

Check the whole-body footprints and player clearance over approach paths, not
only the actors' spawn roots. Preserve a connected escape corridor with at
least the existing 1.2 m route half-width after body envelopes are excluded.
Do not infer safe routes from a clear centre point alone. Retain bounded local
placement/separation and vehicle maneuvers; no physics or ECS dependency.

The gameplay owner keeps validated per-type lift loads, capture scores and
reveal gates in native runtime tuning. Formation geometry and role composition
belong to the focused formation owner, using the shared tuning for lane, depth
and stagger timing. The abstract
simulator and its frozen studies remain separate. Capture/progression consume
one score source; UI reads the same values if target rewards are displayed.

Game codec 6 / authority 14 persists group identity, assigned approach slot,
committed escape choice, pending reveal flags and the successful easy-pocket
counter. Account schema 2 and protected old-run banking are retained. Cold resume
cannot reroll a formation, replay an introduction or release a locked beam.
[ADR008](adr/008-population-and-abduction-progression.md) records the ownership and
compatibility decisions.

## Acceptance coverage

The optimized development gate passes all 52 host suites, including the new
population contracts and existing capture, harvest, military, vehicle, boss,
continuation and account recovery checks. The population suite also passes
AddressSanitizer and UndefinedBehaviorSanitizer. Tests cover every weighted
capture and actual reward, complete-group admission, delayed reveals, the easy
pocket guarantee, measured birth clearance, approach handoff, interrupted groups,
local species reactions and exact paused/cold futures. Malformed population
prefix, group and civilian escape records reject transactionally.

A fresh native SDL/Metal recording at 390×844 contains 4,600 real rendered frames
and 195.267 active seconds through the boss. Video encodes at 20fps, with 0.05s
simulation requests and the actual active delta supplied to animation; modal
holds freeze both. Sampled frames show the overlapping population, separated
cattle and the staged infantry/vehicle/boss arrivals. This is visual evidence,
not a claim of physical-phone frame rate or a human playtest.

Eight two-minute portable runs compare the old torso-core spacing with the new
neutral-body envelopes, using the same two scripted pilot policies and four
seeds. Cow neutral-box overlap samples drop from 8,172 to 11; all passive pairs
fall from 66,288 to 77. These are bounding-box proxies sampled every 0.1s with a
fixed diagnostic camera, not animated triangle collisions. A remaining body-core
proxy episode lasts 0.1s. No zero-overlap or physical-device claim follows.

Whole-run comparison uses actual portable gameplay, three seeds, idle and
oscillating input, first-card choices and fresh/developed Hangar profiles. It
changes no player health and banks no user account. Results are small scripted
samples, not evidence of human skill or fun:

| Profile | Prior median duration | Candidate median duration | Prior projected credits per active minute | Candidate projected credits per active minute |
| --- | ---: | ---: | ---: | ---: |
| Fresh | 46.04 s | 286.03 s | 40.52 | 40.51 |
| Developed | 74.34 s | 499.37 s | 47.29 | 52.04 |

Each row contains six runs per version; one developed candidate survives the
600s horizon and is censored. Median first-choice times change from 11.91→14.18s
for fresh runs and 10.77→13.52s for developed runs. Longer total earnings mainly
reflect longer play; permanent prices and the canonical credit formula remain
unchanged. No numerical retune is hidden in that comparison.

Detailed source bindings, prior failures, revised native recording, sampled
frames, overlap traces, sanitizer output and before/final pacing CSVs are under
`build/evidence/encounter-population/`. Host verification does not refresh an
existing owner app, establish physical-phone budgets or qualify a release.
