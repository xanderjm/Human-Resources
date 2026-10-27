#include "run_state.h"
#include "test_support.h"

#include <array>

namespace gp = human_resources::gameplay;
using test_support::require_near;
namespace {
constexpr float dt = 1.F / 60.F;
gp::Game::State flight(float speed) {
  gp::Game::State s;
  s.view.phase = gp::Phase::active;
  s.view.stats.captures = speed > 3 ? 100 : 0;
  s.sync();
  s.view.velocity = {0, s.view.forward_speed};
  s.flight_bounds = {-200, 200, -200, 200};
  return s;
}
void tick(gp::Game::State& s, gp::Vec2 input) {
  s.view.runner_distance += s.view.forward_speed * dt;
  s.move(dt, {input, false});
}
void active_scroll_ramp_cannot_move_an_idle_ship() {
  gp::Game game;
  game.start({.seed=514});
  game.state_->actors.clear();
  for (unsigned frame = 0; frame < 90; ++frame) {
    game.advance(dt);
    const auto& run = game.snapshot();
    require_near(run.velocity.z, run.forward_speed, .000002F,
        "active world acceleration consumes none of the pilot steering budget");
    require_near(run.player.z - run.runner_distance, 0, .00001F,
        "an idle ship holds its visible station throughout the actual scroll ramp");
  }
}
void scroll_changes_preserve_direction_and_response() {
  for (bool capturing : {false, true}) for (unsigned rank : {0U, 3U}) {
    for (float strength : {.5F, 1.F}) {
      std::array<gp::Game::State, 4> runs{flight(3), flight(3), flight(3), flight(3)};
      const std::array<gp::Vec2, 4> directions{{{1,0}, {-1,0}, {0,1}, {0,-1}}};
      for (auto& s : runs) {
        if (capturing) s.view.beams.resize(1);
        s.view.modules[static_cast<unsigned>(gp::Module::vector_thrusters)] = static_cast<std::uint8_t>(rank);
      }
      for (unsigned frame = 0; frame < 90; ++frame) {
        for (unsigned i = 0; i < runs.size(); ++i) {
          auto& s = runs[i];
          if (frame == 15) {
            const float pilot = s.view.velocity.z - s.view.forward_speed;
            s.view.stats.captures = 100;
            s.sync();
            require_near(s.view.velocity.z - s.view.forward_speed, pilot, .000002F,
                "earned scroll acceleration cannot change pilot velocity");
          }
          tick(s, gp::detail::scaled(directions[i], frame < 45 ? strength : frame < 65 ? -strength : 0));
          const float pilot = i < 2 ? s.view.velocity.x : s.view.velocity.z - s.view.forward_speed;
          const float station = i < 2 ? s.view.player.x : s.view.player.z - s.view.runner_distance;
          const float sign = i % 2 ? -1.F : 1.F;
          require_near(pilot * sign, runs[0].view.velocity.x, .00001F,
              "forward/backward/sideways acceleration, reversal and braking match at either scroll speed");
          require_near(station * sign, runs[0].view.player.x, .0001F,
              "equal input covers equal visible distance regardless of scroll, capture load or thrusters");
        }
      }
    }
  }
}
void edges_absorb_only_pilot_motion() {
  for (float speed : {3.F, 6.8F}) for (float sign : {-1.F, 1.F}) {
    auto side = flight(speed), vertical = flight(speed);
    side.view.player.x = sign * 200;
    vertical.view.player.z = sign * 200;
    for (unsigned frame = 0; frame < 12; ++frame) {
      tick(side, {sign, 0}); tick(vertical, {0, sign});
    }
    require_near(vertical.view.velocity.z, vertical.view.forward_speed, .00001F,
        "vertical boundary stops steering while retaining automatic scroll carry");
    for (unsigned frame = 0; frame < 12; ++frame) {
      tick(side, {}); tick(vertical, {});
      require_near(vertical.view.player.z - vertical.view.runner_distance, side.view.player.x, .0001F,
          "releasing at either vertical edge cannot drift the ship inward");
    }
    for (unsigned frame = 0; frame < 12; ++frame) {
      tick(side, {-sign, 0}); tick(vertical, {0, -sign});
      require_near(vertical.view.velocity.z - vertical.view.forward_speed, side.view.velocity.x, .00001F,
          "steering away from any edge has the same immediate response");
    }
  }
}
}
int main() {
  active_scroll_ramp_cannot_move_an_idle_ship();
  scroll_changes_preserve_direction_and_response();
  edges_absorb_only_pilot_motion();
}
