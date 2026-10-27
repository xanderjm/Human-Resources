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
  require(count(f, "ufo-modular") == 1 && count(f, "ship-drone") == 0 && count(f, "shield-shell") == 0, "starter does not pretend to have orbit drones or upgraded shield");
  const auto& antenna = *std::find_if(f.instances.begin(),f.instances.end(),[](const auto& p){return p.mesh=="ufo-antenna";});
  require_near(antenna.position.x, 2 - 1.12F * 1.012F * std::sin(.18F), 1e-5F, "top socket follows banked fixed hull");
  require_near(antenna.position.y, 2.5F + 1.12F * 1.012F * std::cos(.18F), 1e-5F, "top socket retains source height");
  run.modules[static_cast<unsigned>(hr::gameplay::Module::shield_generator)] = 3;
  run.modules[static_cast<unsigned>(hr::gameplay::Module::orbit_drones)] = 3;
  run.modules[static_cast<unsigned>(hr::gameplay::Module::laser_array)] = 3;
  run.laser_count = 4;
  f.instances.clear();
  hr::product::append_ship_gear(f, run, pose, false);
  require(count(f, "ufo-antenna") == 0 && count(f, "ship-shield-base") == 1 && count(f, "ship-shield-crystal") == 3, "shield replaces antenna at the same socket");
  require(count(f, "ufo-modular") == 1 && count(f, "ship-drone") == 3 && count(f, "ufo-laser") == 4, "four hull lasers plus exactly three miniature craft with embedded real weapons");
  require(count(f, "shield-shell") == 1, "charged shield generator has one rim sphere");
  for (const auto& i : f.instances)
    if (i.mesh == "shield-shell")
      require(i.effect && i.shield_rim && !i.casts_shadow, "shield uses existing transparent depth-tested rim contract");
  run.shield = 0;
  f.instances.clear();
  hr::product::append_ship_gear(f, run, pose, false);
  require(count(f, "shield-shell") == 0, "depleted shield has no false active field");
}
void pitched_hull_has_one_rigid_socket_transform() {
  hr::gameplay::Snapshot run;
  run.laser_count = 1;
  for (float pitch : {-.13F, .13F})
    for (float bank : {-.23F, .23F}) {
      const hr::product::ShipPose pose{{2, 2.5F, 3}, 1.12F, bank, pitch};
      const auto parent = hr::presentation::model_matrix({.position = pose.position,
          .scale = {pose.scale, pose.scale, pose.scale},
          .rotation = {pitch, 0, bank}});
      hr::presentation::StyleFrame frame;
      hr::product::append_ship_gear(frame, run, pose, false);
      for (const auto& part : frame.instances)
        if (part.mesh == "ufo-antenna" || part.mesh == "ufo-beam") {
          const auto local = hr::presentation::model_matrix({.position = {0, part.mesh == "ufo-antenna" ? 1.012F : .04F, 0}});
          const auto expected = hr::presentation::multiply(parent, local);
          const auto actual = hr::presentation::model_matrix(part);
          for (std::size_t i = 0; i < actual.size(); ++i)
            require_near(actual[i], expected[i], 1e-5F, "pitched and banked module stays rigidly attached on every axis");
          require(part.polished_hull, "actual hull hardware shares its restrained material response");
        }
      const auto intake = hr::product::ship_socket(pose, {0, .04F, 0});
      require_near(intake.x, parent[1] * .04F + parent[3], 1e-5F, "beam socket X matches rigid hull");
      require_near(intake.y, parent[5] * .04F + parent[7], 1e-5F, "beam socket height matches rigid hull");
      require_near(intake.z, parent[9] * .04F + parent[11], 1e-5F, "forward lean keeps intake on its actual hull socket");
    }
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
void real_skill_payoff_state_has_bounded_world_motion() {
  hr::gameplay::Snapshot run;
  hr::product::SceneEffects effects;
  run.stats.active_seconds = 1;
  effects.update(run);
  run.stats.active_seconds = 1.1;
  run.player = {2, 1};
  run.dash_remaining_seconds = .1F;
  run.presentation_events[0] = {1, 1.1, hr::gameplay::PresentationEventKind::phase_dash, 0, {}};
  effects.update(run);
  const auto impulse = effects.dash_response();
  require(impulse.x > .8F && impulse.z > .4F, "real dash direction drives the rigid flight impulse");
  hr::presentation::StyleFrame frame;
  const hr::product::ShipPose pose{{2, 2.5F, 1.3F}, 1.12F, -.2F, -.1F};
  hr::product::append_gear_effects(frame, run, &effects, pose, 0, false);
  require(count(frame, "ufo-modular") == 2 && count(frame,"ship-dome")==2 && count(frame,"ship-rotor-0")==2 && count(frame,"ufo-rim-lens")==12, "actual dash carries two complete physical hull assemblies and two finite wake rails");
  run.stats.active_seconds = 1.13;
  run.player = {5, 8};
  effects.update(run);
  require(effects.beats()[0].destination.x == 5 && effects.beats()[0].destination.z == 8,
      "live burst wake follows the actual observed endpoint during its travel");
  run.dash_remaining_seconds = 0;
  run.stats.active_seconds = 1.26;
  effects.update(run);
  const auto completed = effects.beats()[0].destination;
  run.player = {8, 9}; run.stats.active_seconds = 1.27;
  effects.update(run);
  require(effects.beats()[0].destination.x == completed.x && effects.beats()[0].destination.z == completed.z,
      "ordinary movement after the catch cannot stretch the finished dash path");
  frame.instances.clear();
  hr::product::append_gear_effects(frame, run, &effects, pose, 0, true);
  require(count(frame, "ufo-modular") == 0 && frame.instances.size() == 2, "reduced motion preserves brief dash direction without moving afterimages");
  run.presentation_events[1] = {2, 1.27, hr::gameplay::PresentationEventKind::sweep_capture, 7, {}};
  run.presentation_events[2] = {3, 1.27, hr::gameplay::PresentationEventKind::harvest_overdrive, 0, run.player};
  run.harvest_overdrive_seconds = 6;
  run.stats.active_seconds = 1.28;
  effects.update(run);
  frame.instances.clear();
  hr::product::append_gear_effects(frame, run, &effects, pose, 0, false);
  require(frame.instances.size() == 40, "dash retains two complete13part assemblies alongside14 finite skill accents");
  const auto overdrive_angle = effects.rotor_angle();
  effects.update(run);
  require(effects.rotor_angle() == overdrive_angle, "paused overdrive freezes rotor acceleration and all effect clocks");
  run.stats.active_seconds = 2;
  run.harvest_overdrive_seconds = 0;
  effects.update(run);
  frame.instances.clear();
  hr::product::append_gear_effects(frame, run, &effects, pose, 0, false);
  require(frame.instances.empty(), "expired real skill states leave no fake active power or dash wake");
}

} // namespace
int main() {
  equipment_matches_installed_state();
  pitched_hull_has_one_rigid_socket_transform();
  explicit_events_and_pause();
  typed_shots_and_reduced_motion();
  real_game_exposes_committed_module_activity();
  real_skill_payoff_state_has_bounded_world_motion();
}
