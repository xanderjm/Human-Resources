# Persistent enemy engagement in the scrolling arena

Date: 26 October 2026. Status: owner-requested native candidate; host and native
verification are recorded in the newest development-log entry. Human pacing and
physical-device qualification remain separate.

## Context and decision

The owner reports enemies leaving the bottom and appearing to shoot in random
directions. Infantry previously faced its travel guide, could not backpedal,
and ran at 3.2m/s while the world could advance at 6.8m/s. Ordinary military also
expired on an age timer. Multi-second world-velocity aim prediction could point
well beyond the currently visible player.

Gameplay now owns a scrolling combat frame for military bodies, their committed
warning points, boss plans and enemy projectiles. Each active fixed step applies
the same forward translation before collision-admitted local movement. This
preserves military spacing and keeps planted/disabled actors on the level.
Civilians and scenery retain their world movement and retirement. Local military
and projectile velocities exclude this common translation; player velocity stays
in world coordinates. Existing actor/projectile fields are sufficient; no new
durable clock, navigation framework, dependency or asset is introduced.

Military enters physically from the existing offscreen formation positions.
Engagement destinations stay inside the warning/flight band; swept controllers
also respect front/back whole-body flight boundaries. Existing out-of-bounds
ingress or capture releases admit inward motion without teleporting. Military
has no scroll-distance or age despawn: destruction and eligible capture remove
it, with existing death/revive/run-reset policies retained. Existing population
caps bound persistent pressure, and blocked formations wait for capacity.

Infantry steers its travel independently of its bounded physical facing. Its
body follows the source-derived player aim, including the authored cross-body
weapon yaw, while it advances, retreats or sidesteps around reserved bodies.
Preparation, full warnings and fire recovery retain their local planted sources
and shared attack reservations. Ordinary warnings target the actual player at
commitment. The point then stays fixed within the scrolling arena, so a dodge
does not redirect the shot. Vehicles retain physical chassis steering while
their existing turrets track the player/committed warning. Bosses retain their
fan/rake patterns and release schedule, using player velocity relative to the
arena for their existing lead. Remove the boss's former extra scroll carry to
avoid counting the common translation twice.

World-float translation can otherwise make an old physical muzzle disagree with
its recalculated ray after long travel. Reconstruct infantry warning facing and
boss physical plan geometry after translation. Preserve the original boss sweep
order, release offsets and initial turret yaw. This accounts only for floating
point reconstruction; it never samples the player's later position.

Presentation uses actual ground travel for infantry gait, including holding
station against scroll. Existing looping walk/run clips support reverse
playback. During moving aim/fire, only the six leg joints take the gait; root,
pelvis, upper body and weapon retain their authoritative pose. Capture keeps
priority. Zero active delta freezes gait, aiming and gameplay together. Vehicles
continue deriving wheel motion from actual world displacement.

## Alternatives and consequences

Increasing run speed alone still loses planted warnings and disabled enemies.
Clamping final positions would bypass swept clearance and teleport bodies.
Keeping timeouts would contradict persistent engagement. The common combat
frame keeps these policies explicit and reuses existing local physical steering.

This changes combat pressure, interception and run balance. Existing headless
budgets and native captures qualify their recorded scenarios, not human fun or
physical-phone performance. The offline abstract balance simulator is unchanged.

## Persistence and verification

Game codec6/account2 remain. Authority19 includes this engagement change and the
concurrent authority18 equal-flight correction. Numeric tuning is unchanged.
Old continuations use the existing protected backup and exact-once receipt bank
flow; do not reinterpret them or reset permanent account progress.

Regression coverage includes slow/maximum-scroll entry and two-minute retention
for all ordinary classes, both player flight extremes, infantry retreat,
complete warnings and disable, bounded/swept body clearance, all-role reverse
gait and upper socket preservation, boss source/sweep order/release timing,
stationary-hit and real movement/Dash controls, ten repeated boss flight cases,
pause and exact cold futures, and authority18 protected account banking.
