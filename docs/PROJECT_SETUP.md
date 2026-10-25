# Human Resources project setup

This is the historical 24 October foundation-adoption plan. The installed
product has since become the native portrait playable, with cooked textured
meshes, eight animation pairs, gameplay/progression, protected saves and native
UI. The implementation-order/proof-client statements below describe that
starting checkpoint. Use `PRODUCT_CLIENT.md`, `RELEASE_READINESS.md`,
`MOBILE_QUALIFICATION.md` and the newest development-log entry for current
implementation and exact qualification limits; neither sibling is required.

## Sources and adoption

Game Scaffold and Midnight Menu are separate repositories. Human Resources owns a generated snapshot and its existing Git remote. Neither sibling is a submodule or a build dependency.

| Source | Adopted here | Purpose |
| --- | --- | --- |
| Game Scaffold current `native_game_foundation/` | Native Foundation 2.1.0-dev, five profiles, independent capability libraries, shells, cooker, audits, tests and tooling | Canonical starting point including 24 October maintenance fixes |
| Midnight Menu `CMakePresets.json` | Optimized `dev`, separate `debug`, explicit Debug instrumented presets and matching tests | Usable iteration without losing debugging or analysis |
| Midnight Menu workflow policy | Manual triggers for every retained workflow | Explicit hosted execution with complete audit/evidence jobs |
| Midnight Menu dependency cache | Independent copies of exact SDL and recursive SDL_shadercross checkouts | Avoid redundant downloads; upstream bootstrap remains reproducible |
| Midnight Menu installed shader compiler | Copied executable and runtime libraries through supplied-tool bootstrap | Independent local native shader tool bundle |
| Existing Human Resources files | Art, fonts, GLBs, production inventory, model tooling and provisional balance | Preserve product direction and authoring work |

`foundation.lock.json` records the generator snapshot. `setup.sources.json` records reference commits and source-file digests, including working-tree versions. Later product edits deliberately differ from pristine generation. Copied dependencies/tools live under ignored `external/` and `build/`. A new clone bootstraps exact upstream commits; no source or runtime path requires a sibling checkout.

The scaffold already includes Midnight Menu input metadata/axis fixes, bounded validated asset reads, generated-output ownership, matching device/Simulator Metal targets and isolated fixtures. Its older immutable ZIP does not include all these fixes.

## Assets and shipping boundary

- Start with `assets/models/ufo-modular/ufo-modular.glb` and the upgraded assembly. Preserve `Outer_Ring_Pivot`, fixed hull and attachment sockets using the assembly guide.
- The cow, farmer, sheep, pig, soldier and modular vehicles are integration candidates. Check `assets/production/inventory.json` and individual reviews before promotion.
- These are textured GLBs. Midnight Menu instead uses procedural recipes and its own cooked mesh format. Define the small imported-mesh/texture contract needed here before considering reuse of renderer code.
- Existing model tools and `tools/model-viewer/` remain authoring tools and are excluded from game packages.
- `design/balance/endless-upgrade-system.json` owns the proposed module catalogue; `archero-ufo-system.json` and `archero-ufo-approach.html` define the latest candidate using the owner-selected Archero progression model. The previous manual-loadout, twenty-to-thirty-minute and finite-run studies are historical. Frozen simulator snapshots are evidence, not runtime authorities. Introduce validated runtime conversion when gameplay consumes agreed tuning; avoid independently maintained constants files.
- Select runtime font roles and a rasterisation path with HUD work, retaining font notices. The proof client does not render these TTFs.
- `assets/runtime/` is the shipping source allowlist and starts empty. The cooker emits exact inventories; package audits reject missing or extra files. Do not stage the whole art/assets directory.

## Implementation order

1. Add bounded offline GLB import/cooking, texture handling and a native mesh draw path. Prove the saucer hierarchy and cow, coordinates, appearance and device package loading.
2. Add a portable `human_resources_gameplay` target owning movement, capture and run state, with headless tests. Add semantic movement/gesture mapping through a controls owner; SDL stays in adapters.
3. Add presentation data and camera behavior in a product presentation target. Renderer consumes scene data without mutating gameplay. Adapt Midnight Menu's ownership patterns only where this game has a real consumer.
4. Implement pressure, lasers and three random upgrade choices per earned level for the owner-selected endless runner. Use the current proposal and balance review as playtest inputs; retune actual encounters before treating numbers as final. Keep simulation time and RNG ownership explicit.
5. Add title/pause/results, exact-once Research Credit settlement, capped Hangar upgrades, automatic blueprint achievement unlocks and versioned product saves through existing boundaries. Use local revive/credit-bonus test doubles until the loop works without live services; font rendering and optional audio stay separate owners. Qualify a playable iPhone slice before growing content/editor scope.
6. Implement the separately planned monetisation adapter after a playable app exists, with consent, reward/dismissal ordering and physical-device checks.

Do not copy Midnight Menu combat, food/truck content, saves, procedural creatures, studios or large shader suite merely to obtain its architecture. A specific reused owner needs its complete interfaces, dependencies, assets, tests and an actual Human Resources consumer reviewed first.

## Qualification

Use the root README commands. `project.py bootstrap` recreates exact dependencies and isolated host tools. Every retained build preset has a test preset that rejects empty suites. The development log records actual checks here; inherited historical foundation evidence does not qualify Human Resources. Device signing, ads/purchases, physical mobile execution, product budgets and unexecuted platforms remain open.

## Development visual study — 24 October 2026

`docs/STYLE_PREVIEW.md` documents the owner-requested native three-location film.
It provides a portable mesh decoder, offline GLB cook, independent scene
presentation and SDL_GPU textured/shadowed rendering in an explicit development
viewer. Its assets/shaders remain outside shipping allowlists. The foundation
client is still the installed UI proof. Runtime packaging, product gameplay and
device integration remain the next production milestones; this visual study
provides review evidence for those decisions.
