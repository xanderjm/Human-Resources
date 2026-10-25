# Capture and combat presentation

25 October 2026, uncommitted. This is a native presentation milestone, not release qualification.

## Ownership and contracts

- Gameplay exports a fixed 64-entry `PresentationEvent` ring in `Snapshot`. Each confirmed capture, nonfatal enemy hit, defeat, absorbed shield hit, hull hit, actual laser volley and committed warning lock has a per-run increasing sequence, stable actor ID where applicable, exact world position and active timestamp. Startup clears the ring. Emission does not consume RNG or change damage, collision, targeting, encounter timing or balance.
- Product `SceneEffects` consumes each sequence once, keeps at most 48 recent visual beats and 128 actor records, and expires beats after 0.55 active seconds. It never infers a death from disappearance or a collision from rendering geometry. Old effects are dropped at capacity. The latest update also exposes coalesced fresh semantic feedback flags for audio/haptics. Identical paused snapshots clear fresh flags without advancing any clock.
- Product scene presentation owns curved capture convergence, exact banked saucer intake placement, ground-to-intake cone orientation, sparse mint directional rails, full 3D laser endpoint geometry, committed coral warning dashes, existing missile models, small outcome beats, integrated jeep wheel travel and turret aim. Existing skeletal playback, actor IDs and head socket attachments remain intact. Scenery composition and source models/materials are preserved.
- The client owns the cache/reset, semantic audio and optional haptics calls, selected session location and theme command routing. Ordinary saves/settings remain with AccountSession. No new account schema is introduced.

The cache and geometry have separate product owners. `app.cpp` remains the executable composition root; the narrow lifecycle/feedback/diagnostic integration brought it to approximately 805 lines. Further diagnostic option growth should extract the existing QA CLI/capture policy together, rather than adding more unrelated branches or moving product rules into the client.

## Time and diagnostic capture

`--qa-theme farmland|suburbia|desert` selects a session world with validation. `tools/playable_qa/capture.py --theme ...` passes it through to the same executable.

`--qa-capture-skip-frames N` runs and renders the first N real fixed-step QA frames without recording readback. The capture helper exposes this as `--skip-frames N`, adds N to the total QA frames and records N in its provenance. The ordinary Game, RNG, movement bot, deaths, choices, saves and renderer still run. No invulnerability, fixture state or difficulty override exists. Metrics include every skipped frame; movie frame zero corresponds to metrics row N. Use a 1/60 step for normal-speed recorded segments. A skipped warmup does not qualify uninterrupted human play or live frame pacing.

## Verification and review

Focused optimized native suites cover semantic input/projection, exact cone tip at the banked saucer socket, full 3D weapon endpoints, horizontal capture convergence through the intake, stable event sequence and counter equality through a real public Game run, event ring wrap/pause/restart, explicit-outcome-only feedback, bounded cache expiry, six-beam guide capacity and static reduced-motion rails. Audio scheduling regressions cover one fresh volley/lock notification with persistent 120 ms ShotViews at 60/120 Hz and 100 ms callbacks.

The first review snapshot is retained in `build/captures/combat-presentation/`: source-stable normal-speed farmland (2,100 frames / 35 s), Suburbia and Desert (720 frames / 12 s each), plus the accelerated farmland menu-flow capture. These predate the deliberate mint outline/audio/haptics/UI/palette correction. The actual first upgrade screen is farmland-flow frame 98 (`actual-upgrade.png`); the similarly named `upgrade-*.png` samples showed the run and are not card evidence. That actual screenshot exposed the card atlas/text inset problem and was sent to the UI owner.

The first farmland film contains captures but zero incoming projectile frames; it is not attack or missile evidence. Final combined native capture and exact verification results are recorded below after source freeze. Existing `*.json` capture records retain executable/source hashes and full decode checks. A 60 fps encoded movie is not a live frame pacing measurement.

## Remaining limits

Physical phone touch, GPU/thermal budgets, haptic feel, listening quality and human fun are unqualified. Existing shield rim/sphere and laser texture integration remain a separate visual slice; this pass uses existing cooked geometry and preserves renderer preload/fence/batching contracts. The event ring intentionally bounds retained outcomes and can discard old presentation events when a consumer does not observe them in time; simulation outcomes remain authoritative. Full selected-platform and store release gates stay open.

## Final combined checkpoint

`build/evidence/combat-presentation-final.json` consolidates the exact executable/source snapshot, four source-stable fully decoded native films and selected PNG hashes. All four final films use the same source snapshot. Final combined optimized build and six focused suites passed: the two product projection suites, accessibility, world content, scene effects/semantic feedback and portrait layout.

- `final-farmland.mp4`: 720 frames, 12 seconds at 1/60 simulation steps. Frame 600 (`final-capture-action.png`) shows the stronger mint directional rails, source-palette correction and convergent intake. No further beam-detail iteration follows this accepted milestone.
- `final-late-combat.mp4`: 7,200 real Game/GPU frames precede 720 recorded normal-speed frames, covering 120.017–132.000 active seconds. Incoming projectiles occur in 617 recorded frames, with a peak of four. `final-attack-01.png` is frame 60: committed coral aim dashes and the four open ground-reticle ticks remain visible after a dodge. The middle frame shows actual jeep pressure, missiles and capture/damage feedback. The warmup metrics and capture mapping remain in the adjacent CSV/JSON.
- `final-flow-390.mp4`: 280 actual accelerated UI-flow frames at 390×844 logical points / 588×1280 render pixels. `final-menu-390-01..04.png` are title frame 0, real earned-card frame 88, results frame 235 and Hangar frame 243.
- `final-flow-320.mp4`: the same real UI flow at 320×568 logical points / 640×1136 render pixels. `final-menu-320-01..04.png` are title frame 0, real earned-card frame 83, results frame 235 and Hangar frame 243. Actual cards fit/read clearly, with the earlier atlas contamination and description inset defect removed by the UI owner.

The final snapshot includes other owners' frozen UI, source-palette, biological-separation and optional haptics changes. Their dedicated reports own those implementations. The normal clips and UI films use isolated QA accounts; prior review recordings in the same folder can contribute QA progression, so these are visual proofs rather than matched fresh-account balance comparisons. The UI flow uses its explicitly isolated test backend. Videos intentionally omit audio and do not qualify listening quality.

Native/runtime/tool ownership is now handed to the next gear/evolution presentation owner. Preserve the explicit volley/warning-lock event sequence and single-consumption audio/haptics behavior when extending shot/event metadata. Continue the coordinator's profile/mobile schedule before modifying its frozen inputs.
