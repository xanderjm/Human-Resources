#include "run_state.h"
#include "test_support.h"
#include <iostream>

namespace gp = human_resources::gameplay;
using test_support::require;
using test_support::require_near;
namespace {
constexpr float dt = 1.F / 60.F;
gp::Game fixture(unsigned mounts, unsigned targets = 4) {
  gp::Game game;
  game.start({.seed = 413, .unlocked_blueprints = 0xffffU});
  auto& s = *game.state_;
  s.actors.clear();
  s.view.modules[static_cast<unsigned>(gp::Module::laser_array)] = static_cast<std::uint8_t>(mounts - 1);
  for (unsigned i = 0; i < targets; ++i) {
    gp::detail::Actor actor;
    actor.view.id = s.next_id++;
    actor.view.kind = gp::EntityKind::soldier;
    actor.view.infantry_role = gp::InfantryRole::rifleman;
    actor.view.position = {static_cast<float>(i) + 2.F, 4};
    actor.view.hull = actor.view.maximum_hull = 1000;
    actor.locomotion_velocity = {0, -1};
    actor.attack_timer = 100;
    s.actors.push_back(actor);
  }
  s.sync();
  return game;
}
void tick(gp::Game& game) {
  auto& s = *game.state_;
  s.view.shots.clear();
  s.view.stats.active_seconds += dt;
  s.combat(dt);
  s.sync();
}
void sequential_mounts_and_cadence() {
  for (unsigned mounts = 1; mounts <= 4; ++mounts) {
    auto game = fixture(mounts);
    std::vector<double> times;
    std::vector<gp::Vec2> targets;
    for (unsigned frame = 0; frame < 100; ++frame) {
      tick(game);
      const auto& run = game.snapshot();
      require(run.shots.size() <= 1, "a simulation tick never releases simultaneous hull lasers");
      for (const auto& shot : run.shots) {
        require(shot.emitter_index == times.size() % mounts, "every actual mount fires once in order");
        times.push_back(run.stats.active_seconds);
        targets.push_back(shot.to);
      }
    }
    require(times.size() >= mounts * 2, "multiple full bursts fire");
    for (unsigned i = 1; i < mounts; ++i) {
      require_near(static_cast<float>(times[i] - times[i - 1]), gp::tuning::laser_stagger_seconds, dt, "rapid delay separates each mount");
      require(targets[i].x != targets[i - 1].x, "burst spreads onto distinct live targets when possible");
    }
    require_near(static_cast<float>(times[mounts] - times[0]), 1 / gp::tuning::base_laser_shots_per_second, dt, "whole burst preserves baseline repeat cadence");
  }
}
void live_retarget_and_damage() {
  auto game = fixture(3, 1);
  tick(game);
  auto& s = *game.state_;
  require_near(s.actors[0].view.hull, 1000 - gp::tuning::base_laser_damage, .001F, "first mount keeps full damage");
  while (s.next_laser != 0) tick(game);
  require_near(s.actors[0].view.hull, 1000 - gp::tuning::base_laser_damage * (1 + 2 * gp::tuning::added_laser_fraction), .001F, "extra mounts keep half damage and can concentrate on one target");
  game = fixture(3);
  tick(game);
  auto& state = *game.state_;
  state.actors[1].lifted = true;
  state.actors[2].removed = true;
  while (state.next_laser == 1) tick(game);
  require(game.snapshot().shots.size() == 1 && game.snapshot().shots[0].to.x == state.actors[3].view.position.x,
      "delayed shot retargets at release, skipping removed and lifted actors");
  for (auto& actor : state.actors) actor.view.position.x = 100;
  while (state.next_laser != 0) tick(game);
  require(game.snapshot().shots.empty(), "no delayed ray or damage is invented when targets leave range");
}
void pause_dash_and_exact_restore() {
  for (unsigned next = 1; next < 4; ++next) {
    auto game = fixture(4);
    do { tick(game); } while (game.state_->next_laser != next);
    game.pause();
    const auto paused = game.save_state();
    require(!paused.empty(), "mid-burst pause is a valid durable continuation");
    game.advance(10);
    require(game.save_state() == paused, "pause freezes the pending burst and cooldown");
    gp::Game cold;
    require(cold.restore_state(paused) == gp::SaveStateIssue::none, "cold restore accepts every pending mount");
    game.resume(); cold.resume();
    for (unsigned i = 0; i < 60; ++i) {
      tick(game); tick(cold);
      require(game.save_state() == cold.save_state(), "cold future retains exact targets, timing, focus and damage");
      require(game.snapshot().shots.size() == cold.snapshot().shots.size(), "cold release matches live release");
      if (!game.snapshot().shots.empty())
        require(game.snapshot().shots[0].emitter_index == cold.snapshot().shots[0].emitter_index, "cold emitter never restarts at mount zero");
    }
  }
  auto dash = fixture(4);
  tick(dash);
  dash.state_->move(dt, {{1, 0}, true});
  tick(dash);
  require(dash.snapshot().shots.size() == 1 && dash.snapshot().shots[0].emitter_index == 0 && dash.state_->next_laser == 1,
      "Dash begins one immediate counter-burst, then continues sequentially");
}
void boosted_burst() {
  auto game = fixture(4);
  auto& s = *game.state_;
  s.view.research[static_cast<unsigned>(gp::Research::fire_rate)] = 100;
  s.view.harvest_overdrive_seconds = 6;
  std::vector<double> times;
  for (unsigned frame = 0; frame < 40; ++frame) {
    tick(game);
    if (!game.snapshot().shots.empty()) times.push_back(game.snapshot().stats.active_seconds);
  }
  require(times.size() >= 8, "Overdrive and fire research retain complete bursts");
  for (unsigned i = 1; i < 4; ++i)
    require(times[i] > times[i - 1] && times[i] - times[i - 1] <= .035, "boosted mounts remain separate quick releases");
  require_near(static_cast<float>(times[4] - times[0]), 1 / (gp::tuning::base_laser_shots_per_second * s.fire_multiplier()), dt, "boosted overall cadence is retained");
}
}
int main() {
  sequential_mounts_and_cadence(); live_retarget_and_damage(); pause_dash_and_exact_restore(); boosted_burst();
  std::cout << "Sequential laser gameplay contracts passed\n";
}
