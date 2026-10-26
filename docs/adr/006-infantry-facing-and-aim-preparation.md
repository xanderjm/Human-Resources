# Infantry facing and real aim preparation

Date: 26 October 2026. Status: accepted for the tested host slice. Final 23-owner
source passes all 47 suites, isolated SDL/Metal App smoke, correctly clocked native
walking/turning/warning review and an unobscured 60Hz capture handoff. Mobile
qualification is being refreshed separately. Earlier zero-delta helper films do
not qualify skeletal motion; their verdicts remain retracted.

## Context

Actual native infantry can alternate large root turns while moving without an
attack warning. The accepted Game9 public reproduction records twenty moving
non-attack turns above45degrees in45active seconds, with a158.85degree maximum.
Scene orientation follows each discrete velocity; stopped infantry instead use
an unrelated ID yaw. Smoother skeletal blending cannot correct that physical
frame discontinuity.

The existing serialized infantry locomotion vector can retain physical facing.
The existing aim-enter, aim, fire and aim-exit clips should remain the assets.
However, their fresh animation instance starts aim-enter at zero. Facing alone
cannot reconstruct an actual halfway preparation after cold recovery.

## Decision

Gameplay owns a small human turn/step controller: stable collision-admitted
guidance, quick bounded pivots, reduced translation during sharp redirection and
actual swept body clearance. Humans may pivot at rest without backing. Preserve
ordinary vehicle, boss and biological controllers.

Retain infantry physical facing in the existing serialized locomotion vector.
Publish the same heading for rendering and real weapon-frame alignment. Before
committing a warning, turn toward the provisional real shot solution. The usual
cooldown may continue during preparation; a full warning is committed only when
the real frame is aligned and the existing pressure rules permit it. Do not
shorten warning/cooldown duration or invent an extra fixed pose wait. Provisional
aim can track before commitment; the committed root/aim retain existing locks.

Add a dedicated infantry-only preparation flag and real active pose-age clock.
The clock starts at preparation entry, continues through its warning, and clears
at fire/cancellation/end. Pause freezes it. Lift/disable/quiet-window cancellation
must have explicit tested policies. Presentation receives preparation, pose age
and the existing aim point; it does not infer elapsed time from an angle or reuse
an unrelated timer.

Explicit authoritative aim phases sample existing authored clips at that age,
including the first cold draw. They bypass history-dependent previous-pose fades
only where those fades prevent equivalent live/cold poses. Fire/capture priority
and legacy inputs remain. A direct transition from a walking pose into planted
aim-enter remains a visual risk: actual entry/pivot footwork must be reviewed,
not declared grounded from matching clocks or bounded yaw. Any further pose
composition must use existing assets and respond to a demonstrated problem.

Native review exposed a second coordination defect: preparation does not reserve
an attack slot. Infantry can stop and turn while other actors take the remaining
slots. One recorded preparation lasts 1.68 seconds and accumulates nearly a full
turn after the aim-entry clip has ended. Preparing infantry and committed warnings
must therefore share one threat budget with vehicles. An owner converts its own
reservation to a warning without waiting; existing owners survive a phase-cap
reduction and release on cancellation or fire. Boss admission/drain and the global
save bound must count the same union. This adds no stored state and does not
shorten the full warning or lock the provisional target early.

Reservation alone does not establish grounded animation: the same film contains
stationary turns outside attacks. A correction limited to the beginning of
aim-enter would miss these turns. Lower-body footwork must respond to actual
physical turning while upper-body aim and weapon sockets remain coherent. The
previous whole-pose live/cold equality is an implementation choice, rather than a
reason to force AI timing to match a cosmetic clip.

The selected correction is a render-owned turn-in-place layer for grounded,
stopped infantry. It uses measured wrapped physical heading changes, alternating
foot release/replant and a short active-time settle after turning stops. A support
foot compensates for the actual root turn. Only thigh, shin and foot transforms
may change; the upper pose, weapon sockets and authoritative root remain exact.
Ordinary gait still uses actual translation speed. Capture preempts this layer;
zero active delta freezes it. A fresh/cold instance seeds its current heading and
neutral foot history rather than inventing an earlier step. Thus transient feet
may differ from a warm renderer, while upper-body pose age, firing sockets and
gameplay continuation retain their existing exactness. This adds no saved fields,
clips, models, dependencies or generic animation framework. Implementation and bounded native review are accepted. See the newest development
log entry for the final source and evidence bindings; unobserved transitions and
physical-device performance remain outside that acceptance.

Selective leg compensation and temporally stable targets follow the principles
described in [Epic's pose-warping documentation](https://dev.epicgames.com/documentation/unreal-engine/pose-warping-in-unreal-engine)
and [Khronos' glTF foot-placement tutorial](https://github.khronos.org/Vulkan-Site/tutorial/latest/Advanced_glTF/Procedural_Animation_IK/04_foot_placement.html).
The game uses its existing flat-ground rig and math; those engines' APIs and
terrain systems are not dependencies.

This separation of steering choices from bodily locomotion is informed by
[Reynolds' original steering paper](https://www.red3d.com/cwr/steer/gdc99/).
No navigation, physics, animation framework or dependency is added.

## Persistence

Game codec3/authority10/account2. The soldier-only suffix is a validated u8 flag
and float pose age: five bytes, taking its actor record101to106bytes. Ordinary
vehicles retain110bytes; other base actors retain101. Validate flag0/1, finite
nonnegative real age bounded by actor age, legal phase combinations and inactive
zero state. Public projection fields do not replace durable state.

Codec2 Game9 continuations are not reinterpreted. Existing account protection
must preserve wallet/ranks/unlocks, require a verified backup before replacement
and bank the recorded receipt once. Keep older Game7/8, corrupt and future-schema
guards. One fully valid immutable Game9 account from genuine31second public play
supplements early-rejection header tests; it must load through the new protected
account flow. It is qualification evidence, not a shipping dependency on an
ignored private fixture.

## Required evidence

Use coherent compilation across changed Actor, EntityView, ActorAnimation and
StyleInstance consumers. A historical archive must use its exact old headers.
Retain the original16body-pair/four-second bounds and crowded5400tick boss gate.
Require finite pivot/edge/crowd progress, all-role shot/muzzle agreement, warning
locks, capture/disable cancellation, exact cold projection/futures and malformed
state limits. Compare actual live/fresh skinned positions, normals and sockets at
multiple preparation/warning ages. Native temporal review must cover the original
non-attack jitter and walking-to-prepare/pivot entry, with honest foot contact.
Host checks alone do not prove human feel or physical-phone performance.

Every temporal render helper must explicitly deliver and verify the renderer's
active delta. For a public-Game run, use actual active seconds since the previous
render; modal holds deliver zero, while a frame entering a modal may include its
last active step. Record per-frame delivery and check its sum against Game time.
A controlled presentation fixture must state its explicit rate. Increasing world
time/root transforms while leaving `StyleFrame.active_delta_seconds` at zero does
not exercise skeletal playback and cannot qualify gait, footwork or recoil.
