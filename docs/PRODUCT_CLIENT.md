# Native playable client

The installed `human_resources_player` now composes the portable gameplay and
account owners with semantic controls, product presentation, SDL_GPU rendering,
optional synthesized audio, accessibility and atomic local persistence. The
old counter proof remains in `foundation_proof.cpp` as lineage only; it is not
the installed game.

Tractor beams now commit acquired targets through completion. Pickup range still
gates acquisition; capture time, capacity and movement cost remain. Flying away
continues the lift at the ordinary rate once close-sweep conditions cease. See
`TRACTOR_BEAM_RULES.md` for runtime, upgrade and continuation ownership.

The native population candidate now gives each animal/civilian type and equipped
infantry role a distinct lift load and capture score; jeeps and tanks have their
own weighted durations/rewards through the existing Winch gates. The populated
opening starts with a farmer, two sheep and a pig. Cattle and more people precede
rifles, shotgun teams, jeeps, rocket screens, tanks and the first boss. Local
species escape, measured passive ground clearance, complete military formations
and their approach slots are owned by portable gameplay. Easy small-animal
pockets remain available later. These rules are shared by all three locations;
Suburbia keeps its resident appearance. See `ENCOUNTER_POPULATION_PLAN.md` and
`adr/008-population-and-abduction-progression.md`. Game codec6/authority14 retains
the existing flight/laser fields and account2 protection. Current host/native
verification is separate from owner-app delivery and physical-phone acceptance.

Owner flight clarification: released controls keep the ship steady on screen,
with equal forward/backward/lateral steering response. Scroll changes preserve
pilot velocity and vertical-edge stops retain runner carry, so neither world
acceleration nor the ground's direction changes steering response. Existing
capture slowdown, thrusters and Dash remain. See ADR007 and the newest log for
focused flight proof and the separate concurrent shared verification gaps.

Ship hardware now changes the physical assembly: wider and stepped rotors,
complete capacitor cells, replacement barrels/muzzles, branched capture ports,
working winch jaws, shield crystals, repair docks, engine pods and fitted Phase
canopies. All twelve current hardware families have three distinct physical
ranks. Ring speed follows Fire Rate research and actual Overdrive; drones clear
the complete rig. See `SHIP_UPGRADE_VISUALS.md` for geometry, source cooking,
real emitter ownership, reduced motion and exact verification limits. The four
retired hardware families remain absent from this assembly and shipping content.

The latest action-pacing candidate is described in
`adr/011-action-pacing-and-military-escalation.md`: faster weighted lifts and
travel, doubled civilian delivery, earlier military introductions and up to six
complete formations. Codec6/authority17/account2 retains protected incompatible
continuation handling. Existing owner apps remain separate delivery checkpoints.

## Build and launch

The latest enemy candidate uses the scrolling combat frame and persistent
engagement in `adr/012-persistent-enemy-engagement.md`. Infantry faces the player
while independently travelling and visibly backpedalling; full warnings stay
dodgeable. Military no longer expires with age or scroll distance. Authority19
retains codec6/account2 and protected incompatible-run handling. Native candidate
verification and delivery remain distinct from older owner apps.

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
The default is portrait 390×844. The game is portrait only: desktop preview
requests rotate width/height into portrait, enforce a320×568minimum and a3:4
maximum window aspect, and remain resizable through phone/tablet portrait sizes.
iOS/Android shells and the SDL orientation hint retain their portrait lock.
Separate landscape menus are outside the product scope.

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
The newer Atomic Enamel composition adds one checked secondary2048x2048atlas
with reusable blank panels, icons, the preserved title logo, live display glyphs
and an offline-authored upright label companion. Nine/three-slice frames and
aspect-preserving icons are shared across ordinary portrait compositions.
The real first promotion, ordinary small/tall phone HUD and compact menu flows
now use this library. Compact recipes use five single-card spreads; tall phones
retain three two-card spreads. Measured enlarged pages share fonts and icons
without shrinking requested text. Bounded enlarged tactical strips now use the
same frames, icons and measured upright label bank. Native fixtures include
37screen/states,47/59point top safe areas and enlarged text through3.4x.
Final visual acceptance is tracked
in [ATOMIC_UI_IMPLEMENTATION.md](ATOMIC_UI_IMPLEMENTATION.md). The host-only
`human_resources_ui_review` renders37memory-backed screen/state fixtures without
loading a player save. It is excluded from the shipping package.
Text quads batch in groups of 128; static mesh/texture assets preload before play.
The renderer owns per-entity animation players, deformed bounds and GPU uploads. Runtime shaders are offline HLSL. Android
loads APK asset names through SDL_IO rather than filesystem probes.

