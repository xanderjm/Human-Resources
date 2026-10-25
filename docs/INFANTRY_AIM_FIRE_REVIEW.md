# Equipped infantry aim, fire and capture review

The current three-body authoring set is revision 2, published and frozen in
`build/animation/infantry/exports`. `producer-manifest.json` beside those exports
binds the exact inputs, reports and body GLBs. The isolated qualification copy is
`build/animation/infantry-recoil-v2`. Canonical/qualification GLB bytes match.
The six legacy producer tools, recipes and exports remain unchanged.

The normal-speed review is
`build/animation/infantry/recoil-v2-review/infantry-equipped-review.mp4`:
10.1 seconds, 303 decoded frames, 1280×720 at 30 fps. All three bodies perform
`aim_enter → aim → fire → aim_exit`, then beam lift, two struggle cycles and
release. Fire is three actual 0.6-second source cycles. No muzzle flash,
projectile extraction or native effect is implied by this recoil-only film.
Sources, timeline and hashes are in `review-film-validation.json` there.

## Current motion and readable recoil

The bounded revision adds one chest-subtree impulse before the two-hand solve:
3°/12 mm rearward for rifle, 5°/24 mm for shotgun, 3°/15 mm for launcher.
Shoulders, head, hands and weapon react together; root, pelvis and legs do not
receive recoil. The fast impulse has a damped return and exact-zero endpoints.
No finger-detail revision or source mesh/UV/material change was made.

At the original 780-pixel / 20.5-metre portrait projection, actual serialized
60 Hz samples measure the following peak displacements against aim at matching
breathing phase:

| Body | Previous weapon | Revised weapon | Revised head |
| --- | ---: | ---: | ---: |
| Rifle | 1.42 px | 3.00 px | 1.80 px |
| Shotgun | 1.95 px | 4.59 px | 3.13 px |
| Launcher | 1.21 px | 3.14 px | 1.93 px |

Foot transforms are exactly unchanged in that comparison. These are projection
measurements, not a native rendered readability acceptance. A narrower runtime
camera increases those pixel displacements. `authored-phone-recoil.json` binds
the measured GLBs. Actual elevated aim and fire-peak stills preserve full feet,
stock/support contacts, head/tube clearance and the shoulder launcher silhouette.
They are explicitly labelled matrix diagnostics, not baked altitude clips.

## Socket and coordinate contract

All values are metres, +Y up, with nominal raw weapon forward +Z. Matrices in
`socket-aim-measurements.json` are row-major lists operating on column vectors;
transpose their storage when importing into a column-major native matrix array.

Let `G` be a skeleton-global transform in actor-local GLB space. The raw weapon
transform is `D = G_pose(socket_weapon) * inverse(G_rest(socket_weapon))`.
Transform a raw datum as `D * [position,1]`; transform the fire axis as
`normalize(D * [0,0,1,0])`. Apply the actor world transform exactly once outside
that delta. Do not apply the fit transform again, and do not use a bone's long
axis as the fire direction. Marker heads already carry the raw weapon datums.
If the mesh cooker applies a coordinate normalization `N`, use `N * D * N^-1`
for already-normalized weapon vertices, or transform the raw point with `N*D`.

| Body | Neutral weapon yaw | Aim muzzle Y | Marker(s) |
| --- | ---: | ---: | --- |
| Rifle | +45° | 1.0804 m | `socket_muzzle` |
| Shotgun | +55° | 1.0273 m, left barrel | `socket_muzzle_left`, `socket_muzzle_right` |
| Launcher | +40° | 1.3976 m | `socket_nose`, `socket_tube` |

These neutral heights use `aim` at 1.0 seconds before actor world placement.
Breathing adds less than 0.35° elevation variation. Head-rest-forward proxies
track the level barrels within about 2°; there is no facial/eye tracking.
The source clips aim horizontally and require an elevated chest correction for
an elevated UFO. Use explicit source datums for both shotgun barrels; launcher
spawn clearance is `socket_nose + raw(+Z)*0.01m`. Its coral loaded nose remains
integral source geometry; no unload/extraction animation is authored.

## Planted aiming and preserving fire recoil

