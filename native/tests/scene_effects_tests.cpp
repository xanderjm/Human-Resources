#include "../../tools/playable_qa/capture_cancel_trace.h"
#include "test_support.h"
#include <algorithm>
#include <cmath>
#include <human_resources/product/presentation.h>
#include <human_resources/product/scene_effects.h>
#include <human_resources/presentation/scenery.h>
#include <iostream>
namespace {
namespace hr = human_resources;
using test_support::require;
using test_support::require_near;
const hr::presentation::StyleInstance& mesh(const hr::presentation::StyleFrame& frame, std::string_view name) {
  const auto found = std::find_if(frame.instances.begin(), frame.instances.end(), [&](const auto& i) { return i.mesh == name; });
  require(found != frame.instances.end(), "required scene mesh exists");
  return *found;
}
hr::presentation::Vec3 transformed(const hr::presentation::StyleInstance& instance, hr::presentation::Vec3 p) {
  const auto m = hr::presentation::model_matrix(instance);
  return {m[0] * p.x + m[1] * p.y + m[2] * p.z + m[3], m[4] * p.x + m[5] * p.y + m[6] * p.z + m[7], m[8] * p.x + m[9] * p.y + m[10] * p.z + m[11]};
}
void windmill_uses_saved_active_time_and_keeps_its_assembly() {
  hr::gameplay::Snapshot run;
  const auto first = hr::product::scene(run, 390.F / 844.F, false);
  run.stats.active_seconds = 2;
  const auto later = hr::product::scene(run, 390.F / 844.F, false);
  require_near(mesh(first, "windmill-rotor").rotation.z, 0, .0001F,
      "windmill begins at the authored rotor pose");
  require_near(mesh(later, "windmill-rotor").rotation.z, .9F, .0001F,
      "windmill follows gentle active-time spindle rotation");
  run.phase = hr::gameplay::Phase::paused;
  const auto paused = hr::product::scene(run, 390.F / 844.F, false);
  const auto paused_again = hr::product::scene(run, 390.F / 844.F, false);
  require(hr::presentation::model_matrix(mesh(paused, "windmill-rotor")) ==
      hr::presentation::model_matrix(mesh(paused_again, "windmill-rotor")),
      "unchanged paused active time never advances a scenery rotor");
  const auto reduced = hr::product::scene(run, 390.F / 844.F, true);
  require(hr::presentation::model_matrix(mesh(reduced, "windmill-rotor")) ==
      hr::presentation::model_matrix(mesh(first, "windmill-rotor")),
      "reduced motion retains the authored static spindle pose");
  const auto scenery = hr::presentation::generate_scenery({hr::presentation::SceneryTheme::farmland,
      514, run.runner_distance, 390.F / 844.F, 9.7F});
  require(first.instances.size() == later.instances.size(), "rotor motion adds no instances");
  for (std::size_t index = 0; index < scenery.scene.instances.size(); ++index) {
    const auto& before = first.instances[index];
    const auto& after = later.instances[index];
    require(before.mesh == after.mesh && before.stable_id == after.stable_id,
        "scenery motion keeps source identities and ordering");
    if (before.mesh == "windmill-rotor") {
      require(before.position.x == after.position.x && before.position.y == after.position.y &&
          before.position.z == after.position.z && before.scale.x == after.scale.x &&
          before.scale.y == after.scale.y && before.scale.z == after.scale.z,
          "spinning blades stay attached at the same spindle with the same mesh scale");
    } else {
      require(hr::presentation::model_matrix(before) == hr::presentation::model_matrix(after),
          "all other authored scenery transforms including the mast stay unchanged");
    }
  }
}
void jeep_wheels_follow_signed_physical_travel_and_identity() {
  hr::gameplay::Snapshot run;
  run.phase = hr::gameplay::Phase::active;
  run.entities = {{.id=7,.kind=hr::gameplay::EntityKind::jeep,.position={0,10},.body_heading=0.F},
      {.id=9,.kind=hr::gameplay::EntityKind::jeep,.position={3,10},.body_heading=0.F}};
  hr::product::SceneEffects effects;
  effects.update(run);
  run.stats.active_seconds=.1;
  run.entities[0].position.z-=.15F;run.entities[0].velocity={0,-1.5F};
  run.entities[1].position.z+=.15F;run.entities[1].velocity={0,1.5F};
  effects.update(run);
  require_near(effects.wheel_angle(7),-.5F,1e-5F,"forward chassis travel rolls wheels forward");
  require_near(effects.wheel_angle(9),.5F,1e-5F,"reverse chassis travel reverses wheel roll without reversing facing");
  std::swap(run.entities[0],run.entities[1]);
  run.stats.active_seconds=.2;
  run.entities[0].body_heading=.2F;run.entities[0].velocity={};
  run.entities[1].body_heading=.2F;run.entities[1].velocity={};
  effects.update(run);
  require_near(effects.wheel_angle(7),-.5F,1e-5F,"stationary heading changes cannot invent forward wheel travel");
  require_near(effects.wheel_angle(9),.5F,1e-5F,"wheel history follows actor identity when public entity order changes");
  for(const auto phase:{hr::gameplay::Phase::paused,hr::gameplay::Phase::choosing_upgrade}) {
    run.phase=phase;run.entities[0].position.z+=1;
    effects.update(run);
    require_near(effects.wheel_angle(9),.5F,1e-5F,"unchanged active clock freezes wheels during pause, draft and interruption");
  }
  effects.reset();effects.update(run);
  require_near(effects.wheel_angle(7),0,0,"cold presentation reconstructs wheels without replaying prior travel");
}
void actual_convergence_and_rays() {
  hr::gameplay::Snapshot run;
  run.player = {2, 100};
  run.runner_distance = 100;
  run.velocity = {5, 6};
  run.entities.push_back({.id = 1, .kind = hr::gameplay::EntityKind::cow, .position = {-1, 103}});
  run.beams.push_back({.target_id = 1, .target = {-1, 103}, .progress = .5F});
  run.shots.push_back({.from = run.player, .to = {-1, 103}, .remaining_seconds = .1F});
  for (bool reduced : {false, true}) {
    float last_distance = 100;
    for (float progress : {0.F, .2F, .4F, .6F, .8F, .95F, 1.F}) {
      run.entities[0].capture_progress = progress;
      const auto frame = hr::product::scene(run, 390.F / 844.F, reduced);
      const auto& ship = mesh(frame, "ufo-modular");
      const auto& cow = mesh(frame, "cow");
      const auto tip = transformed(mesh(frame, "beam"), {0, 1, 0});
      const auto socket = transformed(ship, {0, .04F, 0});
      require_near(tip.x, socket.x, 1e-4F, "angled beam tip is attached to actual banked saucer socket X");
      require_near(tip.y, socket.y, 1e-4F, "angled beam tip is attached to actual banked saucer socket Y");
      require_near(tip.z, socket.z, 1e-4F, "angled beam tip is attached to actual banked saucer socket Z");
      const float distance = std::hypot(cow.position.x - tip.x, cow.position.z - tip.z);
      require(distance <= last_distance + .01F, "capture closes horizontal distance to intake smoothly");
      last_distance = distance;
      if (progress == 1) {
        require(distance < .001F, "capture finishes inside intake, not beside saucer");
        require(cow.scale.x <= .021F, "target smoothly shrinks through intake");
      }
      for (const auto& ray : frame.instances)
        if (ray.mesh == "cube" && ray.tint[0] == .55F) {
          const auto a = transformed(ray, {0, 0, -1}), b = transformed(ray, {0, 0, 1});
          const auto mount = transformed(ship, {-.36F, -.16F, .29F});
          require_near(std::hypot(std::hypot(a.x-mount.x,a.z-mount.z),a.y-mount.y), .65F*ship.scale.x, 1e-4F, "weapon ray exits actual aimed barrel, not ship centre");
          require_near(b.y, .65F, 1e-4F, "weapon ray ends at target height");
          require_near(b.z, -.7F, 1e-4F, "weapon ray uses forward world projection");
        }
    }
  }
}
void sweep_acquisition_shows_only_real_projected_participation() {
  hr::gameplay::Snapshot run;
  run.phase = hr::gameplay::Phase::active;
  run.entities.push_back({.id = 1, .kind = hr::gameplay::EntityKind::cow, .position = {1, 0}, .capture_progress = .4F});
  run.beams.push_back({.target_id = 1, .target = {1, 0}, .progress = .4F});
  const auto pointers = [](const hr::presentation::StyleFrame& frame) {
    std::vector<hr::presentation::StyleInstance> result;
    for (const auto& instance : frame.instances)
      if (instance.mesh == "cube" && (instance.tint == std::array<float, 4>{.70F, 1, .86F, .96F} ||
                                       instance.tint == std::array<float, 4>{.95F, 1, .57F, .98F})) result.push_back(instance);
    return result;
  };
  const auto ordinary = hr::product::scene(run, 390.F / 844.F, false);
  require(pointers(ordinary).empty(), "ordinary capture progress never claims accepted moving participation");
  run.beams[0].sweep_fraction = .5F;
  run.beams[0].sweep_active = true;
  const auto moving = hr::product::scene(run, 390.F / 844.F, false);
  const auto active = pointers(moving);
  require(active.size() == 8 && moving.instances.size() == ordinary.instances.size() + 8,
          "accepted participation adds one bounded eight-segment charge arc");
  for (const auto& pointer : active)
    require(pointer.effect && !pointer.casts_shadow && pointer.position.y > mesh(moving, "ufo-modular").position.y,
            "charge arc stays on the visible upper hull and adds no shadow work");
  run.stats.active_seconds = .1;
  const auto moving_later = pointers(hr::product::scene(run, 390.F / 844.F, false));
  require(hr::presentation::model_matrix(active[0]) != hr::presentation::model_matrix(moving_later[0]),
          "a contributing unfinished sweep gives a small live charge response");
  const auto reduced = pointers(hr::product::scene(run, 390.F / 844.F, true));
  run.stats.active_seconds = 2;
  const auto reduced_later = pointers(hr::product::scene(run, 390.F / 844.F, true));
  require(reduced.size() == 8 && hr::presentation::model_matrix(reduced[0]) ==
          hr::presentation::model_matrix(reduced_later[0]), "reduced motion keeps the actual partial sweep marker static");
  run.beams[0].sweep_fraction = 1;
  const auto qualified_frame = hr::product::scene(run, 390.F / 844.F, false);
  const auto qualified = pointers(qualified_frame);
  require(qualified.size() == 8 && qualified[0].tint[0] == .95F,
          "qualified contribution earns a distinct gold marker before collection");
  run.stats.active_seconds = 3;
  const auto qualified_later_frame = hr::product::scene(run, 390.F / 844.F, false);
  const auto qualified_later = pointers(qualified_later_frame);
  require_near(qualified[0].position.x, qualified_later[0].position.x, .00001F,
               "qualified arc stops pulsing along the hull");
  require_near(qualified[0].position.y - mesh(qualified_frame, "ufo-modular").position.y,
      qualified_later[0].position.y - mesh(qualified_later_frame, "ufo-modular").position.y, .00001F,
      "qualified arc remains attached while the ordinary hull floats");
  run.beams.resize(6, run.beams[0]);
  require(pointers(hr::product::scene(run, 390.F / 844.F, false)).size() == 8,
          "six contributing beams coalesce to one bounded arc instead of six gauges");
  run.beams.resize(1);
  run.harvest_overdrive_seconds = 1;
  require(pointers(hr::product::scene(run, 390.F / 844.F, false)).empty(),
          "active Overdrive cannot claim participation toward another chain");
  run.harvest_overdrive_seconds = 0;
  run.beams[0].vehicle = true;
  require(pointers(hr::product::scene(run, 390.F / 844.F, false)).empty(),
          "vehicle capture never claims a biological sweep");
}
void actual_damage_is_local_typed_and_pause_safe() {
  hr::product::SceneEffects effects;
  hr::gameplay::Snapshot run;
  effects.update(run);
  run.hull = 50;
  effects.update(run);
  require(effects.damage_feedback().age < 0, "state alone never invents player damage");
  run.stats.active_seconds = .1;
  run.presentation_events[0] = {1, .1, hr::gameplay::PresentationEventKind::shield_hit, 0, {}};
  effects.update(run);
  require(!effects.damage_feedback().hull && effects.damage_feedback().age == 0 && effects.damage_feedback().cause == hr::gameplay::DamageCause::unknown, "actual legacy shield event selects typed unknown feedback");
  auto frame = hr::product::scene(run, 390.F / 844.F, false, false, &effects);
  require(frame.damage_flash < 0 && mesh(frame, "ufo-modular").damage_reactive, "shield hit uses local cyan craft material response");
  const auto paused = hr::presentation::model_matrix(mesh(frame, "ufo-modular"));
  frame = hr::product::scene(run, 390.F / 844.F, false, false, &effects);
  require(hr::presentation::model_matrix(mesh(frame, "ufo-modular")) == paused && effects.damage_feedback().age == 0, "paused damage has no wall-clock motion");
  effects.reset();
  run.presentation_events[1] = {2, .1, hr::gameplay::PresentationEventKind::hull_hit, 7, {}, 0, hr::gameplay::DamageCause::projectile};
  effects.update(run);
  require(effects.damage_feedback().hull && effects.damage_feedback().cause == hr::gameplay::DamageCause::projectile, "hull takes precedence and retains actual cause for simultaneous layer hits");
  frame = hr::product::scene(run, 390.F / 844.F, false, false, &effects);
  require(frame.damage_flash > 0, "hull hit uses distinct coral craft material response");
  frame = hr::product::scene(run, 390.F / 844.F, true, false, &effects);
  require(frame.damage_flash == 0 && effects.damage_feedback().hull && effects.damage_feedback().cause == hr::gameplay::DamageCause::projectile, "reduced motion retains real layer and cause without flashing the craft");
  run.stats.active_seconds = .8;
  effects.update(run);
  require(effects.damage_feedback().age < 0, "damage feedback expires in bounded active time");
}
void history_requires_authority_and_freezes() {
  hr::product::SceneEffects effects;
  hr::gameplay::Snapshot run;
  run.hull = 100;
  run.shield = 20;
  run.entities.push_back({.id = 7, .kind = hr::gameplay::EntityKind::jeep, .hull = 10});
  effects.update(run);
  require(effects.beats().empty(), "initial state never invents effects");
  run.stats.active_seconds = .1;
  run.entities.clear();
  effects.update(run);
  require(effects.beats().empty(), "retirement is not an explosion or capture");
  run.entities.push_back({.id = 9, .kind = hr::gameplay::EntityKind::soldier, .hull = 10});
  run.stats.active_seconds = .2;
  effects.update(run);
  run.entities[0].hull = 8;
  run.stats.active_seconds = .3;
  run.presentation_events[0] = {1, .3, hr::gameplay::PresentationEventKind::impact, 9, {}};
  effects.update(run);
  require(effects.beats().size() == 1 && effects.beats()[0].kind == hr::product::SceneBeatKind::impact, "real hull reduction emits hit on stable ID");
  effects.update(run);
  require(effects.beats().size() == 1 && effects.beats()[0].age == 0, "paused identical snapshot neither repeats nor ages effect");
  run.entities.clear();
  run.stats.soldier_kills = 1;
  run.stats.captures = 1;
  run.shield = 15;
  run.stats.active_seconds = .4;
  run.presentation_events[1] = {2, .4, hr::gameplay::PresentationEventKind::defeat, 9, {}};
  run.presentation_events[2] = {3, .4, hr::gameplay::PresentationEventKind::capture, 10, {}};
  run.presentation_events[3] = {4, .4, hr::gameplay::PresentationEventKind::shield_hit, 0, {}};
  effects.update(run);
  require(effects.beats().size() == 4, "confirmed kill, capture and shield hit have separate beats");
  run.stats.active_seconds = 1.1;
  effects.update(run);
  require(effects.beats().empty(), "all effects expire in bounded active time");
  run.stats.active_seconds = 0;
  run.presentation_events = {};
  effects.update(run);
  require(effects.beats().empty(), "new run clock resets derived history");
  for (unsigned i = 0; i < 500; ++i) {
    run.stats.active_seconds += .0001;
    run.entities = {{.id = i + 100, .kind = hr::gameplay::EntityKind::jeep, .hull = 10}};
    effects.update(run);
    run.stats.active_seconds += .0001;
    run.entities[0].hull = 1;
    run.presentation_events[i % hr::gameplay::presentation_event_capacity] = {i + 1, run.stats.active_seconds, hr::gameplay::PresentationEventKind::impact, i + 100, {}};
    effects.update(run);
    require(effects.beats().size() <= hr::product::SceneEffects::beat_capacity, "unbounded session cannot exceed fixed effect storage");
  }
  effects.reset();
  require(effects.beats().empty(), "explicit restart clears effects");
}
void actual_game_events_match_confirmed_outcomes() {
  hr::gameplay::Game game;
  game.start({.seed = 514});
  std::uint64_t sequence = 0, captures = 0, defeats = 0, impacts = 0;
  for (unsigned tick = 0; tick < 7200 && game.snapshot().phase != hr::gameplay::Phase::dead; ++tick) {
    if (game.snapshot().phase == hr::gameplay::Phase::choosing_upgrade)
      require(game.choose_upgrade(0), "native test selects an earned upgrade");
    const auto& before = game.snapshot();
    hr::gameplay::Vec2 movement{};
    float nearest = 10000;
    for (const auto& actor : before.entities)
      if (actor.kind <= hr::gameplay::EntityKind::pig) {
        const float dx = actor.position.x - before.player.x, dz = actor.position.z - before.player.z;
        const float d = std::hypot(dx, dz);
        if (d < nearest) {
          nearest = d;
          movement = {dx, dz};
        }
      }
    game.advance(1.F / 60, {movement, false});
    const auto& run = game.snapshot();
    // Every frame contains the recent fixed ring. Count each exact sequence once.
    std::array<const hr::gameplay::PresentationEvent*, hr::gameplay::presentation_event_capacity> events{};
    std::size_t count = 0;
    for (const auto& event : run.presentation_events)
      if (event.sequence > sequence)
        events[count++] = &event;
    std::sort(events.begin(), events.begin() + static_cast<std::ptrdiff_t>(count), [](auto a, auto b) { return a->sequence < b->sequence; });
    for (std::size_t i = 0; i < count; ++i) {
      const auto& event = *events[i];
      require(event.sequence == sequence + 1, "presentation sequence is stable and ordered across wrap");
      require(event.active_seconds <= run.stats.active_seconds && event.active_seconds >= 0, "outcomes use real active time");
      require(std::isfinite(event.position.x) && std::isfinite(event.position.z), "event position stays finite");
      sequence = event.sequence;
      captures += event.kind == hr::gameplay::PresentationEventKind::capture;
      defeats += event.kind == hr::gameplay::PresentationEventKind::defeat;
      impacts += event.kind == hr::gameplay::PresentationEventKind::impact;
    }
  }
  const auto stats = game.snapshot().stats;
  require(captures > 0 && captures == stats.captures + stats.vehicle_captures, "every real biological/vehicle capture produces exactly one outcome");
  require(defeats > 0 && defeats == stats.soldier_kills + stats.jeep_kills + stats.tank_kills + stats.boss_kills, "real military deaths produce exactly one defeat outcome");
  require(impacts > 0, "real nonfatal weapon damage emits impact feedback");
  game.pause();
  game.advance(.1F);
  std::uint64_t latest = 0;
  for (const auto& event : game.snapshot().presentation_events)
    latest = std::max(latest, event.sequence);
  require(latest == sequence, "pause cannot invent or replay authoritative outcomes");
  game.start();
  for (const auto& event : game.snapshot().presentation_events)
    require(event.sequence == 0, "restart clears entire event ring");
}
void semantic_feedback_never_replays_persistent_flashes() {
  for (float step : {1.F / 60, 1.F / 120, .1F}) {
    hr::product::SceneEffects effects;
    hr::gameplay::Snapshot run;
    run.stats.active_seconds = 1;
    effects.update(run);
    run.stats.active_seconds = 1.1;
    run.shots.push_back({.remaining_seconds = .12F});
    run.presentation_events[0] = {1, 1.1, hr::gameplay::PresentationEventKind::laser_volley, 0, {}};
    run.presentation_events[1] = {2, 1.1, hr::gameplay::PresentationEventKind::warning_lock, 7, {}};
    effects.update(run);
    require(effects.feedback().laser && effects.feedback().warning, "volley and committed warning lock sound immediately, even at100ms frames");
    require(effects.beats().empty(), "audio-only events do not create imaginary impact effects");
    unsigned volleys = 1, warnings = 1;
    for (unsigned frame = 0; frame < 20; ++frame) {
      run.stats.active_seconds += step;
      effects.update(run);
      volleys += effects.feedback().laser;
      warnings += effects.feedback().warning;
    }
    require(volleys == 1 && warnings == 1, "cached120ms laser and warning snapshots cannot repeat cues at60/120Hz or100ms frames");
    effects.update(run);
    require(!effects.feedback().laser && !effects.feedback().warning, "paused repeated frame has no audio callbacks");
  }
}
void skill_feedback_consumes_real_events_once() {
  hr::product::SceneEffects effects;
  hr::gameplay::Snapshot run;
  run.stats.active_seconds = 1;
  effects.update(run);
  run.stats.active_seconds = 1.1;
  using Kind = hr::gameplay::PresentationEventKind;
  run.presentation_events[0] = {1, 1.1, Kind::capture, 1, {}};
  run.presentation_events[1] = {2, 1.1, Kind::sweep_capture, 1, {}};
  run.presentation_events[2] = {3, 1.1, Kind::harvest_overdrive, 0, {}};
  run.presentation_events[3] = {4, 1.1, Kind::phase_dash, 0, {}};
  effects.update(run);
  auto feedback = effects.feedback();
  require(feedback.capture && feedback.sweep_capture && feedback.harvest_overdrive && feedback.phase_dash,
      "all real skill outcomes reach audio without removing their visual beats");
  require(effects.beats().size() == 4, "audio mapping preserves all four visual beats");
  effects.update(run);
  feedback = effects.feedback();
  require(!feedback.capture && !feedback.sweep_capture && !feedback.harvest_overdrive && !feedback.phase_dash,
      "paused frame cannot replay skill audio");
  run.stats.active_seconds += .02;
  effects.update(run);
  require(!effects.feedback().harvest_overdrive && !effects.feedback().phase_dash,
      "advancing with cached events cannot repeat activation sounds");
  effects.reset(); // The client uses the same reset on a new or restored run.
  run.stats.active_seconds = .1;
  run.presentation_events = {};
  run.presentation_events[0] = {1, .1, Kind::phase_dash, 0, {}};
  effects.update(run);
  require(effects.feedback().phase_dash, "fresh run sequence one is not hidden behind a prior run cursor");
}
struct CaptureCues final {
  std::vector<const hr::presentation::StyleInstance*> cores, backings, cones, intakes;
};
CaptureCues capture_cues(const hr::presentation::StyleFrame& frame) {
  CaptureCues result;
  for (const auto& instance : frame.instances) {
    if (!instance.effect) continue;
    if (instance.mesh == "cube" && instance.tint == std::array<float, 4>{.46F, 1, .75F, .90F}) result.cores.push_back(&instance);
    if (instance.mesh == "cube" && instance.tint == std::array<float, 4>{.025F, .07F, .11F, .42F}) result.backings.push_back(&instance);
    if (instance.mesh == "beam") result.cones.push_back(&instance);
    if (instance.mesh == "ring" && instance.tint == std::array<float, 4>{.48F, 1, .76F, .7F}) result.intakes.push_back(&instance);
  }
  return result;
}
void require_paired_capture_paths(const CaptureCues& cues) {
  require(!cues.cores.empty() && cues.cores.size() == cues.backings.size() &&
      cues.cores.size() + cues.backings.size() <= 72, "active capture paths have one backing per core within the shared72-segment ceiling");
  for (std::size_t i = 0; i < cues.cores.size(); ++i) {
    const auto& core = *cues.cores[i];
    const auto& backing = *cues.backings[i];
    require(core.mesh == backing.mesh && !core.casts_shadow && !backing.casts_shadow,
        "capture path contrast adds no shadow work");
    require(core.position.x == backing.position.x && core.position.y == backing.position.y && core.position.z == backing.position.z &&
        core.rotation.x == backing.rotation.x && core.rotation.y == backing.rotation.y && core.rotation.z == backing.rotation.z,
        "each mint path and its quiet backing follow the same target-to-intake segment");
    require(core.scale.x > 0 && core.scale.y > 0 && core.scale.z > 0 &&
        backing.scale.x > core.scale.x && backing.scale.y > core.scale.y && backing.scale.z == core.scale.z,
        "backing reveals the positive-width mint path without extending its direction or length");
  }
}
void capture_paths_are_paired_bounded_and_reduced_motion_is_static() {
  hr::gameplay::Snapshot run;
  for (unsigned i = 0; i < 6; ++i)
    run.beams.push_back({.target_id = i + 1, .target = {static_cast<float>(i) - 3, 1}, .progress = .5F});
  const auto first = hr::product::scene(run, 390.F / 844.F, true);
  run.stats.active_seconds = 10;
  const auto second = hr::product::scene(run, 390.F / 844.F, true);
  const auto before = capture_cues(first), after = capture_cues(second);
  require(before.cores.size() == 36 && before.backings.size() == 36,
      "six captures show three paired dashes each with one contrast backing");
  require_paired_capture_paths(before);
  require(before.cores.size() == after.cores.size() && before.backings.size() == after.backings.size(),
      "reduced-motion capture path counts remain static");
  for (std::size_t i = 0; i < before.cores.size(); ++i) {
    require(hr::presentation::model_matrix(*before.cores[i]) == hr::presentation::model_matrix(*after.cores[i]) &&
        hr::presentation::model_matrix(*before.backings[i]) == hr::presentation::model_matrix(*after.backings[i]),
        "reduced motion freezes complete core/backing transforms including width and orientation");
  }
  for (double seconds : {0., .137, .413, 4.2}) {
    run.stats.active_seconds = seconds;
    const auto moving = hr::product::scene(run, 390.F / 844.F, false);
    require_paired_capture_paths(capture_cues(moving));
  }
}
void capture_intake_and_soft_opacity_are_shared() {
  hr::gameplay::Snapshot run;
  const auto empty = hr::product::scene(run, 390.F / 844.F, true);
  const auto empty_cues = capture_cues(empty);
  require(empty_cues.intakes.empty() && empty_cues.cones.empty() && empty_cues.cores.empty() && empty_cues.backings.empty(),
      "no active capture invents an acquisition ring, path or soft volume");
  const auto opacity = [](const CaptureCues& cues) {
    float total = 0;
    for (const auto* cone : cues.cones) {
      require(cone->tint[3] > 0 && !cone->casts_shadow, "capture volumes retain positive soft opacity without shadows");
      total += cone->tint[3];
    }
    return total;
  };
  float ordinary_opacity = 0;
  for (bool overdrive : {false, true}) {
    run.harvest_overdrive_seconds = overdrive ? 1.F : 0;
    run.beams = {{.target_id = 1, .target = {-3, 1}, .progress = .5F}};
    const auto single = hr::product::scene(run, 390.F / 844.F, true);
    const auto one = capture_cues(single);
    for (unsigned i = 1; i < 6; ++i)
      run.beams.push_back({.target_id = i + 1, .target = {static_cast<float>(i) - 3, 1}, .progress = .5F});
    const auto multiple = hr::product::scene(run, 390.F / 844.F, true);
    const auto six = capture_cues(multiple);
    require(one.intakes.size() == 1 && six.intakes.size() == 1 && one.cones.size() == 1 && six.cones.size() == 6,
        "one real intake responds once while every target keeps its own soft path");
    require(hr::presentation::model_matrix(*one.intakes[0]) == hr::presentation::model_matrix(*six.intakes[0]),
        "equal real progress produces the same intake response with one or six captures");
    require_near(opacity(one), opacity(six), 1e-6F, "six captures share the same aggregate soft-opacity budget as one");
    if (overdrive) require(opacity(six) > ordinary_opacity, "actual Overdrive retains its stronger shared soft response");
    else ordinary_opacity = opacity(six);
    run.beams.back().progress = .9F;
    const auto advanced = hr::product::scene(run, 390.F / 844.F, true);
    const auto leading = capture_cues(advanced);
    require(leading.intakes.size() == 1 && leading.intakes[0]->scale.x < six.intakes[0]->scale.x,
        "the shared intake contracts toward a genuinely advancing specimen rather than duplicating rings");
  }
}
void flight_attitude_and_capture_payoff() {
  hr::gameplay::Vec2 reference{};
  for (unsigned frequency : {60U, 120U}) {
    hr::product::SceneEffects effects;
    hr::gameplay::Snapshot run;
    effects.update(run);
    run.velocity = {6, 4};
    for (unsigned i = 1; i <= frequency / 2; ++i) {
      run.stats.active_seconds = static_cast<double>(i) / frequency;
      effects.update(run);
      require(effects.flight_velocity().x > 0 && effects.flight_velocity().x <= 6,
          "flight lean approaches actual motion without overshoot");
    }
    const auto velocity = effects.flight_velocity();
    require(velocity.x > 5.9F && velocity.z > 3.9F, "lean settles promptly, without lagging behind flight");
    if (frequency == 60)
      reference = velocity;
    else {
      require_near(velocity.x, reference.x, 1e-4F, "60/120Hz produce the same active-time bank");
      require_near(velocity.z, reference.z, 1e-4F, "60/120Hz produce the same active-time pitch");
    }
    run.velocity = {-6, -4};
    effects.update(run);
    require(effects.flight_velocity().x == velocity.x, "paused input changes cannot snap flight attitude");
    run.stats.active_seconds += 1.0 / frequency;
    effects.update(run);
    require(effects.flight_velocity().x > 0 && effects.flight_velocity().x < velocity.x,
        "direction reversal eases through the current attitude instead of snapping");
    run.stats.active_seconds = 0;
    run.velocity = {};
    effects.update(run);
    require(effects.flight_velocity().x == 0 && effects.flight_velocity().z == 0,
        "new run discards the previous ship's lean");
  }
  hr::product::SceneEffects effects;
  hr::gameplay::Snapshot run;
  run.stats.active_seconds = 1;
  effects.update(run);
  require(effects.capture_response() == 0, "idle craft cannot fabricate successful capture response");
  run.stats.active_seconds = 1.05;
  for (unsigned i = 0; i < 6; ++i)
    run.presentation_events[i] = {i + 1, 1, hr::gameplay::PresentationEventKind::capture, i + 1, {}};
  effects.update(run);
  require(effects.capture_response() > .5F && effects.capture_response() <= 1,
      "six real captures share one restrained hull response");
  const auto frame = hr::product::scene(run, 390.F / 844.F, false, false, &effects);
  require(std::count_if(frame.instances.begin(), frame.instances.end(), [](const auto& part) {
    return part.mesh == "ring" && part.tint[0] == .48F;
  }) == 1,
      "simultaneous capture success emits one readable silhouette collar");
  const auto& craft = mesh(frame, "ufo-modular");
  require(craft.polished_hull && craft.position.y > 2.35F && craft.position.y < 2.6F,
      "material response and small capture compression retain the actual ship root");
  const auto still = hr::product::scene(run, 390.F / 844.F, true, false, &effects);
  const auto& reduced = mesh(still, "ufo-modular");
  require(reduced.position.y == 2.5F && reduced.rotation.x == 0 && reduced.rotation.z == 0,
      "reduced motion removes hover, lean and capture hull compression");
  const auto paused_response = effects.capture_response();
  effects.update(run);
  require(effects.capture_response() == paused_response, "pause freezes the capture return envelope");
  run.stats.active_seconds = 2;
  effects.update(run);
  require(effects.capture_response() == 0, "successful intake response retires in bounded active time");
}
void capture_loss_continuity_and_lifecycle() {
  constexpr float aspect=390.F/844.F;
  for(float progress:{.05F,.9F,.99F}) {
    hr::product::SceneEffects effects;
    hr::gameplay::Snapshot run;
    run.stats.active_seconds=3;
    run.entities.push_back({.id=42,.kind=hr::gameplay::EntityKind::soldier,.position={3,1},
        .capture_progress=progress,.infantry_role=hr::gameplay::InfantryRole::rifleman});
    const auto before=mesh(hr::product::scene(run,aspect,false,false,&effects),"infantry-rifleman-body");
    run.entities[0].capture_progress=0;
    run.stats.active_seconds+=1.0/60;
    run.runner_distance+=.1F;
    const auto cancelled=mesh(hr::product::scene(run,aspect,false,false,&effects),"infantry-rifleman-body");
    require_near(cancelled.position.x,before.position.x,1e-6F,"real progress reset preserves last displayed root X");
    require_near(cancelled.position.y,before.position.y,1e-6F,"real progress reset preserves last displayed root height");
    require_near(cancelled.position.z,before.position.z+.1F,1e-6F,"retained root follows runner origin without teleporting in gameplay world");
    require(cancelled.scale.x==before.scale.x && cancelled.rotation.x==before.rotation.x && cancelled.rotation.z==before.rotation.z,
        "cancel preserves displayed size and tilt");
    const auto paused=mesh(hr::product::scene(run,aspect,false,false,&effects),"infantry-rifleman-body");
    require(hr::presentation::model_matrix(paused)==hr::presentation::model_matrix(cancelled),"same active time freezes release including scale/tilt");
    for(unsigned frame=0;frame<90;++frame) {
      run.stats.active_seconds+=1.0/60;
      run.entities[0].position.x+=.01F;
      run.runner_distance+=.02F;
      const auto moving=hr::product::scene(run,aspect,false,false,&effects);
      require(std::isfinite(mesh(moving,"infantry-rifleman-body").position.y),"moving ground root remains finite throughout source release");
      const auto& gun=mesh(moving,"weapon-carbine");
      require(gun.attachment_parent_id==42,"release uses one body root for the actual weapon socket");
    }
    const auto landed=mesh(hr::product::scene(run,aspect,false,false,&effects),"infantry-rifleman-body");
    const auto authority=mesh(hr::product::scene(run,aspect,false),"infantry-rifleman-body");
    require_near(landed.position.x,authority.position.x,1e-6F,"release converges on the current moving ground root");
    require_near(landed.position.y,0,1e-6F,"authored release ends grounded");
    require(landed.scale.x==1 && landed.rotation.x==0 && landed.rotation.z==0,"release completes regrowth and removes capture tilt");
    // Reacquire during a second release; its first root is the current pose.
    run.entities[0].capture_progress=.9F; run.stats.active_seconds+=.2;
    (void)hr::product::scene(run,aspect,false,false,&effects);
    run.entities[0].capture_progress=0; run.stats.active_seconds+=.02;
    (void)hr::product::scene(run,aspect,false,false,&effects);
    run.stats.active_seconds+=.2;
    const auto falling=mesh(hr::product::scene(run,aspect,false,false,&effects),"infantry-rifleman-body");
    run.entities[0].capture_progress=.01F; run.stats.active_seconds+=.02;
    const auto acquired=mesh(hr::product::scene(run,aspect,false,false,&effects),"infantry-rifleman-body");
    require(hr::presentation::model_matrix(falling)==hr::presentation::model_matrix(acquired),"reacquisition starts at the currently displayed pose");
    effects.reset();
    const auto restored=mesh(hr::product::scene(run,aspect,false,false,&effects),"infantry-rifleman-body");
    const auto fresh=mesh(hr::product::scene(run,aspect,false),"infantry-rifleman-body");
    require(hr::presentation::model_matrix(restored)==hr::presentation::model_matrix(fresh),"new/cold restore has no stale release history");
  }
  hr::product::SceneEffects effects;
  hr::gameplay::Snapshot run;
  for(unsigned i=1;i<=128;++i) run.entities.push_back({.id=i,.kind=hr::gameplay::EntityKind::cow,.capture_progress=.9F});
  effects.begin_capture_frame(run);
  const hr::product::CapturePose raised{{3,2,1},{.3F,.5F,.2F},.05F},ground{{0,0,0},{},1};
  for(const auto& actor:run.entities) (void)effects.capture_pose(actor,raised,ground,1,0);
  for(auto& actor:run.entities) {actor.id+=128;actor.capture_progress=0;}
  run.stats.active_seconds=2;effects.begin_capture_frame(run);
  for(const auto& actor:run.entities) require(effects.capture_pose(actor,ground,ground,2,0).position.y==0,"all128 old IDs retire before128 replacements");
  run.entities[0].capture_progress=.9F; run.stats.active_seconds=3;effects.begin_capture_frame(run);
  (void)effects.capture_pose(run.entities[0],raised,ground,3,0);
  run.entities[0].kind=hr::gameplay::EntityKind::farmer;run.entities[0].capture_progress=0;run.stats.active_seconds=4;effects.begin_capture_frame(run);
  require(effects.capture_pose(run.entities[0],ground,ground,4,0).position.y==0,"retyped ID cannot inherit capture history");
}
void real_game_range_loss_is_continuous() {
  const auto trace=hrqa::actual_capture_cancel_trace();
  require(trace.cancelled_at>0 && trace.samples[trace.cancelled_at-1].actor.capture_progress>.9F &&
      trace.samples[trace.cancelled_at].actor.capture_progress==0 && !trace.samples[trace.cancelled_at].beam,
      "public Game retains immediate range-loss reset and beam removal");
  hr::product::SceneEffects effects;
  hr::presentation::StyleInstance before;
  const auto& source = trace.samples.front().actor;
  const std::string_view source_mesh = source.kind == hr::gameplay::EntityKind::cow ? "cow" : source.kind == hr::gameplay::EntityKind::sheep               ? "sheep"
                                                                                          : source.kind == hr::gameplay::EntityKind::pig                   ? "pig"
                                                                                          : source.kind == hr::gameplay::EntityKind::farmer                ? "farmer-body"
                                                                                          : source.infantry_role == hr::gameplay::InfantryRole::shotgunner ? "infantry-shotgunner-body"
                                                                                          : source.infantry_role == hr::gameplay::InfantryRole::rocketeer  ? "infantry-rocketeer-body"
                                                                                                                                                           : "infantry-rifleman-body";
  for(std::size_t i=0;i<trace.samples.size();++i) {
    const auto& sample=trace.samples[i];
    hr::gameplay::Snapshot run;
    run.stats.active_seconds=sample.seconds;run.runner_distance=sample.runner_distance;
    run.player=sample.player;run.entities.push_back(sample.actor);
    const auto frame=hr::product::scene(run,390.F/844.F,false,false,&effects);
    const auto after = mesh(frame, source_mesh);
    if(i==trace.cancelled_at) {
      require_near(after.position.x, before.position.x, 1e-6F, "actual range-loss frame preserves displayed X");
      require_near(after.position.y, before.position.y, 1e-6F, "actual range-loss frame preserves displayed height");
      require_near(after.position.z, before.position.z + sample.runner_distance - trace.samples[i - 1].runner_distance, 1e-6F, "actual reset preserves actor world station");
      require(after.scale.x == before.scale.x, "actual reset cannot instantaneously regrow the actor");
    }
    before=after;
  }
  require(before.scale.x==1 && before.position.y==0,"actual trace reaches current authoritative ground pose after release");
}
void confirmed_fast_capture_keeps_one_readable_source_body() {
  for (bool reduced : {false, true}) {
    hr::gameplay::Game game;
    game.start({.seed = 514});
    hr::product::SceneEffects effects;
    auto previous = hr::product::scene(game.snapshot(), 390.F / 844.F, reduced, false, &effects);
    std::uint32_t captured{};
    for (unsigned frame = 0; frame < 600 && !captured; ++frame) {
      const auto& before = game.snapshot();
      hr::gameplay::Vec2 movement{};
      float nearest = 10000;
      for (const auto& actor : before.entities)
        if (actor.kind < hr::gameplay::EntityKind::soldier) {
          const float dx = actor.position.x - before.player.x, dz = actor.position.z - before.player.z;
          const float distance = std::hypot(dx, dz);
          if (distance < nearest) {
            nearest = distance;
            movement = {dx, dz};
          }
        }
      if (before.phase == hr::gameplay::Phase::choosing_upgrade)
        (void)game.choose_upgrade(0);
      game.advance(1.F / 60, {movement, false});
      effects.update(game.snapshot());
      const auto current = hr::product::scene(game.snapshot(), 390.F / 844.F, reduced, false, &effects);
      if (!effects.completed_captures().empty()) {
        captured = effects.completed_captures().front().id;
        require(game.snapshot().stats.captures > 0, "authority awards collection before its short visible intake finishes");
        require(std::none_of(game.snapshot().entities.begin(), game.snapshot().entities.end(), [&](const auto& actor) { return actor.id == captured; }),
            "visible finisher is not a second live gameplay actor");
        const auto body = std::find_if(current.instances.begin(), current.instances.end(), [&](const auto& part) { return part.stable_id == captured; });
        const auto old = std::find_if(previous.instances.begin(), previous.instances.end(), [&](const auto& part) { return part.stable_id == captured; });
        require(body != current.instances.end() && old != previous.instances.end() && body->mesh == old->mesh,
            "real collection retains exactly its actual source species and skin identity");
        require(std::count_if(current.instances.begin(), current.instances.end(), [&](const auto& part) { return part.stable_id == captured; }) == 1,
            "removal frame cannot draw both live and retained copies");
        const float first_step = std::hypot(std::hypot(body->position.x - old->position.x, body->position.z - old->position.z), body->position.y - old->position.y);
        std::cout << "CAPTURE_FINISH reduced=" << reduced << " source=" << body->mesh << " first_step=" << first_step << " old=" << old->position.x << "," << old->position.y << "," << old->position.z << " new=" << body->position.x << "," << body->position.y << "," << body->position.z << '\n';
        require(first_step < .65F,
            "confirmed intake begins at the last displayed pose without teleporting the specimen");
        const auto held = hr::presentation::model_matrix(*body);
        game.pause();
        effects.update(game.snapshot());
        const auto paused = hr::product::scene(game.snapshot(), 390.F / 844.F, reduced, false, &effects);
        const auto frozen = std::find_if(paused.instances.begin(), paused.instances.end(), [&](const auto& part) { return part.stable_id == captured; });
        require(frozen != paused.instances.end() && hr::presentation::model_matrix(*frozen) == held, "pause freezes the entire retained capture pose");
        game.resume();
      }
      previous = current;
    }
    require(captured != 0, "public moving pursuit exercises a real fast capture finisher");
    for (unsigned frame = 0; frame < 30; ++frame) {
      game.advance(1.F / 60);
      effects.update(game.snapshot());
      (void)hr::product::scene(game.snapshot(), 390.F / 844.F, reduced, false, &effects);
    }
    require(std::none_of(effects.completed_captures().begin(), effects.completed_captures().end(), [&](const auto& actor) { return actor.id == captured; }),
        "confirmed finisher retires within half an active second");
    effects.reset();
    require(effects.completed_captures().empty(), "new run/restore clears every retained source body");
  }
}
void only_confirmed_collection_can_retain_a_source_body() {
  hr::gameplay::Snapshot run;
  hr::product::SceneEffects effects;
  run.stats.active_seconds = 1;
  run.entities.push_back({.id = 42, .kind = hr::gameplay::EntityKind::cow, .capture_progress = .2F});
  effects.update(run);
  (void)hr::product::scene(run, 390.F / 844.F, false, false, &effects);
  run.entities.clear();
  run.stats.active_seconds = 1.05;
  run.presentation_events[0] = {1, 1.05, hr::gameplay::PresentationEventKind::defeat, 42, {}};
  effects.update(run);
  (void)hr::product::scene(run, 390.F / 844.F, false, false, &effects);
  require(effects.completed_captures().empty(), "kill or ordinary retirement never invents a collected body");
  run.entities.push_back({.id = 43, .kind = hr::gameplay::EntityKind::pig, .capture_progress = .2F});
  run.stats.active_seconds = 1.1;
  effects.update(run);
  (void)hr::product::scene(run, 390.F / 844.F, false, false, &effects);
  run.entities.clear();
  run.stats.active_seconds = 1.15;
  run.presentation_events[1] = {2, 1.15, hr::gameplay::PresentationEventKind::capture, 43, {}};
  effects.update(run);
  (void)hr::product::scene(run, 390.F / 844.F, false, false, &effects);
  require(effects.completed_captures().size() == 1, "actual matching collection permits one bounded source finisher");
  run.entities.push_back({.id = 43, .kind = hr::gameplay::EntityKind::farmer});
  run.stats.active_seconds = 1.2;
  effects.update(run);
  const auto replaced = hr::product::scene(run, 390.F / 844.F, false, false, &effects);
  require(effects.completed_captures().empty() && mesh(replaced, "farmer-body").stable_id == 43,
      "retyped ID discards the previous source body before admitting its replacement");
  effects.reset();
  effects.update(run);
  (void)hr::product::scene(run, 390.F / 844.F, false, false, &effects);
  require(effects.completed_captures().empty(), "restore cannot reconstruct old collections from the event ring alone");
}
} // namespace
int main() {
  jeep_wheels_follow_signed_physical_travel_and_identity();
  windmill_uses_saved_active_time_and_keeps_its_assembly();
  actual_damage_is_local_typed_and_pause_safe();
  actual_convergence_and_rays();
  sweep_acquisition_shows_only_real_projected_participation();
  history_requires_authority_and_freezes();
  actual_game_events_match_confirmed_outcomes();
  semantic_feedback_never_replays_persistent_flashes();
  skill_feedback_consumes_real_events_once();
  capture_paths_are_paired_bounded_and_reduced_motion_is_static();
  capture_intake_and_soft_opacity_are_shared();
  flight_attitude_and_capture_payoff();
  capture_loss_continuity_and_lifecycle();
  real_game_range_loss_is_continuous();
  confirmed_fast_capture_keeps_one_readable_source_body();
  only_confirmed_collection_can_retain_a_source_body();
}
