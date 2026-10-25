# Frozen infantry/spatial native release refresh — 25 October 2026

The existing native effects audit passes **52 checks, zero failures** and the
original release audit passes **18 checks, zero failures**, with **96 natural
native runs and 360 saved account attempts**. This is portable C++ evidence,
not the abstract Python balance simulator or a rendered/device playtest.

Reproduction, executed once for this refresh:

```sh
python3 tools/gameplay_qa/release_review.py \
  --output-dir build/gameplay-qa/infantry-spatial-refresh/release-review \
  --freeze build/military-variants-prep/portable-source-freeze.json
```

The revision-2 role/planted attack/muzzle/projectile and portrait geometry
freeze matched **all thirteen listed source hashes before and after** compilation
and execution. Its manifest stayed byte-identical. All **48 broader audited
inputs** also stayed unchanged. Source discovery includes the current
`military.cpp`, `save_state.cpp` and body-separation implementation.

Only the Python runner changed: optional output directory with its historical
default preserved, plus explicit freeze verification that fails on a mismatch.
No C++ fixture adaptation was required. Original draft policies, steering,
assertions, seeds, run counts and simulated horizons are unchanged. Historical
`build/gameplay-qa/release-review/` and `release_review.md` remain untouched.

## Natural pacing comparison

Twelve seeds (21–32) per row, actual public `Game` movement/drafts/damage/death,
60 Hz, no revive or stat intervention. These compare the complete current
frozen source set with the historical release evidence; several production
owners changed between them, so differences cannot be assigned to infantry or
separation alone.

| Account | Policy | Historical median min | Current median min | Current range min | Median change |
| --- | --- | ---: | ---: | ---: | ---: |
| Fresh | Automatic baseline | 3.10 | 2.69 | 2.55–3.21 | −13.1% |
| Fresh | Collector | 3.18 | 3.15 | 2.60–5.14 | −0.9% |
| Fresh | Offense | 4.40 | 3.56 | 2.59–5.59 | −19.0% |
| Fresh | Defense | 5.26 | 5.22 | 3.22–6.69 | −0.7% |
| Maxed/all unlocked | Automatic baseline | 4.76 | 4.22 | 2.83–6.09 | −11.3% |
| Maxed/all unlocked | Collector | 6.57 | 5.54 | 3.21–6.14 | −15.7% |
| Maxed/all unlocked | Offense | 5.60 | 5.59 | 3.10–6.95 | −0.2% |
| Maxed/all unlocked | Defense | 5.91 | 5.62 | 5.24–7.69 | −4.9% |

All 96 runs die before twenty active minutes. Observed natural peaks are 21
actors and 12 projectiles. Fresh bot medians remain below the 5–10-minute
candidate target except Defense. This preserves the earlier human playtest
qualification gap; it is not an instruction to retune pressure from bot results.

Counterfactual diagnostics remain separate: fresh Collector median is 189.092s
with production rules, 297.750s when bosses are retired without rewards, and
291.767s when tanks are retired without rewards. These thirty-six modified
runs identify meaningful pressure families but do not qualify real survival or
justify removing them.

## Positive saved progression

Twelve independent accounts, four per Collector/Offense/Defense policy, each
complete thirty sequential attempts. All **360** actual begin/settle/purchase/
reopen cycles succeed, with byte-identical reopened account encoding. No fake
currency, ad bonus or revive is supplied. Every final account has sixteen
blueprints and at least sixteen purchased permanent ranks, so these are
meaningful positive journeys rather than empty successful saves.

- First earned choice: **13.52–14.02 active seconds** on each account's first attempt.
- First purchase: attempt **1–2**; ten accounts purchase on attempt one and two
  Defense accounts on attempt two. Previously all twelve purchased on attempt one.
- First free blueprint: attempt **1–2**.
- All sixteen blueprints: attempt **13–16**, previously **11–15**.
- Three Collector accounts cap all twenty ranks at attempts **23, 24 and 25**;
  the fourth has nineteen ranks after thirty attempts. Previously all four
  capped between attempts 22 and 26.
- Collector thirty-attempt base earnings: **5,919–7,989 credits**; Offense
  **3,908–4,357**; Defense **3,832–4,167**. Rewards remain positive and usable,
  while policy-dependent payout differences remain a playtest concern.

The purchase policy buys an affordable alternative track instead of saving for
one preferred track. Unlock/cap times depend on that policy and on these seeds.

## Boundary and stress evidence

All original nonfinite-time/control, permanent-cap, capture-reset and paused-event
assertions pass. The hour-long six-slot stress fixture retains finite state and
three distinct late draft families: level 82, damage 95.52, peak ten actors/four
projectiles. That fixture explicitly grants test-only invulnerability and is
not natural-run or immortality evidence.

Bots do not learn the new infantry roles, deliberately time Phase Drive, reason
about attack animations, or reproduce human touch, latency and warning
comprehension. They use the original simple nearest-target, predicted-projectile
and committed-warning avoidance heuristics. No fun, retention, revenue, rendered
muzzle/trajectory alignment, speaker, accessibility, mobile thermal or release
qualification follows from this audit. Parent-owned effect probes and current
native visual review remain separate evidence.

## Exact evidence

### Native effects regression

```sh
python3 tools/gameplay_qa/run_native_probe.py \
  --output-dir build/gameplay-qa/infantry-spatial-refresh/native-probe \
  --freeze build/military-variants-prep/portable-source-freeze.json
```

All 52 existing effects/reward/draft/pause/director checks pass. Synthetic
`EntityKind::soldier` targets now explicitly select `InfantryRole::rifleman`;
`none` no longer describes a valid ordinary infantry fixture. This does not
change natural spawns or their role selection. The old ±8.25m assertion was
adapted to the frozen configured field half-width (5.65m): normalized full-axis
input must still reach each exact edge within 1mm and an outward dash must
remain bounded. No threshold or behavioural assertion was relaxed. The freeze
check independently pins the intended numeric geometry.

The native runner verifies all thirteen freeze entries and byte-identical
manifest before/after, and its broader source hashes remain stable. Historical
`build/gameplay-qa/native-probe.log` and its manifest are preserved; new output is
isolated under `infantry-spatial-refresh/native-probe/`.

| File | SHA-256 |
| --- | --- |
| Native `native-probe.log` | `1a2347cab42d22ffd4e24b63cd2ba18959fec65087fa77c9e21e43db6b80604f` |
| Native `source-hashes.json` | `2787df1ba181f20775bcbf2836c3f4ea78c0cb74a8b8d0e3aaaef403e9a29af6` |

### Release runs and saved journeys

All outputs are under
`build/gameplay-qa/infantry-spatial-refresh/release-review/`:

| File | SHA-256 |
| --- | --- |
| `review.log` | `ee1021fa41ff116982a9569c95e0652873de13b25860b5192a6b6cbf8a8094ef` |
| `sources.json` | `b209b35503fc4382acf24aa2a1082fcb8e10790c034ea91716945272b10873f9` |
| `freeze-verification.json` | `a2c5ce0a37d111a73e39a048c7c41162b5571a80bd44dd452d0480e4722477dd` |

Freeze manifest SHA-256:
`e69d7682310b5920bcf1fce5e5079ba5238744f356a1f69295f0e3d451e4d01f`.

No production, root/local CMake, rendering, mobile or shared development-log
files changed during this narrow QA refresh.