The foundation package audit retains exact output inventories and additionally
checks the recipe's derived source hashes. Only the selected sources are
consumed; unused authoring models and development HUD screenshots
are excluded from this product content cook. Native art promotion is a
playable integration candidate, not a physical-device visual approval claim.

## Ownership and controls

The progression candidate starts with a 1.6-second sheep lift and adds +8 percentage
points of base abduction rate per earned level after level one. It uses existing
saved level state, adds to eligible research/Hangar/Winch gains, and retains the
biological minimum and two-second tank floor. The next promotion costs
`12 + 2 * current level` XP, including existing survival XP; Capture Speed choices
grant 12/18/25 biological rate points, capped at +100% research. Promotion and
Current Build display the level reward, with matching spoken values. Six hardware
slots, twelve live families and three ranks remain. Nine families, including Beam
Splitter, are available in random drafts from a fresh account. Heavy Winch, Arc
Coil and Phase Drive retain their achievement gates; the four retired families
and Mirrorball do not appear in offers, the library or recipes. Beam Splitter
is neither guaranteed nor preinstalled. Game codec6/authority16 and
its new numeric fingerprint use existing protected old-run banking; account2
progress is preserved. Existing delivered apps are separate binaries.

World travel starts at 2.4 m/s and gains 0.6 m/s gradually over the first 90 active
seconds, plus 0.01 m/s per capture and 0.12 m/s per level after the first, capped
at 4.6 m/s. The ramp uses existing saved active time/counters and freezes during
pause and upgrade choices. `runtime-spatial.json` owns this tuning; the offline
balance simulator remains separate. Changed tuning uses the existing protected
incompatible-run recovery rather than silently resuming an older ruleset.

`human_resources_product` owns presentation and the floating-stick contact
policy. It reads gameplay snapshots and account views; it does not mutate rules
or durable progress. `human_resources_client` dispatches commands to those
owners. The scene consumes monotonic runner distance separately from bounded
player dodging and fades foreground tree occluders near the saucer.

Drag an open part of the scene to fly with Midnight Menu’s anchored 48-point thumbstick; flick quickly in a direction to Dash without lifting. Another finger may press HUD controls
without stealing the movement contact. Keyboard WASD/arrows and controller left
stick map to the same semantic movement. Enter/controller south activates a
focused menu action; directional actions move focus. Escape/controller start
pauses. Space/controller east or the Dash button triggers the starter Dash;
Phase Drive hardware improves it. The base cooldown is four active seconds.
Dash travels as a short decelerating burst and also triggers an ordinary counter-volley when a valid target is available.
The ship centre can reach all four camera edges on phones, tablets and wide windows, leaving roughly half the saucer outside. The world fills the output and safe HUD controls overlay flight. Layout-derived world limits survive cold continuations and update after a device resize.
Flying close while moving accelerates capture; three qualified close sweeps
within five active seconds grant six seconds of Overdrive capture/fire cadence.
Multiple hull lasers fire as a quick sequential burst, with 70ms base spacing
and the existing whole-burst cadence. Confirmed hits briefly flash the struck
enemy/player assembly white, yellow and red; reduced motion suppresses the
material pulse while preserving the ordinary damage indicators.
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
