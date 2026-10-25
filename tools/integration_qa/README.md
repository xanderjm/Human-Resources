# Native integration QA — 25 October 2026

This pass reviews the actual native product boundaries. It does not use the
abstract balance simulator as gameplay evidence.

## Findings and disposition

| Priority | Finding | Owner and result |
| --- | --- | --- |
| P1 | Gameplay forward was positive world Z while scenery forward was negative local Z; the ship and actors moved down the screen for upward input. | Animation/scene consumer corrected ship, actor, effect and facing transforms. The new public input/camera regression passes. |
| P1 | Opening an evolution recipe left a shared return screen set to upgrade; a later Hangar → Blueprints → Back could reopen an expired upgrade screen. | Client owner introduced separate library/settings return screens and records library entry. Source review confirms the stale return route is removed; normal native interaction was not observed in this pass. |
| P2 | Retrying an initial read/checkpoint-recovery failure could bank the interrupted run but remain on title instead of showing recovered results. | Client owner routes a successful cold recovery retry to results. Save-session recovery tests existed; the native retry interaction remains unobserved here. |
| P2 | Blueprint rows mixed account unlock counts with unrelated in-run draw prerequisites and displayed internal IDs. | This pass added generated display names and explicit milestone units, preserved separate draw requirements, updated portrait/wide rows and spoken summaries, and added fresh/earned assertions. |

## Durable regression

`native/tests/product_projection_tests.cpp` is registered as
`product_projection_contracts`. It uses public Controls, Game, product scene and
the actual camera matrix. It checks all four keyboard semantic directions,
upward touch drag and controller axis, ahead actors, actor facing, capture beams,
warning aims, laser endpoints, projectile placement, and the saucer footprint at
32 portrait field/station extremes, including 100 km world stations.

Reproduce and record source-bound evidence:

```sh
python3 tools/integration_qa/run_projection.py
ctest --test-dir build/dev -R '^(progression_contracts|product_accessibility_contracts|product_projection_contracts)$' --output-on-failure
```

The blueprint contract preserves IDs and raw progress/threshold values. New
`title` comes from generated module names. `milestone` names captures, base
credits, military defeats or active seconds and explicitly marks unlocked rows.
`requirement` remains the current-run draw prerequisite. No save schema, tuning,
account loader or app navigation implementation changed in this narrow fix.

Actual verification: optimized CMake build with warnings as errors passed;
focused progression, product accessibility and product projection suites passed
3/3; static repository audit and diff check passed. Accessibility coverage
includes 72 existing phone/screen combinations and the new milestone speech
assertions. Source-bound output lives under `build/integration-qa/`.

## Normal native input evidence gap

The uniquely identified portrait QA bundle launched successfully with an isolated
normal `--save-dir`; its log reports Metal, a 588×1280 render target and a missing
fresh account (`save=1`). The CUA `getApp` call then stalled for 1,183.4 seconds
before being aborted. No accessibility tree, keyboard/touch action, pause/focus
interaction, recipe navigation, death or retry was observed by this pass.
Foreground was released to the performance owner and GUI attempts stopped.
`native.stderr.log` preserves the actual startup output. This is a tooling gap,
not a successful manual playthrough or physical-device qualification.

The earlier recorded animated portrait frames were inspected against the
balanced farmland style frame. They predate the final axis/preload/label
snapshot and cannot qualify its visuals. Combat-warning readability still needs
the final late-combat native capture: the current warning line is approximately
1.3 logical pixels wide in a 390-point viewport. Do not infer visual acceptance
or fun from the projection regression.

## Proposed shared development-log entry

- Scope: independent cross-owner integration review; public input/camera
  regression; readable blueprint names and explicit account milestone units,
  separate from run draw prerequisites, in portrait/wide library and semantics.
- Decisions/owners: product projection tests and QA runner added; progression
  view adds display fields from generated names/counters; UI and accessibility
  consume those fields. Client owner fixed stale library return and cold-retry
  results routing. Animation consumer fixed forward projection/facing.
- Verification: optimized warnings-as-errors build; progression, accessibility
  and projection 3/3 pass; static repository verification passes. Source-bound
  evidence in `build/integration-qa/`.
- Caveats: QA bundle Metal startup succeeded; CUA app binding stalled and was
  aborted, so normal input/navigation/lifecycle was not observed in this pass.
  No physical mobile, visual/fun or release-ready claim. Runtime source frozen
  and foreground released for final performance/mobile qualification.
- Next: inspect final native combat/readability and normal lifecycle/navigation
  flows after profiling; retain device/signing/store gaps in the release ledger.
