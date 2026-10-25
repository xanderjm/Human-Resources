#include "human_resources/gameplay/gameplay.h"
#include "human_resources/gameplay/tuning.h"
#include "test_support.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <limits>

namespace game = human_resources::gameplay;
namespace tuning = game::tuning;
using test_support::require;
using test_support::require_near;

namespace {
void choose(game::Game& run, std::size_t preferred = 0) {
  if (run.snapshot().phase == game::Phase::choosing_upgrade) {
    require(run.choose_upgrade(preferred), "eligible upgrade selection succeeds");
  }
}
void advance_for(game::Game& run, float seconds, game::Control control = {}) {
  const int frames = static_cast<int>(seconds * 60);
  for (int i = 0; i < frames; ++i) {
    choose(run);
    run.advance(1.0F / 60, control);
  }
}
void phase_and_movement() {
  game::Game run;
  require(run.snapshot().phase == game::Phase::ready, "new game is ready");
  require(!run.choose_upgrade(0), "cannot select an upgrade outside its phase");
  run.start();
  const auto initial = run.snapshot();
  require_near(initial.hull, tuning::base_hull, 0.001F, "initial hull comes from catalogue");
  run.advance(std::numeric_limits<float>::quiet_NaN());
  require(run.snapshot().stats.active_seconds == 0, "invalid elapsed does not mutate time");
  run.advance(100);
  require(run.snapshot().stats.active_seconds <= tuning::maximum_catchup_seconds + 0.001, "catch-up work is bounded");
  run.pause();
  const auto paused = run.snapshot();
  run.advance(10, {{1, 1}, true});
  require(run.snapshot().stats.active_seconds == paused.stats.active_seconds, "pause freezes simulation time");
  require_near(run.snapshot().player.z, paused.player.z, 0.0001F, "pause freezes world movement");
  run.resume();
  advance_for(run, 3, {{1, 1}, false});
  require(run.snapshot().player.x > paused.player.x, "semantic movement moves ship");
  require(run.snapshot().player.x <= tuning::field_half_width, "movement respects playable horizontal bounds");
  run.start();
  run.advance(1.0F / 60, {{std::numeric_limits<float>::infinity(), 0}, false});
  require(std::isfinite(run.snapshot().player.x), "invalid controls do not poison state");
}
void draft_and_restart() {
  game::Game run;
  run.start();
  for (int i = 0; i < 60 * 30 && run.snapshot().phase == game::Phase::active; ++i) {
    run.advance(1.0F / 60);
  }
  require(run.snapshot().phase == game::Phase::choosing_upgrade, "survival earns a first upgrade by nineteen seconds");
  const auto frozen = run.snapshot().stats.active_seconds;
  run.advance(1);
  require(run.snapshot().stats.active_seconds == frozen, "upgrade choice freezes every active clock");
  const auto& choices = run.snapshot().choices;
  for (std::size_t i = 0; i < choices.size(); ++i) {
    require(!choices[i].title.empty() && !choices[i].description.empty(), "upgrade communicates its result");
    for (std::size_t j = i + 1; j < choices.size(); ++j) {
      require(choices[i].hardware != choices[j].hardware || (choices[i].hardware ? choices[i].module != choices[j].module : choices[i].research != choices[j].research), "offer has distinct families");
    }
  }
  require(!run.choose_upgrade(3), "out of range selection is rejected");
  require(run.reroll() && run.reroll(), "two free rerolls available");
  require(!run.reroll(), "reroll allowance is bounded per run");
  require(run.choose_upgrade(0), "select card and continue");
  run.start();
  require(run.snapshot().stats.level == 1 && run.snapshot().stats.active_seconds == 0, "restart resets run progression");
  require(run.snapshot().rerolls_remaining == tuning::rerolls, "restart restores rerolls");
  for (const auto rank : run.snapshot().modules) {
    require(rank == 0, "restart resets installed hardware");
  }
  game::RunSetup setup;
  setup.permanent_ranks = {255, 255, 255, 255};
  run.start(setup);
  require_near(run.snapshot().hull, tuning::base_hull + tuning::permanent_per_rank[1] * tuning::permanent_maximum_ranks[1], 0.001F, "external ranks cannot exceed Hangar caps");
  require(run.snapshot().laser_count == 1 && run.snapshot().beam_capacity == 1, "Hangar never adds weapons or beams");
}
void capture_and_schedule() {
  game::Game run;
  run.start();
  for (int i = 0; i < 60 * 90 && run.snapshot().phase != game::Phase::dead; ++i) {
    choose(run);
    const auto& view = run.snapshot();
    game::Vec2 movement{0, -tuning::forward_scroll_metres_per_second / tuning::base_movement_metres_per_second};
    float distance = 100000;
    for (const auto& entity : view.entities) {
      if (entity.kind > game::EntityKind::pig) {
        continue;
      }
      const float dx = entity.position.x - view.player.x, dz = entity.position.z - view.player.z;
      if (dx * dx + dz * dz < distance) {
        distance = dx * dx + dz * dz;
        movement = {dx * 0.25F, dz * 0.25F - tuning::forward_scroll_metres_per_second / tuning::base_movement_metres_per_second};
      }
    }
    run.advance(1.0F / 60, {movement, false});
  }
  require(run.snapshot().stats.captures > 5, "spatial biological capture produces real score and progression");
  require(run.snapshot().stats.score > 0, "captures contribute specimen score");
  game::Game a, b;
  game::RunSetup setup;
  setup.seed = 90;
  a.start(setup);
  setup.permanent_ranks = {5, 5, 5, 5};
  b.start(setup);
  for (int i = 0; i < 60 * 90; ++i) {
    choose(a, 0);
    choose(b, 2);
    // Run the same number of active fixed steps despite different draft timing.
    a.advance(1.0F / 60, {{std::sin(static_cast<float>(i) * 0.03F), 0}, false});
    b.advance(1.0F / 60, {{std::cos(static_cast<float>(i) * 0.01F), 0}, false});
    require(a.snapshot().phase != game::Phase::dead && b.snapshot().phase != game::Phase::dead, "early director comparison remains alive");
  }
  require(a.snapshot().director_spawn_attempts == b.snapshot().director_spawn_attempts, "director attempted schedule independent of upgrades and movement");
  require(a.snapshot().director_schedule_hash == b.snapshot().director_schedule_hash, "director RNG independent of drafts and combat RNG");
}
void run_to_death() {
  game::Game run;
  run.start({7841});
  bool saw_projectile = false, saw_warning = false;
  std::size_t maximum_entities = 0, maximum_projectiles = 0;
  const auto started = std::chrono::steady_clock::now();
  for (int i = 0; i < 60 * 1800 && run.snapshot().phase != game::Phase::dead; ++i) {
    choose(run);
    run.advance(1.0F / 60);
    const auto& view = run.snapshot();
    saw_projectile = saw_projectile || !view.projectiles.empty();
    for (const auto& entity : view.entities) {
      saw_warning = saw_warning || entity.warning_seconds > 0;
    }
    maximum_entities = std::max(maximum_entities, view.entities.size());
    maximum_projectiles = std::max(maximum_projectiles, view.projectiles.size());
    require(view.entities.size() <= 55, "endless director actors remain bounded");
    require(view.projectiles.size() <= tuning::projectile_active_cap, "projectile population bounded");
    require(view.stats.evolutions <= tuning::maximum_evolutions, "evolution cap enforced");
    for (const auto rank : view.modules) {
      require(rank <= 3, "module ranks capped");
    }
  }
  require(saw_warning && saw_projectile, "military telegraphs lead to spatial projectiles");
  require(run.snapshot().phase == game::Phase::dead, "uncontrolled ship reaches genuine damage death");
  require(run.snapshot().stats.active_seconds > 30, "run remains active beyond introduction");
  const auto ended = run.snapshot().stats.active_seconds;
  run.advance(5);
  require(run.snapshot().stats.active_seconds == ended, "death freezes world time");
  if (run.revive_available()) {
    const auto stats = run.snapshot().stats;
    require(run.revive(), "eligible completed reward revives once");
    require(run.snapshot().stats.active_seconds == stats.active_seconds, "revive preserves active run time");
    require_near(run.snapshot().hull, run.snapshot().maximum_hull * tuning::revive_hull_fraction, 0.001F, "revive restores configured hull");
    require(run.snapshot().projectiles.empty(), "revive clears ordinary projectiles");
    require(!run.revive(), "cannot revive a living ship");
  }
  const double wall = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
  std::cout << "Gameplay probe: death at " << ended << " active seconds; peak actors " << maximum_entities << ", projectiles " << maximum_projectiles << "; host wall " << wall << "s\n";
}
} // namespace
int main() {
  phase_and_movement();
  draft_and_restart();
  capture_and_schedule();
  run_to_death();
}