The root's chosen runtime behavior is deliberate authoritative stopping during
committed warning and fire recovery, with recovery inside the existing cooldown.
This is owned by military gameplay integration, not implemented by this author.
Set the actor's upright heading to target-bearing minus its neutral weapon yaw
while establishing the planted firing stance. Preserve that committed heading
through the attack. Walking carry uses actual stride travel; capture preempts
and takes full-body ownership. No sideways-sliding forward gait or arbitrary
40–55° waist-twist overlay is required by this handoff.

For elevation, rotate **chest and all descendants**, about the chest joint,
using a target solution from a **neutral aim reference**. Keep root, pelvis,
spine and the entire leg chains unchanged. Both arm chains, hand markers,
weapon and head then share the correction, preserving stock/grip relationships.
The current reference ranges for a target at Y=2.2–2.5 m are:

| Horizontal range from base muzzle | Rifle chest pitch | Shotgun | Launcher |
| --- | ---: | ---: | ---: |
| 3 m | 17.05–21.20° | 18.03–22.17° | 12.18–16.45° |
| 6 m | 9.58–12.07° | 10.10–12.60° | 6.83–9.33° |
| 12 m | 5.07–6.42° | 5.33–6.68° | 3.62–4.96° |

The diagnostic solves the muzzle position as well as direction as the chest
turns. A bare `atan2(targetY-currentMuzzleY,range)` ignores that origin movement.
For a pitch-plane basis `(forward,up)` through pivot P, let target coordinates
be `(F,V)` and the neutral muzzle's perpendicular offset be v. The solution is
`pitch = atan2(V,F) - asin(v/sqrt(F²+V²))`, within the reviewed bounded range.
Recompute from the neutral/reference frame transformed by the current parent.
The plane example assumes the target has the same lateral offset as the muzzle;
solve actor bearing from the actual transformed muzzle for general target X/Z.

Critically, apply the resulting chest correction to the **current authored fire
chest matrix and its descendants**, including its recoil rotation/translation.
Do not overwrite that matrix with the neutral aimed matrix, and do not solve aim
from the recoiling barrel each frame: either would cancel the authored impulse.
For reference-parent mapping, reconstruct the neutral chest/weapon under the
current spine parent, solve the correction there, then premultiply the current
fire chest world matrix by that correction about the reference pivot. Child
locals stay authored. Fire's initial pose is the neutral shot frame; the kick
follows discharge. Blend the correction with aim-enter/exit, not the legs.
Head gaze is a proxy; any later look correction should use the neutral head
origin and retain the inherited authored impulse during firing.

The pitch-only serialized diagnostics preserve foot transforms exactly and
palm gaps below 1 micrometre at their sampled neutral pose; body floor remains
about 2 mm. All 42 clips separately pass key/half-key source/skin/grip/floor/loop
and transition validation. Capture contains a source root lift of about 0.55 m;
consumers must not add that same source lift again in actor placement.

## Producer/read boundary and qualification limits

`tools/build_infantry_animation.py --output-root PATH --validator-python PYTHON`
locks PATH, stages all selected bodies, validates using the configured existing
NumPy/Pillow art Python, rechecks source hashes and publishes reports/GLBs/blends
with the existing crash-recoverable transaction helper. Default output is
`build/animation/infantry`. No package is injected into Blender.
`tools/validate_infantry_animation.py --output-root PATH` takes the same lock.
Consumers take `locked_output(PATH)`, verify the producer manifest, copy only
required exports/reports into their private snapshot, and cook from that copy.
A `--only` publication is marked partial; full integration requires all three.

The existing bounded HRA encoder can consume an isolated report adapter:
`source=body_source`, `source_sha256=body_source_sha256`, preserving all original
input hashes. Do not change the original reports or legacy six cooker. Use the
actual baked times: for example run is 22/30 seconds, not nominal recipe0.75s.
The producer cold pass and two serialized standalone validators pass; generic
transaction crash/concurrency tests already qualify the reused helper on macOS.
Windows lock execution and native/mobile infantry integration remain separate.

No seated clip was added. Actual jeep review shows two empty seats and no
reviewed steering/grip datums; the rifleman's source hip width is about0.596m.
A real pelvis/thigh/footwell and hand-control fit has not been demonstrated.
Do not place a standing body in the cabin or claim crew clearance from this work.
The exploratory moving/yaw-overlay renders are superseded by the explicit
planted-fire decision and are not shipping motion or qualification evidence.
