# Native playable client

The installed `human_resources_player` now composes the portable gameplay and
account owners with semantic controls, product presentation, SDL_GPU rendering,
optional synthesized audio, accessibility and atomic local persistence. The
old counter proof remains in `foundation_proof.cpp` as lineage only; it is not
the installed game.

## Build and launch

Use `python3 tools/project.py verify` for the aggregate repository gate. Ordinary
iteration uses the optimized `dev` preset. On macOS, build the launchable app with
`cmake --build build/dev --target human_resources_macos_app`, then open
`build/dev/Human Resources.app`. The staging target copies exact generated assets
and seals the completed developer bundle with an ad-hoc signature. Do not stage
an app while it is running; stop it or stage into a separate QA directory.

`--smoke-test` renders three native frames. `--playthrough-smoke` automates real
semantic finger press/release activation through title, movement, pause/settings,
earned cards, results, Hangar and another start. It uses a separate local save
backend before loading any account. This is accelerated UI coverage; it does
not qualify organic pacing, death, physical touch or release performance.
`--capture path.rgba` writes raw RGBA frames at the renderer's logged world
extent by default. `--capture-pixels WIDTHxHEIGHT` explicitly selects an
offscreen composition extent, bounded to 4096 pixels per side and eight million
pixels total; its aspect must match the actual logical window. It requires a
capture path and never changes the world target or reads the write-only
swapchain. The helper's matching option records actual capture dimensions;
FFmpeg input-size flags alone cannot change native rendering.
`--width 375 --height 667` selects a smaller portrait window for layout review.
The default is portrait 390×844; wide desktop layouts remain supported.

Normal presentation keeps the longest-side 1280-pixel world/MSAA target, then
draws screen quads in a single-sample pass at the actual physical swapchain
extent. Text/buttons therefore avoid the world's upscaling step. Normal play
allocates no native-sized world/depth target. Explicit captures alone allocate
cached RGBA8 composition/readback storage: at most 64 MiB combined, about
24.1 MiB at 1206×2622. Shaders, atlas, layout and semantic coordinates are shared.

`--save-dir PATH` selects an isolated normal-input account without automation.
`--qa-frames N --qa-step-seconds 0.0166667 --qa-seed 1 --qa-metrics PATH.csv`
records bounded, fixed-step CPU/driver timings and draw counts, automatically
choosing upgrades and retrying after real deaths. The metrics directory owns a
separate `qa-save/`; use a fresh output directory for a matched profile.
`--qa-save-dir` overrides that path. Test backends are selected before any load.
The normal client uses one 60 Hz frame deadline; capture runs are unpaced.
CPU timings are not GPU measurements.


## Text size and flight inspection

The playable reads the native system text preference when rebuilding its UI;
changes apply without restarting the game. Reduced motion uses the same current
preference sample. Font sizes grow from each existing type role and the compact
14-point minimum. Fitting layouts retain their controls; overflowing content uses
measured heading/body pages with safe, fixed navigation and action targets.
Primary title, Pause, settings and results controls stay together where they fit.
Readable upgrades lead with the actual offer name. Three direct Preview controls
let players compare offers without traversing another offer's description; detail
continuation pages keep the original choice identity. Preview state is transient
and never chooses or persists an upgrade. Large tactical UI
keeps hull and Pause in one top row; Pause contains the secondary flight stats.

Pause **Current Build** lists installed hardware/ranks, this flight's temporary
research and evolutions, and separately labelled permanent Hangar ranks. This is
an inspection view; page position is transient and does not alter or save rules.
Screen, offer, protected-save, viewport and preference changes clear stale page,
pressed and focus context. Protected-save pages retain only their explicit retry
and verified-backup recovery actions. English uses the existing shipped atlas;
this change does not select an optional general text/shaping system.

Settings **Privacy policy** opens a complete measured offline reader. Back
returns to Settings, page state is transient, and protected saves keep exclusive
recovery actions. The copy explains local storage/backups and absent online
services without inventing a developer contact or public URL. Actual store
publication still requires genuine identity/contact/support and a public policy;
see [STORE_PREPARATION.md](STORE_PREPARATION.md).


