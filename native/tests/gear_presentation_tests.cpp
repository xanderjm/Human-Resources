#include "test_support.h"
#include <algorithm>
#include <cmath>
#include <human_resources/product/gear_presentation.h>
#include <human_resources/product/presentation.h>
#include <human_resources/product/scene_effects.h>
namespace {
namespace hr = human_resources;
using test_support::require;
using test_support::require_near;
unsigned count(const hr::presentation::StyleFrame& f, std::string_view mesh) {
  return static_cast<unsigned>(std::count_if(f.instances.begin(), f.instances.end(), [&](const auto& i) { return i.mesh == mesh; }));
}
void equipment_matches_installed_state() {
  hr::gameplay::Snapshot run;
  run.laser_count = 1;
  run.beam_capacity = 1;
  run.shield = 20;
  run.maximum_shield = 20;
  hr::presentation::StyleFrame f;
  const hr::product::ShipPose pose{{2, 2.5F, 3}, 1.12F, .18F};
  hr::product::append_ship_gear(f, run, pose, false);
  require(count(f, "ufo-antenna") == 1 && count(f, "ufo-shield") == 0, "starter has one antenna, no shield hardware");
  require(count(f, "ufo-laser") == 1 && count(f, "ufo-beam") == 1, "starter has its actual single laser and beam");
  require(count(f, "ufo-modular") == 0 && count(f, "shield-shell") == 0, "starter does not pretend to have orbit drones or upgraded shield");
  const auto& antenna = f.instances.front();
  require_near(antenna.position.x, 2 - 1.12F * 1.012F * std::sin(.18F), 1e-5F, "top socket follows banked fixed hull");
  require_near(antenna.position.y, 2.5F + 1.12F * 1.012F * std::cos(.18F), 1e-5F, "top socket retains source height");
  run.modules[static_cast<unsigned>(hr::gameplay::Module::shield_generator)] = 3;
  run.modules[static_cast<unsigned>(hr::gameplay::Module::orbit_drones)] = 3;
  run.modules[static_cast<unsigned>(hr::gameplay::Module::laser_array)] = 3;
  run.laser_count = 4;
  f.instances.clear();
  hr::product::append_ship_gear(f, run, pose, false);
  require(count(f, "ufo-antenna") == 0 && count(f, "ufo-shield") == 1, "shield replaces antenna at the same socket");
  require(count(f, "ufo-modular") == 3 && count(f, "ufo-laser") == 7, "four hull lasers plus exactly three armed orbit drones");
  require(count(f, "shield-shell") == 1, "charged shield generator has one rim sphere");
  for (const auto& i : f.instances)
    if (i.mesh == "shield-shell")
      require(i.effect && i.shield_rim && !i.casts_shadow, "shield uses existing transparent depth-tested rim contract");
  run.shield = 0;
  f.instances.clear();
  hr::product::append_ship_gear(f, run, pose, false);
  require(count(f, "shield-shell") == 0, "depleted shield has no false active field");
}
void explicit_events_and_pause() {
  hr::gameplay::Snapshot run;
  run.stats.active_seconds = 1;
  run.modules[static_cast<unsigned>(hr::gameplay::Module::emp_ring)] = 3;
  hr::product::SceneEffects effects;
  effects.update(run);
  hr::presentation::StyleFrame f;
  const hr::product::ShipPose pose{{0, 2.5F, 2.3F}, 1.12F, 0};
  hr::product::append_gear_effects(f, run, &effects, pose, 0, false);
  require(f.instances.empty(), "installed EMP never fabricates a pulse from cosmetic time");
  run.stats.active_seconds = 1.1;
  run.presentation_events[0] = {1, 1.1, hr::gameplay::PresentationEventKind::emp_pulse, 0, {}, 7};
  effects.update(run);
  hr::product::append_gear_effects(f, run, &effects, pose, 0, false);
  require(count(f, "ring") == 1, "one actual pulse emits one expanding ring");
  effects.update(run);
  require(effects.beats().size() == 1 && effects.beats()[0].radius == 7 && effects.beats()[0].age == 0, "paused snapshot preserves real radius and cannot repeat or age pulse");
  run.stats.active_seconds = 1.4;
  effects.update(run);
  f.instances.clear();
  hr::product::append_gear_effects(f, run, &effects, pose, 0, false);
  require_near(f.instances.front().scale.x, 7, 1e-4F, "pulse reaches exact authoritative radius");
  run.stats.active_seconds = 2;
  effects.update(run);
  f.instances.clear();
  hr::product::append_gear_effects(f, run, &effects, pose, 0, false);
  require(f.instances.empty(), "old pulse expires boundedly");
  effects.reset();
  require(effects.beats().empty(), "restart clears gear history");
}
void typed_shots_and_reduced_motion() {
  hr::gameplay::Snapshot run;
  run.laser_count = 4;
  run.modules[static_cast<unsigned>(hr::gameplay::Module::orbit_drones)] = 3;
  run.shots.push_back({{}, {2, 8}, .1F, true, hr::gameplay::ShotKind::emp});
  hr::presentation::StyleFrame f;
  const hr::product::ShipPose pose{{0, 2.5F, 2.3F}, 1.12F, 0};
  hr::product::append_gear_shots(f, run, pose, 0, true);
  require(f.instances.empty(), "EMP does not masquerade as laser spokes");
  run.shots[0].kind = hr::gameplay::ShotKind::drone;
  run.shots[0].emitter_index = 2;
  hr::product::append_gear_shots(f, run, pose, 0, true);
  const auto first = f.instances;
  run.stats.active_seconds = 12345;
  f.instances.clear();
  hr::product::append_gear_shots(f, run, pose, 0, true);
  require(first.size() == 2 && f.instances.size() == 2, "drone has one bounded halo and one core");
  require_near(first[0].position.x, f.instances[0].position.x, 1e-6F, "reduced motion drone shot follows static drone X");
  require_near(first[0].position.z, f.instances[0].position.z, 1e-6F, "reduced motion drone shot follows static drone Z");
  run.shots.clear();
  for (unsigned i = 0; i < 64; ++i)
    run.shots.push_back({{}, {2, 8}, .1F, false, hr::gameplay::ShotKind::arc});
  f.instances.clear();
  hr::product::append_gear_shots(f, run, pose, 0, false);
  require(f.instances.size() == 128, "maximum secondary shots have a bounded two-segment arc representation");
}
void real_game_exposes_committed_module_activity() {
  hr::gameplay::Game game;
  game.start({.seed = 17});
  std::uint64_t last = 0;
  bool saw_drone = false, saw_pulse = false;
  for (unsigned frame = 1; frame < 1900 && game.snapshot().phase != hr::gameplay::Phase::dead; ++frame) {
    if (game.snapshot().phase == hr::gameplay::Phase::choosing_upgrade)
      (void)game.choose_upgrade(0);
    game.advance(.1F, {{.8F * std::sin(static_cast<float>(frame) * .07F), 0}, false});
    const auto& run = game.snapshot();
    for (const auto& shot : run.shots) {
      if (shot.kind == hr::gameplay::ShotKind::drone) {
        require(shot.emitter_index < run.modules[static_cast<unsigned>(hr::gameplay::Module::orbit_drones)], "real drone hit identifies an installed drone");
        saw_drone = true;
      }
      if (shot.kind == hr::gameplay::ShotKind::laser)
        require(shot.emitter_index < run.laser_count, "real laser hit identifies its actual emitter");
    }
    for (const auto& event : run.presentation_events)
      if (event.sequence > last && event.kind == hr::gameplay::PresentationEventKind::emp_pulse) {
        require(run.modules[static_cast<unsigned>(hr::gameplay::Module::emp_ring)] > 0 && event.radius > 0, "real EMP activity carries its committed gameplay radius");
        saw_pulse = true;
      }
    for (const auto& event : run.presentation_events)
      last = std::max(last, event.sequence);
  }
  require(saw_drone && saw_pulse, "public earned progression exercises actual drone and EMP metadata");
  game.start();
  require(game.snapshot().drone_burst_seconds == 0 && game.snapshot().capture_burst_seconds == 0 && game.snapshot().movement_burst_seconds == 0, "new run clears active equipment durations");
}

} // namespace
int main() {
  equipment_matches_installed_state();
  explicit_events_and_pause();
  typed_shots_and_reduced_motion();
  real_game_exposes_committed_module_activity();
}
