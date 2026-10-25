# Native biological overlap — 25 October 2026

## Finding and bounded fix

The suburbia/desert middle captures show actual ground-target clumps, beyond capture lift. Native spawning previously accepted overlapping roots, and passive targets walked independently without neighbour separation. Eight public native runs confirmed repeated deep intersections, including a cow pair overlapping for 13.6 seconds. This warranted a small gameplay correction rather than a physics system or presentation offsets.

The current fix adds deterministic spawn clearance and bounded separation for passive organic targets. It reduces measured body-core overlap by **94.8%** in the same sample. It deliberately does not enforce disjoint limb/horn bounding boxes or claim zero visual intersection.

## Measurement

`python3 tools/gameplay_qa/biological_overlap.py` compiles real production C++ and uses only the public `Game` API. Four seeds (1, 21, 42, 514), stationary and specimen-seeking controls, run naturally for 120 active seconds each. There is no invulnerability, stat injection, private state access or Python balance simulation. Both policies select ordinary offered upgrades, preferring capture hardware. This is an overlap study, not a balance or skill model.

Snapshots are sampled at 10 Hz: 9,600 samples total. Both bodies must have **zero capture progress**, lie inside the actual 588:1280 portrait camera frustum, and survive the scene's 32m root culling. The test uses the production overhead camera matrix and the scene's gameplay-to-render Z mapping. Camera-visible means within the frustum; it does not prove unoccluded pixels, account for HUD coverage or resolve animated triangles.

Footprints come from consumed HRM vertex/index data via `measure_body_footprints.py`. Whole-body yaw-oriented X/Z boxes identify candidate contacts; they include limbs/horns and are not penetration proof. The stricter metric uses root-centred disks inscribed in actual neutral body triangle cross-sections at common Y=0.5m. These exclude hats. Animation can deform those cross-sections, so they remain explicit neutral-body proxies. The screenshots provide the complementary visual evidence.

| Body | Measured neutral core radius (m) | Authored separation radius (m) |
| --- | ---: | ---: |
| Farmer | 0.112163 | 0.237 |
| Resident | 0.236826 | 0.237, shared farmer kind |
| Cow | 0.435716 | 0.436 |
| Sheep | 0.254663 | 0.255 |
| Pig | 0.220903 | 0.221 |
| Soldier | 0.161195 | 0.162, obstacle only |

The farmer kind can display either farmer or resident art, so gameplay uses their larger measured radius consistently across cosmetic themes. The audit uses the smaller farmer radius, a conservative cross-theme deep-overlap test. Full measured boxes, body hashes and exact radii are retained in `build/gameplay-qa/biological-overlap/footprints.json`.

## Same-seed observations

| Policy, four runs each | Before: samples with a core overlap | After | Before/after episodes ≥1s | Before/after longest episode |
| --- | ---: | ---: | ---: | ---: |
| Stationary | 21.85% | 0.71% | 41 / 1 | 9.0 / 1.4s |
| Specimen-seeking | 21.23% | 1.52% | 40 / 3 | 13.6 / 1.4s |
| Combined | 21.54% | 1.11% | 81 / 4 | 13.6 / 1.4s |

Before the correction, a farmer/sheep pair's roots reached 2.4mm separation despite both having zero capture progress. The post-fix sample contains short residual crossings, often after a ground actor becomes available again. These are handled gradually rather than teleported apart. Whole-body box contacts remain common (about 80% of samples); those loose boxes include protrusions and do not establish remaining body-core interpenetration. Final motion/silhouette acceptance belongs to the coordinator's native visual comparison.

The initial baseline was measured before the production edit; its console-observed per-run counts and hashes were recorded during review. The normal runner overwrites the latest raw trace. The historical baseline log hash was `bacb5e9a0aa4d1dce24f3eb694e557caa25d242ed3f585892959e4b8ba8d8cc9`; the source-manifest hash was `18077c161dabe2fa7f98b3827b8aebae4d7bf11d7bd68813540ead68c59e6be6`. Current raw evidence is the post-fix run. The summary above preserves the observed baseline rather than presenting it as a newly rerun dataset.

## Production ownership and limits

