#include "run_state.h"
#include "test_support.h"
#include <iostream>
namespace gp = human_resources::gameplay;
namespace tune = gp::tuning;
using test_support::require;
using test_support::require_near;
namespace {
constexpr float dt = 1.F / 60.F;
gp::Game::State harvest() {
  gp::Game::State s;
  s.view.phase = gp::Phase::active;
  s.view.hull = tune::base_hull;
  s.view.shield = tune::base_shield;
  s.view.velocity = {0, tune::forward_scroll_metres_per_second};
  s.sync();
  return s;
}
void target(gp::Game::State& s, float x = 0) {
  std::erase_if(s.actors, [](const auto& a) { return a.removed; });
  gp::detail::Actor a;
  a.view.id = s.next_id++;
  a.view.kind = gp::EntityKind::cow;
  a.view.position = {x, 0};
  a.view.hull = a.view.maximum_hull = 1;
  s.actors.push_back(a);
}
float capture_one(gp::Game::State& s) {
  const auto captures = s.view.stats.captures;
  unsigned frames = 0;
  while (s.view.stats.captures == captures && frames++ < 600) {
    s.view.stats.active_seconds += dt;
    s.advance_timers(dt);
    s.capture(dt);
  }
  require(s.view.stats.captures == captures + 1, "real capture completes");
  return static_cast<float>(frames) * dt;
}
void locked_capture_keeps_progress_and_pickup_range() {
  for (unsigned rank = 0; rank <= 3; ++rank) {
    auto s = harvest();
    s.view.modules[static_cast<std::size_t>(gp::Module::tractor_lens)] = static_cast<std::uint8_t>(rank);
    s.sync();
    const float radius = s.view.capture_radius;
    target(s, radius + .01F);
    s.capture(dt);
    require(s.view.beams.empty(), "each lens rank rejects acquisition beyond its actual pickup radius");
    s.actors.front().view.position.x = radius;
    s.capture(dt);
    require(s.view.beams.size() == 1, "the exact pickup radius can acquire a target");
    const auto id = s.view.beams.front().target_id;
    const float progress = s.actors.front().view.capture_progress;
    s.view.player = {0, 100};
    s.view.runner_distance = 100;
    target(s); // A later near target must wait for the occupied slot.
    s.actors.back().view.position.z = 100;
    for (unsigned frame = 0; frame < 60; ++frame) {
      s.enemies(dt); s.capture(dt);
      require(s.view.beams.size() == 1 && s.view.beams.front().target_id == id,
              "range loss and offscreen retirement cannot release or replace an occupied beam");
      require(s.actors.front().lifted && s.actors.back().view.capture_progress == 0,
              "the original target stays lifted and owns its slot");
    }
    require_near(s.actors.front().view.capture_progress, progress + 1.F / s.capture_seconds(s.actors.front()), .00001F,
                 "out-of-range capture continues slowly at the unchanged ordinary rate");
    capture_one(s);
    require(s.view.stats.captures == 1 && s.view.xp == tune::biological_capture_xp,
            "the locked target completes once with its ordinary XP reward");
    s.capture(dt);
    require(s.view.beams.size() == 1 && s.view.beams.front().target_id == s.actors.back().view.id,
            "completion frees the slot for a new in-range pickup");
  }
  auto s = harvest(); target(s); s.view.velocity.x = 2;
  s.capture(dt);
  const float exposure = s.actors.front().sweep_progress;
  const float progress = s.actors.front().view.capture_progress;
  s.view.player.x = 20;
  s.capture(dt);
  require(s.actors.front().sweep_progress == exposure && s.view.harvest_chain == 0,
          "leaving the core retains earned exposure without inventing sweep qualification");
  require_near(s.actors.front().view.capture_progress - progress, dt / s.capture_seconds(s.actors.front()), .000001F,
               "leaving the core removes its speed bonus while the beam keeps capturing");
}
void locked_biological_save_and_future() {
  gp::Game original; original.start({.seed = 514});
  auto& s = *original.state_;
  s.actors.clear(); s.view.beams.clear(); target(s);
  s.capture(dt);
  const auto target_id = s.actors.front().view.id;
  s.view.player.z = s.view.runner_distance + tune::player_forward_allowance;
  s.actors.front().view.position.z = s.view.runner_distance - tune::enemy_retire_behind - 1;
  s.sync();
  require(s.view.beams.size() == 1, "the original pickup owns the out-of-field target");
  original.pause(); const auto paused = original.save_state();
  require(!paused.empty(), "locked out-of-field biological capture is checkpointable");
  original.advance(100, {{1, 1}, true});
  require(original.save_state() == paused, "paused out-of-range beam cannot advance");
  gp::Game restored;
  require(restored.restore_state(paused) == gp::SaveStateIssue::none && restored.save_state() == paused,
          "cold restore retains exact lock, slot and progress");
  original.resume(); restored.resume();
  for (unsigned frame = 0; frame < 240; ++frame) {
    if (original.snapshot().phase == gp::Phase::choosing_upgrade) {
      require(original.choose_upgrade(0) && restored.choose_upgrade(0), "matching earned choices resume the capture");
    }
    original.advance(dt, {{1, 0}, frame == 0});
    restored.advance(dt, {{1, 0}, frame == 0});
    const auto bytes = original.save_state();
    require(!bytes.empty() && bytes == restored.save_state(), "movement and Dash retain identical locked-capture futures");
  }
  require(original.snapshot().stats.captures >= 1, "the out-of-field target completes after movement and restore");
  unsigned events = 0;
  for (const auto& event : original.snapshot().presentation_events)
    if (event.sequence && event.actor_id == target_id && event.kind == gp::PresentationEventKind::capture) ++events;
  require(events == 1, "the original target emits exactly one capture reward");
}
void earned_levels_accelerate_every_lift() {
  require_near(gp::level_capture_speed_bonus(0), 0, 0, "invalid zero level cannot underflow the bonus");
  for (const auto kind : {gp::EntityKind::sheep, gp::EntityKind::cow, gp::EntityKind::jeep, gp::EntityKind::tank}) {
    auto s = harvest();
    target(s); s.actors.front().view.kind = kind;
    s.view.modules[static_cast<unsigned>(gp::Module::heavy_winch)] = 3;
    const auto& a = s.actors.front();
    float previous = s.capture_seconds(a);
    for (unsigned level = 2; level <= 10; ++level) {
      s.view.stats.level = level;
      const float seconds = s.capture_seconds(a);
      const float winch = kind >= gp::EntityKind::jeep ? .4F : 0.F;
      const float minimum = kind == gp::EntityKind::tank ? tune::tank_capture_minimum : tune::capture_minimum_seconds;
      const float expected = std::max(minimum, tune::base_capture_seconds * gp::abduction_profile(kind).lift_load /
          (1 + winch + static_cast<float>(level - 1) * .08F));
      require_near(seconds, expected, .00001F, "each earned level adds eight points of actual base lift rate");
      require(seconds <= previous && (seconds < previous || seconds == minimum), "successive levels accelerate lifts until the existing safety floor");
      previous = seconds;
    }
    s.view.stats.level = 100000;
    require(s.capture_seconds(a) > 0, "long endless runs retain positive lift durations");
    if (kind == gp::EntityKind::tank)
      require_near(s.capture_seconds(a), tune::tank_capture_minimum, 0, "level bonuses preserve the tank minimum");
    if (kind < gp::EntityKind::jeep)
      require_near(s.capture_seconds(a), tune::capture_minimum_seconds, 0, "levels preserve the biological minimum before Overdrive");
  }
  auto s = harvest(); target(s); s.view.stats.level = 5;
  s.setup.permanent_ranks[3] = 5;
  s.view.research[static_cast<unsigned>(gp::Research::capture_speed)] = 18;
  const float base = tune::base_capture_seconds * gp::abduction_profile(gp::EntityKind::cow).lift_load;
  require_near(s.capture_seconds(s.actors.front()), base / (1 + .32F + .15F + .18F), .00001F,
      "Hangar, level and research gains add without compounding");
  s.capture_burst = 2; s.vehicle_burst = 2; s.view.harvest_overdrive_seconds = 2;
  require_near(s.capture_seconds(s.actors.front()), std::max(tune::capture_minimum_seconds,
      base / (1 + .32F + .15F + .18F + tune::splitter_burst + tune::vehicle_bio_burst)) / tune::harvest_overdrive_multiplier,
      .00001F, "temporary bursts keep their ordinary headroom and Overdrive rule");
  for (unsigned rarity = 0; rarity < 3; ++rarity) {
    s.view.research[static_cast<unsigned>(gp::Research::capture_speed)] = 95;
    const auto offer = s.make_choice(false, static_cast<unsigned>(gp::Research::capture_speed), rarity);
    require_near(offer.gain, 5, 0, "stronger capture research still exposes only actual remaining headroom");
    s.apply(offer);
    require(!s.research_eligible(static_cast<unsigned>(gp::Research::capture_speed)), "maxed capture research leaves the pool");
  }
}
void lens_copy_describes_pickup() {
  auto s = harvest();
  for (unsigned rank = 1; rank <= 3; ++rank) {
    const auto choice = s.make_choice(true, static_cast<std::size_t>(gp::Module::tractor_lens), 0);
    require(choice.conditions.find("active beams stay locked") != std::string::npos &&
        choice.description.find("grace") == std::string::npos, "each lens offer describes permanent lock without obsolete grace");
    s.view.modules[static_cast<std::size_t>(gp::Module::tractor_lens)] = static_cast<std::uint8_t>(rank);
    s.sync();
  }
}
}
int main() {
  locked_capture_keeps_progress_and_pickup_range();
  locked_biological_save_and_future();
  lens_copy_describes_pickup();
  earned_levels_accelerate_every_lift();
  std::cout << "Locked capture range, duration, slots, sweep and continuation contracts passed\n";
}