## Shipping content boundary

`assets/runtime/game-content.json` explicitly selects the consumed Farmland, Suburbia and Desert
models, materials and approved fonts. The runtime cooker imports existing
reviewed authoring sources with the repository's existing NumPy/Pillow art tools,
produces bounded native HRM meshes, eight HRA skeletal-animation companions and an ASCII font/graphic atlas under ignored
`build/.../generated`, and binds every consumed source hash in the runtime
package manifest. Generated meshes, atlases and headers are never source
assets. `python tools/project.py bootstrap` prepares pinned, hashed offline tools under
ignored `build/tools/`. `GAME_ART_PYTHON` and `GAME_BLENDER` are strict explicit
overrides; a checked scoped install, current interpreter or Blender on PATH
can satisfy the same contract. There is no private application-cache dependency.
Missing/stale animation exports rebuild from source recipes through the selected
Blender and art validator. See [OFFLINE_ART_TOOLCHAIN.md](OFFLINE_ART_TOOLCHAIN.md).
Python, Blender, font rasterization tools and source GLBs do not ship.

The atlas uses the approved Beam UI and Abduction Compact typography plus the
existing HUD panel, upgrade card, coin, shield, beam, laser and XP graphics.
Text quads batch in groups of 128; static mesh/texture assets preload before play.
The renderer owns per-entity animation players, deformed bounds and GPU uploads. Runtime shaders are offline HLSL. Android
loads APK asset names through SDL_IO rather than filesystem probes.

The foundation package audit retains exact output inventories and additionally
checks the recipe's derived source hashes. Only the selected sources are
consumed; unused authoring models and development HUD screenshots
are excluded from this product content cook. Native art promotion is a
playable integration candidate, not a physical-device visual approval claim.

## Ownership and controls

`human_resources_product` owns presentation and the floating-stick contact
policy. It reads gameplay snapshots and account views; it does not mutate rules
or durable progress. `human_resources_client` dispatches commands to those
owners. The scene consumes monotonic runner distance separately from bounded
player dodging and fades foreground tree occluders near the saucer.

Drag an open part of the scene to fly; another finger may press HUD controls
without stealing the movement contact. Keyboard WASD/arrows and controller left
stick map to the same semantic movement. Enter/controller south activates a
focused menu action; directional actions move focus. Escape/controller start
pauses. Space/controller east or the Dash button triggers the starter Dash;
Phase Drive hardware improves it. The base cooldown is four active seconds.
Dash also triggers an ordinary counter-volley when a valid target is available.
Flying close while moving accelerates capture; three qualified close sweeps
within five active seconds grant six seconds of Overdrive capture/fire cadence.
Level-up pauses the run for exactly three choices. Focus loss and
background cancel input, freeze simulation and discard elapsed catch-up.

## Remaining qualification

Host builds and native Metal captures do not prove iPhone/Android execution,
thermal behavior, battery use, safe-area hardware, touch precision, VoiceOver,
TalkBack, storefront signing or store review. Synthetic audio remains an initial
feedback pass with no licensed music; haptics are unavailable. Live advertising
and billing are absent. Optional reward test doubles cannot be represented as
live rewarded ads. Physical-device visual review and performance budgets remain
release gates. Skeletal animation integration and native source-parity evidence are tracked by
`ANIMATION_RUNTIME_HANDOFF.md`. Actual game clips freeze during pause, menus,
background and save protection; the temporary organic bob has been removed.

AccountSession retains exact unsaved operations and full receipts. Failed writes
block conflicting mutations and expose Retry Save; protected corrupt/future
saves can be reset only after a verified backup. A cold recovery retry opens
results after successful settlement. The Hangar library shows blueprint progress
and earned evolution ingredients/effects, sourced from the existing catalogue.

The title location control cycles all three existing scenery kits before Start.
Farmland is the default; selection stays within the app session and does not
change account schema or shared gameplay rules. Suburbia uses the animated
resident/nightcap human target. See `WORLD_CONTENT_HANDOFF.md` for exact inventory,
measured output cost and focused integration evidence.