- `runtime-spatial.json` owns the source-derived radii, four spawn candidates and 0.6m/s maximum separation contribution; `generate_tuning.py` validates/emits them.
- `body_separation.cpp` owns passive ground motion/clearance. Four candidate positions reuse the director's existing samples; no encounter RNG is added. A blocked attempt can be omitted, like an existing population-cap omission. Attempt count, sample consumption and schedule hash ordering remain unchanged.
- Two stable-ID ordered all-pairs passes move at most 28 passive bodies. The total separation displacement stays within `0.6 × dt` metres per actor, in addition to intended wandering. Roots remain within the existing field bounds. Preferred wandering is retained separately; `EntityView.velocity` reports actual ground displacement.
- Capturing, lifted and removed actors are excluded from movement and neighbour resolution. Passive targets yield to uncaptured soldiers, whose positions, military steering, contact, warning aims and attack timers remain untouched. Vehicles and bosses do not participate.
- Positions are authoritative gameplay positions before capture/combat consume them. Scene roots and beam targets continue to use those same owned values. No presentation-only separation offsets, new physics library, reward change or difficulty retune were introduced.

The source changes in `director.cpp` only add passive spawn clearance, preserve its initial wander heading and delegate passive motion. Existing combat events and military logic remain intact. `run_state.h` adds the intended wander field/private helper declarations; local gameplay CMake includes the helper. No client, capture or combat implementation is edited by this fix.

## Focused verification and cost

`python3 tools/gameplay_qa/body_separation_tests.py` compiles actual production methods with access relaxation confined to the test fixture. Eight groups pass with stable source hashes:

- Exact coincident roots separate smoothly with finite state, truthful velocity and unchanged intended heading.
- Field-edge movement stays bounded without teleporting or exceeding its displacement budget.
- Capture/lift/removal exclusions are honoured; nearby military position, aim and timers remain unchanged.
- One hundred seeded initial fields have clear core footprints; bounded placement consumes no extra RNG and retains spawn/hash accounting.
- Public pause freezes motion; eight paired public director prefixes remain identical across account power/draft differences.

The optimized host microbenchmark with 28 passive bodies and 18 soldier obstacles averaged **3.99µs per helper step** over 20,000 steps. At 60 Hz that is approximately 0.24ms per simulated second on this host. This is absolute helper cost, not a physical-device or full-game performance claim.

The normal optimized native gameplay target builds. The existing native probe rerun passes all **52 checks**, zero failures, with stable hashes, including module/research/evolution semantics, capture/kill reward exclusion, late drafts and sixteen paired five-minute schedules. Repository static verification passes. Root owns aggregate/client/device qualification and test registration; this pass does not edit root CMake or the shared development log.

Evidence directory: `build/gameplay-qa/biological-overlap/` contains `review.log`, `pairs.csv`, `sources.json`, `footprints.json`, `body-separation-tests.log` and `body-separation-test-hashes.json`. Current production sources were stable during the recorded measurements.

Final SHA-256:

- Post-fix overlap `review.log`: `8160e3e4bea798b97b18cbf7b5c6146c39739a260940c9405975d88a83f39527`
- Overlap `sources.json`: `3d18dd4f5cdbc43d1fea32ebd82e9cdd9e169361bc4fcad7d57ec51a43a26a6f`
- Focused `body-separation-tests.log`: `713311500b0ef6f7e1800ad58a75e28c88862f94027203f1ea10d3951b30b8b1`
- Existing `native-probe.log`: `99956681cf622949d149d37f81e0947f3842a001a65e069d5b878975b1056660`
- Existing native `source-hashes.json`: `e7cbd3e61c7af6998a70ac3542ac4965584e129e7e0f8d946e1566fe59cf5896`

### Coordinator's development-log handoff

25 October 2026, uncommitted: fixed passive biological clumping through source-derived body radii, deterministic bounded spawn clearance and two capped local separation passes. Positions/velocities remain gameplay-owned; capture/lift and military rules are preserved, without additional RNG or renderer offsets. Eight natural same-seed two-minute runs reduce conservative core overlap from 21.54% to 1.11%, long episodes 81→4 and longest 13.6→1.4s. Eight focused groups and 52 existing native checks pass with stable hashes; optimized gameplay build/static audit pass. The 28+18 host microbenchmark is 3.99µs/helper step. Root owns final visual, device and aggregate release acceptance and focused CTest registration.
