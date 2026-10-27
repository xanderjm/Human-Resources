#include "run_state.h"
#include "test_support.h"
#include <iostream>

namespace gp = human_resources::gameplay;
namespace tune = gp::tuning;
using test_support::require;
using test_support::require_near;
namespace {
constexpr float dt = 1.F / 60;
gp::detail::Actor enemy(gp::EntityKind kind, unsigned id, gp::Vec2 position) {
  gp::detail::Actor a;
  a.view.id = id; a.view.kind = kind; a.view.position = position;
  a.view.hull = a.view.maximum_hull = 100000;
  a.view.body_heading = 0; a.locomotion_velocity = {0, -1};
  if (kind == gp::EntityKind::soldier) a.view.infantry_role = gp::InfantryRole::rifleman;
  return a;
}
void arena_tick(gp::Game::State& state, float speed) {
  const float distance = speed * dt;
  state.view.forward_speed = speed;
  state.view.runner_distance += distance;
  state.view.player.z += distance;
  state.view.velocity = {0, speed};
  state.scroll_military(distance);
  state.view.stats.active_seconds += dt;
  state.view.invulnerability_seconds = 1;
  state.enemies(dt); state.projectiles(dt);
  std::erase_if(state.bullets, [](const auto& p) { return p.removed; });
}
void entry_retreat_and_lifetime() {
  for (float speed : {tune::forward_scroll_metres_per_second, tune::scroll_maximum_metres_per_second}) {
    for (auto kind : {gp::EntityKind::soldier, gp::EntityKind::jeep, gp::EntityKind::tank}) {
      gp::Game::State state;
      state.view.player = {0, 0}; state.next_id = 2;
      state.actors = {enemy(kind, 1, {4, tune::enemy_spawn_ahead})};
      bool entered = false, backed = false;
      for (unsigned tick = 0; tick < 60 * 120; ++tick) {
        // Exercise both player extremes after ingress, then return to centre.
        const float station = tick < 1200 ? 0.F : tick < 2400 ? 16.5F : tick < 3600 ? -17.F : 0.F;
        state.view.player = {0, state.view.runner_distance + station};
        arena_tick(state, speed);
        const auto& a = state.actors.front();
        const float z = a.view.position.z - state.view.runner_distance;
        require(!a.removed && a.view.id == 1, "military survives scroll and its former timeout until defeated or captured");
        if (z < 15) entered = true;
        if (entered) require(z >= -15.001F && z <= 15.001F, "engaged enemies stay within the arena through both flight extremes");
        require(std::hypot(a.view.velocity.x, a.view.velocity.z) <= tune::military_movement_speed[gp::detail::military_index(kind)] + .001F,
            "arena following retains bounded local locomotion");
        if (kind == gp::EntityKind::soldier && a.view.warning_seconds == 0 && !a.infantry_aim.preparing && a.view.fire_recovery_seconds == 0) {
          const auto facing = gp::detail::direction(a.locomotion_velocity);
          backed = backed || a.view.velocity.x * facing.x + a.view.velocity.z * facing.z < -.1F;
        }
      }
      require(entered, "each military class physically enters from beyond the top edge");
      if (kind == gp::EntityKind::soldier) require(backed, "infantry can retreat while retaining its player-facing body");
    }
  }
}
void warning_translation_and_disable() {
  for (auto kind : {gp::EntityKind::soldier, gp::EntityKind::jeep, gp::EntityKind::tank}) {
    gp::Game::State state; state.view.player = {}; state.next_id = 2;
    auto a = enemy(kind, 1, {3, 8});
    a.attack_timer = 0;
    state.actors = {a};
    for (unsigned tick = 0; tick < 180 && state.actors.front().view.warning_seconds == 0; ++tick) arena_tick(state, 6.8F);
    require(state.actors.front().view.warning_seconds > 0, "moving arena admits a complete ordinary warning");
    const auto root = state.actors.front().view.position;
    const float runner = state.view.runner_distance;
    const auto aim = state.actors.front().aim;
    const float initial = state.actors.front().view.warning_seconds;
    require_near(aim.x, state.view.player.x, .00001F, "warning aims at the actual player rather than a far extrapolation");
    require_near(aim.z, state.view.player.z, .00001F, "longitudinal aim targets the actual player at warning lock");
    for (unsigned tick = 0; tick < 20; ++tick) {
      state.view.player.x = -5;
      arena_tick(state, 6.8F);
      const auto& actual = state.actors.front();
      require_near(actual.view.position.x, root.x, 0, "committed warning plants lateral root");
      require_near(actual.view.position.z - state.view.runner_distance, root.z - runner, .0001F, "committed source remains planted relative to the arena");
      require_near(actual.aim.x, aim.x, 0, "warning does not follow a lateral dodge");
      require_near(actual.aim.z - state.view.runner_distance, aim.z - runner, .0001F, "warning retains its committed arena station");
      require(gp::detail::length_squared(actual.view.velocity) == 0, "a planted source publishes no local locomotion");
    }
    require_near(state.actors.front().view.warning_seconds, initial - 20 * dt, .00001F, "arena following never shortens the warning");
    state.actors.front().view.disabled_seconds = 10;
    const float station = state.actors.front().view.position.z - state.view.runner_distance;
    for (unsigned tick = 0; tick < 120; ++tick) arena_tick(state, 6.8F);
    require_near(state.actors.front().view.position.z - state.view.runner_distance, station, .001F,
        "disabled enemies stay on the field without taking a local step or timing out");
    require(state.actors.front().view.warning_seconds == 0, "disable still cancels the actual attack");
  }
}
void scrolling_continuation_and_pause() {
  gp::Game run; run.start({.seed = 514});
  auto& s = *run.state_;
  s.actors = {enemy(gp::EntityKind::soldier, s.next_id++, {4, 8})};
  s.view.beams.clear(); s.view.stats.captures = 100; s.sync();
  for (unsigned tick = 0; tick < 600; ++tick) {
    if (run.snapshot().phase == gp::Phase::choosing_upgrade) require(run.choose_upgrade(0), "resolve earned engagement fixture upgrade");
    s.view.invulnerability_seconds = 1;
    run.advance(dt, {{std::sin(static_cast<float>(tick) * .02F), .1F}, false});
  }
  const auto bytes = run.save_state();
  gp::Game cold;
  require(!bytes.empty() && cold.restore_state(bytes) == gp::SaveStateIssue::none, "scrolling combat saves and cold restores");
  run.pause(); cold.pause();
  const auto paused = run.save_state();
  run.advance(.2F); require(run.save_state() == paused, "pause freezes common scrolling and local engagement together");
  run.resume(); cold.resume();
  for (unsigned tick = 0; tick < 600; ++tick) {
    for (auto* g : {&run, &cold}) {
      if (g->snapshot().phase == gp::Phase::choosing_upgrade) require(g->choose_upgrade(0), "resolve identical continuation choice");
      g->state_->view.invulnerability_seconds = 1;
      g->advance(dt, {{std::sin(static_cast<float>(tick) * .03F), -.2F}, tick == 90});
    }
    require(!run.save_state().empty() && run.save_state() == cold.save_state(), "engagement cold future remains exact under movement and Dash");
  }
}
} // namespace
int main() {
  entry_retreat_and_lifetime();
  warning_translation_and_disable();
  scrolling_continuation_and_pause();
  std::cout << "PASS arena ingress, persistent engagement, retreat, warnings, disable, pause and cold futures\n";
}
