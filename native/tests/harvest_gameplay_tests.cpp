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
void sweep_rewards_real_pilot_motion() {
  auto idle = harvest(); target(idle);
  const float idle_seconds = capture_one(idle);
  require_near(idle_seconds, tune::base_capture_seconds, dt * 2, "automatic forward scroll retains ordinary capture duration");
  require(idle.view.harvest_chain == 0, "automatic runner motion cannot earn sweep chain");
  auto sweep = harvest(); target(sweep);
  sweep.view.velocity.x = tune::harvest_minimum_pilot_speed + .5F;
  const float sweep_progress = capture_one(sweep);
  require(sweep_progress < idle_seconds * .3F && sweep.view.harvest_chain == 1,
          "close moving capture is four times faster and earns a chain step");
  auto far = harvest(); target(far, 3.F);
  far.view.velocity.x = 2;
  require_near(capture_one(far), idle_seconds, dt, "moving in outer beam radius cannot claim a close sweep");
  require(far.view.harvest_chain == 0, "outer-range capture retains ordinary reward");
  auto flick = harvest(); target(flick);
  flick.view.velocity.x = 2;
  for (unsigned i = 0; i < 4; ++i) flick.capture(dt);
  flick.view.velocity.x = 0;
  capture_one(flick);
  require(flick.view.harvest_chain == 0, "a tiny motion flick cannot claim sustained close-pass exposure");
}
void chain_overdrive_and_expiry() {
  auto s = harvest();
  s.view.velocity.x = 2;
  for (unsigned i = 0; i < tune::harvest_chain_target; ++i) {
    target(s); capture_one(s);
    require(s.view.harvest_chain == (i + 1) % tune::harvest_chain_target, "only actual sweep captures advance chain");
  }
  require_near(s.view.harvest_overdrive_seconds, tune::harvest_overdrive_seconds, .0001F, "third close capture earns bounded Overdrive");
  require_near(s.fire_multiplier(), tune::harvest_overdrive_multiplier, .0001F, "Overdrive increases actual weapon cadence");
  s.view.velocity.x = 0;
  target(s, 3.F);
  const float boosted = capture_one(s);
  require(boosted < 1.4F && boosted > 1.3F, "earned Overdrive increases actual outer-range capture cadence");
  s.advance_timers(10);
  require(s.view.harvest_chain == 0 && s.view.harvest_overdrive_seconds == 0, "active-time expiry removes chain and Overdrive");
  require_near(s.fire_multiplier(), 1, .0001F, "expired Overdrive restores normal weapon cadence");
  target(s); s.view.velocity.x = 2; capture_one(s);
  s.advance_timers(tune::harvest_chain_window_seconds + dt);
  require(s.view.harvest_chain == 0 && s.view.harvest_chain_seconds == 0, "missed active-time chain window resets progress");
  unsigned triggers = 0;
  for (const auto& event : s.view.presentation_events)
    if (event.sequence && event.kind == gp::PresentationEventKind::harvest_overdrive) ++triggers;
  require(triggers == 1, "one third-capture trigger emits exactly one authoritative reward event");
}
void capture_upgrades_preserve_skill_rewards() {
  auto s = harvest();
  s.setup.unlocked_blueprints = 0xffffU;
  s.setup.permanent_ranks[3] = 5;
  s.view.research[static_cast<std::size_t>(gp::Research::capture_speed)] = 100;
  s.view.modules[static_cast<std::size_t>(gp::Module::beam_splitter)] = 3;
  s.view.modules[static_cast<std::size_t>(gp::Module::heavy_winch)] = 3;
  s.view.modules[static_cast<std::size_t>(gp::Module::laser_array)] = 1;
  s.capture_burst = s.vehicle_burst = 5;
  s.view.harvest_overdrive_seconds = tune::harvest_overdrive_seconds;
  s.view.velocity.x = 2;
  s.sync();
  const float burst_before = s.view.harvest_overdrive_seconds;
  for (unsigned i = 0; i < tune::harvest_chain_target; ++i) {
    target(s);
    const float elapsed = capture_one(s);
    require(elapsed < .15F, "legal strong capture build completes a fast close sweep");
    require(s.view.harvest_chain == 0 && s.view.harvest_chain_seconds == 0,
            "strong moving captures cannot precharge another chain during Overdrive");
  }
  require(s.view.harvest_overdrive_seconds < burst_before,
          "strong capture build cannot refresh its active Overdrive");
  s.advance_timers(tune::harvest_overdrive_seconds);
  for (unsigned i = 0; i < tune::harvest_chain_target; ++i) {
    target(s); capture_one(s);
    require(s.view.harvest_chain == (i + 1) % tune::harvest_chain_target,
            "strong capture build rearms only from fresh moving captures after the burst");
  }
  require_near(s.view.harvest_overdrive_seconds, tune::harvest_overdrive_seconds, .0001F,
               "third fresh strong capture earns the next distinct Overdrive");
  s.advance_timers(tune::harvest_overdrive_seconds);
  target(s); s.capture(dt);
  s.view.velocity.x = 0;
  capture_one(s);
  require(s.view.harvest_chain == 0,
          "one-frame flick cannot earn a sweep even with the strongest capture bonuses");
}
void burst_expiry_boundary() {
  for (const bool expires : {false, true}) {
    auto s = harvest();
    s.view.velocity.x = 2;
    s.view.harvest_overdrive_seconds = expires ? dt * .5F : dt * 1.5F;
    target(s);
    s.actors.front().view.capture_progress = .99F;
    s.actors.front().sweep_progress = tune::harvest_sweep_fraction;
    s.advance_timers(dt);
    s.capture(dt);
    require(s.view.stats.captures == 1 && s.view.xp > 0,
            "a sweep completing on either side of burst expiry retains its ordinary capture reward");
    require(s.view.harvest_chain == (expires ? 1U : 0U),
            "only sweeps completing after active-time burst expiry start the next chain");
    require(s.view.harvest_overdrive_seconds < dt,
            "expiry-boundary capture cannot restart the old burst");
  }
}
void lost_capture_clears_exposure() {
  auto s = harvest(); target(s); s.view.velocity.x = 2;
  s.capture(dt);
  require(s.actors.front().sweep_progress > 0, "moving close beam records actual exposure");
  s.view.player.x = 10;
  s.capture(dt);
  require(s.actors.front().sweep_progress == 0 && s.actors.front().view.capture_progress == 0,
          "broken ordinary beam resets both capture progress and sweep qualification");
}
void save_pause_and_future() {
  for (const bool active_burst : {false, true}) {
    gp::Game original;
    original.start({.seed = 514});
    // A fixed, explicitly synthetic skill state isolates codec/timer coverage.
    auto& state = *original.state_;
    state.view.harvest_chain = active_burst ? 0U : 2U;
    state.view.harvest_chain_seconds = active_burst ? 0.F : 3.F;
    state.view.harvest_overdrive_seconds = active_burst ? 1.F : 0.F;
    state.actors.front().view.capture_progress = .25F;
    state.actors.front().sweep_progress = .1F;
    const auto bytes = original.save_state();
    require(!bytes.empty(), "valid harvest skill state is checkpointable");
    gp::Game restored;
    require(restored.restore_state(bytes) == gp::SaveStateIssue::none && restored.save_state() == bytes,
            "chain, boost and per-target exposure survive exact fieldwise restore");
    restored.pause();
    const auto paused = restored.save_state();
    restored.advance(100, {{1, 0}, false});
    require(restored.save_state() == paused, "pause cannot consume a skill chain or earned Overdrive");
    restored.resume();
    for (unsigned i = 0; i < 120; ++i) {
      original.advance(dt, {{.7F, .3F}, false});
      restored.advance(dt, {{.7F, .3F}, false});
      require(restored.save_state() == original.save_state(), "restored skill authority has the same future");
    }
    original.start();
    require(original.snapshot().harvest_chain == 0 && original.snapshot().harvest_chain_seconds == 0 &&
            original.snapshot().harvest_overdrive_seconds == 0, "new run cannot inherit a harvest reward");
  }
}
void starter_evade_and_countervolley() {
  auto s = harvest();
  s.last_direction = {1, 0};
  s.laser_clock = 10;
  s.move(dt, {{}, true});
  require_near(s.view.player.x, tune::starter_dash_distance, .0001F, "fresh ship can evade without a random module");
  require_near(s.view.dash_cooldown_seconds, tune::starter_dash_cooldown, .0001F, "starter evade has its bounded cooldown");
  require_near(s.view.invulnerability_seconds, tune::starter_dash_invulnerability, .0001F, "starter evade provides brief actual protection");
  const float x = s.view.player.x;
  s.move(dt, {{}, true});
  require_near(s.view.player.x, x, .0001F, "held/repeated dash cannot bypass its cooldown");
  gp::detail::Actor enemy;
  enemy.view.id = s.next_id++;
  enemy.view.kind = gp::EntityKind::soldier;
  enemy.view.infantry_role = gp::InfantryRole::rifleman;
  enemy.view.position = {0, 5};
  enemy.view.hull = enemy.view.maximum_hull = 100;
  s.actors.push_back(enemy);
  s.combat(dt);
  require(s.actors.front().view.hull < 100 && !s.view.shots.empty(),
          "accepted evade immediately counters through the existing weapon and target rules");
  s.view.invulnerability_seconds = tune::starter_dash_invulnerability;
  const float hull = s.view.hull, shield = s.view.shield;
  s.hurt(20);
  require(s.view.hull == hull && s.view.shield == shield, "actual incoming damage is avoided during evade protection");
  s.advance_timers(tune::starter_dash_invulnerability + dt);
  s.hurt(20);
  require(s.view.hull < hull || s.view.shield < shield, "evade protection expires rather than granting permanent safety");
  for (unsigned rank = 1; rank <= 3; ++rank) {
    auto upgraded = harvest();
    upgraded.view.modules[static_cast<std::size_t>(gp::Module::phase_drive)] = static_cast<std::uint8_t>(rank);
    upgraded.last_direction = {1, 0};
    upgraded.move(dt, {{}, true});
    require_near(upgraded.view.player.x, tune::dash_distance, .0001F, "Phase Drive upgrades actual evade distance");
    require(upgraded.view.invulnerability_seconds > tune::starter_dash_invulnerability &&
            upgraded.view.dash_cooldown_seconds < tune::starter_dash_cooldown,
            "every Phase Drive rank improves starter protection and reuse time");
  }
  auto edge = harvest();
  edge.view.player.x = tune::field_half_width - .1F;
  edge.last_direction = {1, 0};
  edge.move(dt, {{}, true});
  require(edge.view.player.x <= tune::field_half_width, "starter evade cannot escape the playable field");
  gp::Game game;
  game.start(); game.advance(dt, {{1, 0}, true});
  const auto bytes = game.save_state();
  gp::Game restored;
  require(restored.restore_state(bytes) == gp::SaveStateIssue::none && restored.save_state() == bytes,
          "accepted starter dodge cooldown/protection survives exact continuation");
  game.pause();
  const auto paused = game.save_state();
  game.advance(10, {{1, 0}, true});
  require(game.save_state() == paused, "paused dash cannot move, fire, or consume cooldown time");
}
void evade_preserves_existing_protection() {
  for (unsigned rank = 0; rank <= 3; ++rank) {
    auto hit = harvest();
    hit.view.modules[static_cast<std::size_t>(gp::Module::phase_drive)] = static_cast<std::uint8_t>(rank);
    hit.hurt(1);
    const float hull = hit.view.hull, shield = hit.view.shield;
    hit.move(dt, {{1, 0}, true});
    require_near(hit.view.invulnerability_seconds, tune::hit_invulnerability_seconds, .0001F,
                 "starter and upgraded evade preserve longer hit protection");
    hit.advance_timers(.31F);
    hit.hurt(10);
    require(hit.view.hull == hull && hit.view.shield == shield,
            "evading after a hit cannot admit damage before the original hit protection ends");
    hit.advance_timers(tune::hit_invulnerability_seconds);
    hit.hurt(10);
    require(hit.view.hull < hull || hit.view.shield < shield, "preserved hit protection still expires");

    gp::Game game;
    game.start();
    auto& revived = *game.state_;
    revived.view.modules[static_cast<std::size_t>(gp::Module::phase_drive)] = static_cast<std::uint8_t>(rank);
    revived.view.phase = gp::Phase::dead;
    revived.view.hull = 0;
    revived.view.stats.active_seconds = 180;
    require(game.revive(), "ordinary eligible revive supplies actual recovery protection");
    const float revived_hull = revived.view.hull;
    revived.move(dt, {{1, 0}, true});
    require_near(revived.view.invulnerability_seconds, tune::revive_invulnerability_seconds, .0001F,
                 "starter and upgraded evade preserve longer revive protection");
    revived.advance_timers(.31F);
    const float recovered_shield = revived.view.shield;
    revived.hurt(10);
    require(revived.view.hull == revived_hull && revived.view.shield == recovered_shield,
            "evading after revive cannot cancel recovery protection");
    revived.advance_timers(tune::revive_invulnerability_seconds);
    const float expired_shield = revived.view.shield;
    revived.hurt(10);
    require(revived.view.hull < revived_hull || revived.view.shield < expired_shield,
            "preserved revive protection still expires");
  }
}
}
int main() {
  sweep_rewards_real_pilot_motion();
  chain_overdrive_and_expiry();
  capture_upgrades_preserve_skill_rewards();
  burst_expiry_boundary();
  lost_capture_clears_exposure();
  save_pause_and_future();
  starter_evade_and_countervolley();
  evade_preserves_existing_protection();
  std::cout << "Harvest movement, chain, reward, cancellation and continuation contracts passed\n";
}
