#include "run_state.h"
#include "test_support.h"
#include <iostream>

namespace gp = human_resources::gameplay;
namespace tune = gp::tuning;
using test_support::require;
using test_support::require_near;
namespace {
constexpr float dt = 1.F / 60;
float distance(gp::Vec2 a, gp::Vec2 b) {
  return std::hypot(a.x - b.x, a.z - b.z);
}
gp::detail::Actor actor(gp::EntityKind kind, unsigned id, gp::Vec2 position) {
  gp::detail::Actor a;
  a.view.id = id;
  a.view.kind = kind;
  a.view.position = position;
  a.view.hull = a.view.maximum_hull = 90;
  if (kind == gp::EntityKind::soldier)
    a.view.infantry_role = gp::InfantryRole::rifleman;
  a.attack_timer = 100; // Isolate approach; committed attacks have separate proofs below.
  return a;
}
void bounded_approach_and_planted_sources() {
  for (auto moving : {gp::EntityKind::soldier, gp::EntityKind::jeep, gp::EntityKind::tank, gp::EntityKind::boss}) {
    for (auto fixed : {gp::EntityKind::soldier, gp::EntityKind::jeep, gp::EntityKind::tank, gp::EntityKind::boss}) {
      gp::Game::State s;
      s.view.player = {0, 0};
      const float gap = tune::military_body_radius[static_cast<unsigned>(moving) - 4] +
                        tune::military_body_radius[static_cast<unsigned>(fixed) - 4] + tune::military_spacing_margin;
      s.actors.push_back(actor(fixed, 1, {0, 9}));
      s.actors.push_back(actor(moving, 3, {0, 9 + gap + 1}));
      s.actors[0].view.disabled_seconds = 100;
      const auto original = s.actors[0].view.position;
      bool moved = false;
      for (unsigned frame = 0; frame < 240; ++frame) {
        auto& a = s.actors[1];
        const auto before = a.view.position;
        const auto wanted = gp::detail::scaled(gp::detail::direction({0 - before.x, 4 - before.z}), tune::military_movement_speed[static_cast<unsigned>(moving) - 4]);
        const auto velocity = s.military_velocity(a, wanted, dt);
        a.view.velocity = velocity;
        a.view.position = gp::detail::add(a.view.position, gp::detail::scaled(velocity, dt));
        moved = moved || distance(a.view.position, before) > .001F;
        require(distance(a.view.position, original) + .0001F >= gap, "every military body pair keeps its measured approach envelope");
        require(distance(a.view.position, before) <= tune::military_movement_speed[static_cast<unsigned>(moving) - 4] * dt + .00001F,
            "avoidance changes ordinary bounded velocity, never teleports or pushes a root");
        require(std::abs(a.view.position.x) <= tune::field_half_width - tune::military_body_radius[static_cast<unsigned>(moving) - 4] + .0001F,
            "accepted military body motion stays within the whole-body field edge");
      }
      require(moved && distance(s.actors[1].view.position, original) > gap, "approaching military can route around a fixed body without hiding it");
      require(distance(s.actors[0].view.position, original) == 0, "local approach never changes the obstacle root");
    }
  }
  std::cout << "PASS all sixteen military body pairs route around fixed bodies with bounded motion\n";
}
void locked_aim_and_recovery() {
  for (auto kind : {gp::EntityKind::soldier, gp::EntityKind::jeep, gp::EntityKind::tank, gp::EntityKind::boss}) {
    gp::Game::State state;
    state.view.player = {0, 0};
    auto source = actor(kind, 1, {3, 9});
    source.view.warning_seconds = .4F;
    source.aim = source.view.warning_aim = {0, 0};
    source.locomotion_velocity = {0, -1};
    state.actors.push_back(source);
    state.actors.push_back(actor(gp::EntityKind::jeep, 3, {3, 16}));
    const auto original = source.view.position;
    for (unsigned frame = 0; frame < 20; ++frame) {
      state.enemies(dt);
      const auto& actual = state.actors[0];
      require(distance(actual.view.position, original) == 0 && distance(actual.view.warning_aim, source.aim) == 0,
          "spacing cannot displace a live committed source or retarget its warning");
      require(distance(actual.view.velocity, {}) == 0, "committed source remains visibly planted");
    }
  }
  std::cout << "PASS spacing leaves all committed military roots and warning aims exact\n";
}
void capture_projection_and_edge_return() {
  gp::Game::State state;
  state.actors.push_back(actor(gp::EntityKind::soldier, 1, {0, 9}));
  state.actors.push_back(actor(gp::EntityKind::jeep, 3, {0, 14}));
  state.actors[0].lifted = true;
  state.actors[0].view.capture_progress = .5F;
  for (unsigned frame = 0; frame < 180; ++frame) {
    if (frame == 90) {
      state.actors[0].lifted = false;
      state.actors[0].view.capture_progress = 0;
    }
    auto& moving = state.actors[1];
    const gp::Vec2 wanted = gp::detail::scaled(gp::detail::direction({0 - moving.view.position.x, 4 - moving.view.position.z}), tune::military_movement_speed[1]);
    moving.view.velocity = state.military_velocity(moving, wanted, dt);
    moving.view.position = gp::detail::add(moving.view.position, gp::detail::scaled(moving.view.velocity, dt));
    require(distance(moving.view.position, state.actors[0].view.position) >= tune::military_body_radius[0] + tune::military_body_radius[1],
        "capture reserves its ground projection so cancellation never returns into another body");
  }
  auto released = actor(gp::EntityKind::soldier, 5, {7.3F, 9});
  state.actors = {released};
  for (unsigned frame = 0; frame < 60; ++frame) {
    auto& a = state.actors[0];
    const auto velocity = state.military_velocity(a, {-3.2F, 0}, dt);
    a.view.position = gp::detail::add(a.view.position, gp::detail::scaled(velocity, dt));
  }
  require(state.actors[0].view.position.x < tune::field_half_width - tune::military_body_radius[0],
      "a soldier released beyond the whole-body field edge returns through normal movement");
  std::cout << "PASS capture projection reservation and bounded inward return after edge release\n";
}
void ingress_and_random_stream() {
  for (unsigned seed = 1; seed <= 40; ++seed) {
    gp::Game::State state, control;
    state.random_state = control.random_state = seed;
    for (unsigned i = 0; i < 27; ++i) {
      const auto kind = i < 18 ? gp::EntityKind::soldier : i < 23 ? gp::EntityKind::jeep
                                                       : i < 26   ? gp::EntityKind::tank
                                                                  : gp::EntityKind::boss;
      state.spawn(kind);
      for (unsigned sample = 0; sample < 4; ++sample)
        (void)control.random_unit();
    }
    require(state.actors.size() == 27, "bounded clearance keeps every authored maximum-population arrival");
    require(state.random_state == control.random_state && state.view.director_spawn_attempts == 27,
        "military placement consumes no additional encounter random samples or spawn attempts");
    for (std::size_t i = 0; i < state.actors.size(); ++i)
      for (std::size_t j = i + 1; j < state.actors.size(); ++j) {
        const auto& a = state.actors[i];
        const auto& b = state.actors[j];
        const float gap = tune::military_body_radius[static_cast<unsigned>(a.view.kind) - 4] + tune::military_body_radius[static_cast<unsigned>(b.view.kind) - 4];
        require(distance(a.view.position, b.view.position) + .0001F >= gap,
            "even a maximum-density authored ingress cannot start military bodies overlapped");
      }
  }
  std::cout << "PASS maximum-cap ingress retains actors and encounter RNG without overlapping bodies\n";
}
void public_reproduction_and_restore() {
  for (unsigned seed : {1U, 514U, 7841U}) {
    gp::Game run;
    run.start({.seed = seed});
    bool checkpointed = false;
    for (unsigned frame = 0; frame < 60 * 150 && run.snapshot().phase != gp::Phase::dead; ++frame) {
      if (run.snapshot().phase == gp::Phase::choosing_upgrade)
        require(run.choose_upgrade(0), "ordinary earned choice resumes the public spacing reproduction");
      run.advance(dt);
      const auto& s = run.snapshot();
      for (std::size_t i = 0; i < s.entities.size(); ++i)
        for (std::size_t j = i + 1; j < s.entities.size(); ++j) {
          const auto& a = s.entities[i];
          const auto& b = s.entities[j];
          if (a.kind < gp::EntityKind::soldier || b.kind < gp::EntityKind::soldier || a.capture_progress > 0 || b.capture_progress > 0)
            continue;
          const float gap = tune::military_body_radius[static_cast<unsigned>(a.kind) - 4] + tune::military_body_radius[static_cast<unsigned>(b.kind) - 4];
          require(distance(a.position, b.position) + .001F >= gap, "fresh public Game no longer collapses military body roots into shared flank targets");
        }
      if (!checkpointed && s.stats.active_seconds > 32) {
        checkpointed = true;
        run.pause();
        const auto bytes = run.save_state();
        run.advance(10, {{1, 1}, true});
        require(run.save_state() == bytes, "paused approaches preserve exact saved roots and clocks");
        gp::Game copy;
        require(copy.restore_state(bytes) == gp::SaveStateIssue::none && copy.save_state() == bytes, "actual crowded run round trips byte exactly");
        run.resume();
        copy.resume();
        for (unsigned future = 0; future < 240; ++future) {
          if (run.snapshot().phase == gp::Phase::choosing_upgrade)
            require(run.choose_upgrade(0), "original future accepts earned choice");
          if (copy.snapshot().phase == gp::Phase::choosing_upgrade)
            require(copy.choose_upgrade(0), "restored future accepts earned choice");
          gp::Control c{{std::sin(static_cast<float>(future) * .04F), .1F}, future % 137 == 0};
          run.advance(dt, c);
          copy.advance(dt, c);
          require(run.save_state() == copy.save_state(), "body avoidance preserves exact future after pause, cold restore and Dash");
        }
      }
    }
    require(checkpointed, "real public reproduction reaches actual crowded military encounters");
  }
  std::cout << "PASS fresh public spacing reproduction and exact paused/restored futures\n";
}
} // namespace
int main() {
  bounded_approach_and_planted_sources();
  locked_aim_and_recovery();
  capture_projection_and_edge_return();
  ingress_and_random_stream();
  public_reproduction_and_restore();
}
