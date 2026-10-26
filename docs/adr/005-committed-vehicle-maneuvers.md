# Committed ordinary vehicle maneuvers

Date: 25 October 2026; host qualification26October. Status: implementation and
bounded host source/runtime/presentation accepted; mobile refresh and physical
qualification remain open.

## Context

Actual Game8 jeeps can change chassis heading by 69–114 degrees in one tick.
The chassis also supplies real weapon origins, so a cosmetic yaw filter would
make the rendered body disagree with combat. Jeep and tank motion needs bounded
turning, real forward/reverse translation and coherent braking.

Two initial Game9 candidates retain collision clearance but fail finite actual
director cases. A forward-ray guide does not admit the opposite curved backing
path. A later sampled reverse arc admits an exit but discards the selected side,
remaining angle and exit on the next tick; execution can abandon the admitted
maneuver and stop indefinitely. Signed velocity and facing identify the current
gear but cannot retain this commitment through a stop.

## Decision

Keep this inside portable gameplay and restrict it to ordinary jeeps and tanks.
Brake straight to rest before admitting a local reverse-turn, reverse-brake and
forward-exit sequence. Planning and execution use the same bounded fixed-step
transition, acceleration, braking, angular limit and swept body clearance.
Evaluate actual backing paths and usable exits before applying side preference.
Persist phase, signed remaining turn and remaining exit distance; consume only
actual accepted motion. Planting, lift, pause and cold recovery preserve the
commitment. New obstacles can interrupt it safely; replanning starts from rest.
A fully closed cage holds rather than inventing movement.

The wire suffix is one phase byte and two floats for each ordinary
vehicle (nine bytes); biological actors and bosses retain their existing record
body. Game codec becomes 2 and authority becomes 9; account schema remains 2.
Validate phases, finite bounded residuals and their combinations. No renderer
state, generic navigation framework or dependency is introduced. The renderer
continues to consume physical facing; wheel travel uses signed body-frame
displacement.

Admission and execution must also agree at the handoff: an endpoint that the
ordinary controller immediately classifies as blocked cannot count as a usable
exit. Normal steering reserves its actual discrete brake-to-rest corridor after
turning. Independent finite tests exposed both mismatches before acceptance.

The combined gate also exposes a crowd interaction with the existing boss:
both direct station capsules can be blocked by a slower tank while a horizontal
corridor remains clear. The bounded correction uses that corridor and the
existing saved flank choice with world acceleration, returning to ordinary
station/attack rules when the route clears. It adds no saved mode and changes no
retirement, scroll, speed or spawn budget. Actual next-step military sweep and
field admission remain required. The unchanged crowded boss gate and a labelled
detour continuation/swept-clearance observer pass on the final host source.

The local arc/straight/gear vocabulary is informed by
[LaValle's Reeds–Shepp discussion](https://msl.cs.uiuc.edu/planning/node822.html)
and [Dellaert and Hutchinson's motion primitives](https://www.roboticsbook.org/S65_driving_planning.html).
This product does not implement their general shortest-path planners.

## Migration and alternatives

Codec1 Game7/8 continuations are not reinterpreted. A direct inspection may
report unsupported schema. The existing outer account boundary must still load
the wallet and ranks as protected incompatible continuation, verify a backup
before replacement and bank recorded credits exactly once. Future-schema and
corrupt-data protections remain. Earlier source and owner test saves stay intact.

Instantaneous discrete velocity selection, renderer smoothing and repeated
stateless scoring were rejected by source/visual disagreement or actual failed
escape cases. The small saved commitment is justified because its progress
changes the next authoritative decision.

## Required verification

Retain the original sixteen fixed-body pairs and four-second bounds, ingress
counts/RNG, weapon/warning alignment and whole-body swept clearance. Require
finite real director escapes from edge-plus-body, one-sided retreat and reopened
cages; a closed cage must hold honestly. Verify bounded yaw, no sideways sliding,
signed wheel travel and real brake-to-stop gear transitions. Check cold first
draw and exact futures during each maneuver phase and interruption, malformed
wire states, full save budgets and actual codec1 protected banking.

Actor layout changes require all gameplay translation units to be rebuilt
coherently. Measure allocations and peak planning-frame CPU cost, including
simultaneous dense blocked vehicles, before accepting the source. Native temporal
review must also reject visible hesitation or oscillation even when numerical
angle bounds pass. These gates qualify the final host slice recorded in the
26October development log:47aggregate suites and independent bounded native
review pass. The natural opening film contains three jeeps and one early tank,
with real reversing on jeeps and no hit; a separate controlled presentation still
verifies damage HUD pixels.
It does not establish all late-run/native tank motion, physical phone performance
or human play quality. Earlier rejected candidates and their actual failures stay
preserved in ignored evidence.
