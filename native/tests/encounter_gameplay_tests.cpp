#include "run_state.h"
#include "test_support.h"

#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>

namespace game = human_resources::gameplay;
namespace tune = game::tuning;
using test_support::require;
using test_support::require_near;
namespace {
constexpr float dt = 1.F / 60;
float distance(game::Vec2 a, game::Vec2 b) { return std::hypot(a.x - b.x, a.z - b.z); }
void opening_and_deliveries() {
  for (unsigned seed = 1; seed <= 100; ++seed) {
    game::Game run;
    run.start({.seed = seed});
    const auto& view = run.snapshot();
    require(view.entities.size() == tune::initial_biological_count, "all authored opening specimens are actually present");
    unsigned near{}, left{}, right{};
    for (const auto& a : view.entities) {
      require(a.position.z > 1 && a.position.z < 23, "opening routes span the visible playable field");
      near += distance(a.position, view.player) < view.capture_radius ? 1U : 0U;
      left += a.position.x < 0 ? 1U : 0U;
      right += a.position.x > 0 ? 1U : 0U;
      for (const auto& b : view.entities) if (a.id != b.id) {
        const auto clearance = tune::biological_body_radius[static_cast<unsigned>(a.kind)] +
            tune::biological_body_radius[static_cast<unsigned>(b.kind)];
        require(distance(a.position, b.position) >= clearance, "opening bodies never overlap");
      }
    }
    require(near >= 2 && left >= 8 && right >= 8,
            "opening offers immediate capture and populated routes on both sides");
    float minimum_x = 100, maximum_x = -100;
    for (const auto& a : view.entities) {
      minimum_x = std::min(minimum_x, a.position.x);
      maximum_x = std::max(maximum_x, a.position.x);
      if (a.id > tune::encounter_cohort_size)
        require(std::abs(a.position.x) - tune::biological_body_radius[static_cast<unsigned>(a.kind)] >
            tune::encounter_open_route_half_width, "later opening pockets leave an actual body-clear central dodge route");
    }
    require(maximum_x - minimum_x > 7, "opening pockets use the field rather than repeating narrow centre rails");
    unsigned homogenous{};
    for (unsigned group = 0; group < tune::initial_biological_count / tune::encounter_cohort_size; ++group) {
      const auto first = group * tune::encounter_cohort_size;
      unsigned matching{};
      for (unsigned slot = 0; slot < tune::encounter_cohort_size; ++slot)
        matching += view.entities[first + slot].kind == view.entities[first].kind ? 1U : 0U;
      homogenous += matching == tune::encounter_cohort_size ? 1U : 0U;
      require(matching >= 2 || view.entities[first + 1].kind == view.entities[first + 2].kind,
              "social pocket contains a herd, two pairs or a farmer with livestock");
      const auto& a = view.entities[first];
      const auto& b = view.entities[first + 1];
      const auto& c = view.entities[first + 2];
      const float area = std::abs((b.position.x - a.position.x) * (c.position.z - a.position.z) -
          (c.position.x - a.position.x) * (b.position.z - a.position.z));
      require(area > .8F, "a real pocket has visible two-dimensional area rather than one diagonal train");
    }
    require(homogenous > 0, "opening includes an actual same-species herd");
    for (unsigned group = 0; group < tune::initial_biological_count / tune::encounter_cohort_size; ++group) {
      const auto first = group * tune::encounter_cohort_size;
      bool cattle{};
      for (unsigned slot = 0; slot < tune::encounter_cohort_size; ++slot)
        cattle = cattle || view.entities[first + slot].kind == game::EntityKind::cow;
      if (!cattle) continue;
      // Measured consumed cow.hrm whole neutral bounds, not the smaller contact
      // core: +/-0.727397263m X and +/-0.899999976m Z. Their corner radius
      // conservatively contains the full ground footprint at EVERY travel yaw.
      const float footprint = std::hypot(.727397263F, .899999976F);
      for (unsigned i = 0; i < tune::encounter_cohort_size; ++i) {
        const auto& a = view.entities[first + i];
        if (group > 0)
          require(std::abs(a.position.x) - footprint > tune::encounter_open_route_half_width,
                  "cattle pockets leave the full rotated body footprint outside the central escape route");
        require(std::abs(a.position.x) + footprint < tune::field_half_width,
                "full rotated cattle footprint stays within the actual playable field");
        for (unsigned j = i + 1; j < tune::encounter_cohort_size; ++j) {
          const auto& b = view.entities[first + j];
          require(std::abs(a.position.x - b.position.x) > 2 * footprint ||
              std::abs(a.position.z - b.position.z) > 2 * footprint,
              "whole cooked cattle ground bodies never intersect within a delivered pocket at any travel heading");
        }
      }
    }
  }
  game::Game::State state;
  state.view.stats.active_seconds = 14;
  state.view.runner_distance = 26.4F;
  state.biological_budget = static_cast<float>(tune::encounter_cohort_size);
  state.next_boss = 150;
  state.director(dt);
  require(state.actors.size() == 4, "one biological budget delivers one full quartet");
  for (const auto& a : state.actors) {
    const auto ahead = a.view.position.z - state.view.runner_distance;
    require(ahead > tune::enemy_spawn_ahead - .2F && ahead < tune::enemy_spawn_ahead + 4,
            "ongoing bodies approach from the actual portrait horizon");
    require(std::abs(a.view.position.x) - tune::biological_body_radius[static_cast<unsigned>(a.view.kind)] >
        tune::encounter_open_route_half_width, "ongoing pockets leave the central dodge route body-clear");
  }
  std::cout << "PASS populated opening routes, immediate capture and horizon quartets\n";
}
game::detail::Actor threat(game::EntityKind kind, unsigned id, game::Vec2 position) {
  game::detail::Actor a;
  a.view.id = id;
  a.view.kind = kind;
  a.view.infantry_role = game::InfantryRole::rifleman;
  a.view.position = position;
  a.view.hull = a.view.maximum_hull = 1000;
  return a;
}
void visible_warnings_and_recovery() {
  game::Game::State horizon;
  horizon.view.player = {0, 6};
  horizon.actors.push_back(threat(game::EntityKind::soldier, 1, {0, 17}));
  horizon.enemies(dt);
  require(horizon.actors.front().view.warning_seconds == 0, "forward flying cannot trigger invisible military warnings");
  horizon.actors.front().view.position = {0, 10};
  horizon.enemies(dt);
  require_near(horizon.actors.front().view.warning_seconds, .9F, .001F, "visible infantry retains full authored dodge warning");

  game::Game::State recovery;
  recovery.view.stats.active_seconds = tune::encounter_build_seconds + tune::encounter_assault_seconds + 1;
  recovery.actors.push_back(threat(game::EntityKind::soldier, 1, {0, 8}));
  recovery.enemies(dt);
  require(recovery.actors.front().view.warning_seconds > 0, "recovery keeps readable attacks alive");
  const auto aim = recovery.actors.front().view.warning_aim;
  for (unsigned frame = 0; frame < 65; ++frame) {
    recovery.view.player.x = 5;
    recovery.enemies(dt);
  }
  require(!recovery.bullets.empty(), "committed recovery warning completes into a real shot");
  require(recovery.actors.front().view.warning_aim.x == aim.x, "movement after warning can dodge the committed aim");

  game::Game::State boss;
  boss.view.stats.active_seconds = 155;
  for (unsigned id = 1; id <= 3; ++id) boss.actors.push_back(threat(game::EntityKind::soldier, id, {static_cast<float>(id), 8}));
  boss.actors.push_back(threat(game::EntityKind::boss, 4, {0, 10}));
  boss.enemies(dt);
  require(boss.actors.back().view.warning_seconds > 0, "boss warning cannot be starved by older infantry");
  unsigned warnings{};
  for (const auto& a : boss.actors) warnings += a.view.warning_seconds > 0 ? 1U : 0U;
  require(warnings <= tune::encounter_boss_warning_cap, "boss phase retains a fair total warning cap");
  std::cout << "PASS visible warnings, active recovery, committed dodges and reserved boss volley\n";
}
void introductions_and_schedule() {
  game::Game::State s;
  s.next_boss = 150;
  s.view.stats.active_seconds = tune::encounter_intro_escort_second;
  s.director(dt);
  require(s.actors.size() == 2, "opening escort pair arrives on time without waiting for a budget");
  for (const auto kind : {game::EntityKind::jeep, game::EntityKind::tank}) {
    game::Game::State state;
    state.next_boss = 150;
    state.view.stats.active_seconds = tune::enemy_start_second[static_cast<unsigned>(kind) - 4];
    state.director(dt);
    require(std::any_of(state.actors.begin(), state.actors.end(), [kind](const auto& a) { return a.view.kind == kind; }),
        "first heavy is introduced at its actual time gate");
  }
  game::Game::State full, empty;
  full.next_boss = empty.next_boss = 150;
  for (unsigned i = 0; i < tune::biological_active_cap; ++i) full.actors.push_back(threat(game::EntityKind::farmer, i + 1, {0, 0}));
  full.biological_budget = empty.biological_budget = 4;
  full.director(dt);
  empty.director(dt);
  require(full.actors.size() == tune::biological_active_cap, "full horizon delivery never exceeds actor capacity");
  require(full.random_state == empty.random_state && full.view.director_spawn_attempts == empty.view.director_spawn_attempts &&
      full.view.director_schedule_hash == empty.view.director_schedule_hash, "blocked cohorts retain the same encounter schedule");
  std::cout << "PASS timed introductions and bounded population-independent cohort schedule\n";
}
void earned_speed_flee_and_expanded_flight() {
  game::Game::State state;
  state.sync();
  require_near(state.view.forward_speed, tune::forward_scroll_metres_per_second, .0001F, "fresh world starts at its authored real speed");
  state.view.stats.captures = 60;
  state.view.stats.level = 7;
  state.sync();
  require_near(state.view.forward_speed, 3.28F, .0001F, "actual collected specimens and earned power raise world travel");
  state.view.stats.captures = 100000;
  state.view.stats.level = 1000;
  state.sync();
  require_near(state.view.forward_speed, tune::scroll_maximum_metres_per_second, .0001F, "earned world acceleration has an explicit upper bound");
  auto body = threat(game::EntityKind::farmer, 1, {-4, 0});
  state.actors = {body, threat(game::EntityKind::farmer, 2, {4, 0})};
  state.view.player = {};
  for (unsigned i = 0; i < 60; ++i) {
    state.move_biological(dt);
    for (const auto& a : state.actors) {
      require(a.view.velocity.z >= 0 && a.view.velocity.z < tune::forward_scroll_metres_per_second,
              "innocents actually flee forward on the ground while the scroll overtakes them");
      require(std::hypot(a.view.velocity.x, a.view.velocity.z) < tune::forward_scroll_metres_per_second,
              "civilian fleeing remains slower than even initial world travel");
    }
  }
  require(state.actors.front().view.position.x < -4 && state.actors.back().view.position.x > 4,
          "civilians open sideways away from the real ship");
  game::Game::State distant, distant_other;
  distant.view.player = {0, -20};
  distant_other.view.player = {6, -20};
  distant.actors = {threat(game::EntityKind::farmer, 1, {-2, 0}), threat(game::EntityKind::farmer, 2, {2, 0})};
  distant_other.actors = distant.actors;
  for (unsigned frame = 0; frame < 60; ++frame) {
    distant.move_biological(dt); distant_other.move_biological(dt);
  }
  for (unsigned i = 0; i < 2; ++i) {
    require(distant.actors[i].view.position.x == distant_other.actors[i].view.position.x &&
        distant.actors[i].view.position.z == distant_other.actors[i].view.position.z,
        "distant civilians keep their own travel heading instead of reacting globally to the ship");
    require(distant.actors[i].view.position.z > 1, "outside fear range civilians still visibly jog forward");
  }
  require(distant.actors[0].view.velocity.x * distant.actors[1].view.velocity.x < 0,
          "individual travel headings differ without changing the forward escape pace");
  game::Game run;
  run.start();
  for (unsigned frame = 0; frame < 60 * 5; ++frame) {
    if (run.snapshot().phase == game::Phase::choosing_upgrade) require(run.choose_upgrade(0), "earned draft can resume boundary flight");
    run.advance(dt, {{0, 1}, false});
  }
  require_near(run.snapshot().player.z - run.snapshot().runner_distance, tune::player_forward_allowance, .05F,
               "actual held forward input can reach the expanded upper flight boundary");
  for (unsigned frame = 0; frame < 60 * 10; ++frame) {
    if (run.snapshot().phase == game::Phase::choosing_upgrade) require(run.choose_upgrade(0), "earned draft can resume return flight");
    run.advance(dt, {{0, -1}, false});
  }
  require_near(run.snapshot().player.z - run.snapshot().runner_distance, -tune::player_backward_allowance, .05F,
               "actual held backward input traverses the full expanded flight band");
  const auto bytes = run.save_state();
  game::Game restored;
  require(restored.restore_state(bytes) == game::SaveStateIssue::none && restored.save_state() == bytes,
          "earned actual speed and changed world station recover from existing serialized counters");
  for (unsigned frame = 0; frame < 180; ++frame) {
    if (run.snapshot().phase == game::Phase::choosing_upgrade) {
      require(run.choose_upgrade(0) && restored.choose_upgrade(0), "paired future accepts the same real draft");
    }
    const game::Control control{{.6F, .3F}, frame % 90 == 0};
    run.advance(dt, control); restored.advance(dt, control);
    require(run.save_state() == restored.save_state(), "accelerated world, AI and stopped headings preserve exact restored future");
  }
  std::cout << "PASS earned bounded speed, real civilian fleeing, expanded flight and restored future\n";
}
void readable_band_and_stopped_mounts() {
  game::Game::State state;
  state.view.player = {0, -17};
  state.actors = {threat(game::EntityKind::soldier, 1, {0, -13})};
  state.enemies(dt);
  require(state.actors.front().view.warning_seconds == 0, "below-screen sources cannot begin a warning near the lower player bound");
  auto& a = state.actors.front();
  a.view.warning_seconds = .5F;
  a.aim = state.view.player;
  state.enemies(dt);
  require(a.view.warning_seconds == 0 && state.bullets.empty(), "an unlaunched warning cancels after its source leaves the readable band");
  state = game::Game::State{};
  state.view.player = {};
  state.spawn(game::EntityKind::jeep);
  auto& jeep = state.actors.front();
  jeep.view.position = {3, 10};
  jeep.attack_timer = 0;
  state.enemies(dt);
  require(jeep.view.body_heading.has_value() && jeep.view.warning_seconds > 0, "actual vehicle initializes an explicit heading before commitment");
  const auto yaw = game::vehicle_body_yaw(jeep.view);
  const auto position = jeep.view.position;
  state.enemies(dt);
  require_near(distance(jeep.view.position, position), 0, .00001F, "vehicle plants while its committed warning is live");
  require_near(game::vehicle_body_yaw(jeep.view), yaw, .00001F, "planted vehicle cannot snap to an ID-derived chassis direction");
  const auto muzzle = game::jeep_minigun_shot(jeep.view, jeep.aim, jeep.view.minigun_barrel);
  require(muzzle.valid, "standoff commitment retains a source-valid mounted shot");
  std::cout << "PASS both readable warning boundaries and stopped source-aligned vehicle heading\n";
}
void real_run_trace() {
  for (const auto seed : {1U, 514U, 7841U}) {
    game::Game run;
    run.start({.seed = seed});
    double first_capture = -1, first_warning = -1, first_shot = -1, first_jeep = -1;
    float empty_window{}, maximum_empty{};
    std::size_t maximum_population{}, maximum_biological{};
    for (unsigned frame = 0; frame < 60 * 120 && run.snapshot().phase != game::Phase::dead; ++frame) {
      if (run.snapshot().phase == game::Phase::choosing_upgrade) require(run.choose_upgrade(0), "natural draft can continue");
      run.advance(dt);
      const auto& v = run.snapshot();
      std::size_t visible{};
      for (const auto& a : v.entities) {
        const float ahead = a.position.z - v.runner_distance;
        if (a.kind < game::EntityKind::soldier && ahead >= -6 && ahead <= 24) ++visible;
        if (first_warning < 0 && a.warning_seconds > 0) first_warning = v.stats.active_seconds;
        if (first_jeep < 0 && a.kind == game::EntityKind::jeep) first_jeep = v.stats.active_seconds;
      }
      if (v.stats.captures > 0 && first_capture < 0) first_capture = v.stats.active_seconds;
      if (!v.projectiles.empty() && first_shot < 0) first_shot = v.stats.active_seconds;
      empty_window = visible ? 0 : empty_window + dt;
      maximum_empty = std::max(maximum_empty, empty_window);
      maximum_population = std::max(maximum_population, v.entities.size());
      maximum_biological = std::max(maximum_biological, visible);
      require(v.entities.size() <= tune::biological_active_cap + 27, "real delivery trace preserves the combined actor cap");
      require(v.projectiles.size() <= tune::projectile_active_cap, "real delivery trace preserves projectile cap");
    }
    require(first_capture > 0 && first_capture < 3, "ordinary stationary opening pays off within three seconds");
    require(first_warning > 2 && first_warning < 9, "opening threat becomes visible and warns without a long idle introduction");
    require(first_shot > first_warning && first_shot < 12, "first authored warning leads to an actual avoidable projectile");
    require(first_jeep >= tune::enemy_start_second[1] && first_jeep < tune::enemy_start_second[1] + 1, "ordinary run delivers the first jeep at the gate");
    require(maximum_empty < .1F, "ordinary first two minutes never lose all visible harvest specimens");
    std::cout << std::fixed << std::setprecision(3) << "TRACE seed=" << seed << " active_seconds=" << run.snapshot().stats.active_seconds
        << " first_capture=" << first_capture << " first_warning=" << first_warning << " first_shot=" << first_shot
        << " first_jeep=" << first_jeep << " max_visible_harvest=" << maximum_biological << " max_population=" << maximum_population
        << " max_empty_harvest_seconds=" << maximum_empty << " captures=" << run.snapshot().stats.captures << '\n';
  }
}
} // namespace
int main() {
  opening_and_deliveries();
  visible_warnings_and_recovery();
  introductions_and_schedule();
  earned_speed_flee_and_expanded_flight();
  readable_band_and_stopped_mounts();
  real_run_trace();
}
