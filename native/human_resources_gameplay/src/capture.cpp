#include "run_state.h"

namespace human_resources::gameplay {
using namespace detail;

unsigned Game::State::used_beam_slots() const {
  unsigned used = 0;
  for (const auto& beam : view.beams) {
    used += beam.vehicle ? 2U : 1U;
  }
  return used;
}
bool Game::State::capture_eligible(const Actor& target) const {
  if (target.removed || target.view.kind == EntityKind::boss) {
    return false;
  }
  if (biological(target.view.kind)) {
    return true;
  }
  const auto winch = rank(Module::heavy_winch);
  if (winch == 0 || vehicle_capture_cooldown > 0) {
    return false;
  }
  const float fraction = target.view.hull / target.view.maximum_hull;
  if (target.view.kind == EntityKind::jeep) {
    return fraction <= (evolved(Evolution::hostile_acquisition) ? 1 : tuning::jeep_capture_threshold[winch - 1] / 100);
  }
  return winch == 3 && fraction <= (evolved(Evolution::hostile_acquisition) ? tuning::evolution_tank_threshold : tuning::tank_capture_threshold);
}
float Game::State::capture_seconds(const Actor& target) const {
  if (vehicle(target.view.kind)) {
    const float bonus = static_cast<float>(rank(Module::heavy_winch) - 1) * tuning::vehicle_capture_speed_step;
    const float seconds = tuning::vehicle_capture_seconds / (1 + bonus);
    return target.view.kind == EntityKind::tank ? std::max(seconds, tuning::tank_capture_minimum) : seconds;
  }
  const float bonus = permanent(3) + research(Research::capture_speed) / 100 +
                      (capture_burst > 0 ? tuning::splitter_burst : 0) + (vehicle_burst > 0 ? tuning::vehicle_bio_burst : 0);
  const float soldier = target.view.kind == EntityKind::soldier ? tuning::soldier_capture_seconds_multiplier : 1;
  const float seconds = std::max(tuning::capture_minimum_seconds, tuning::base_capture_seconds * soldier / (1 + bonus));
  return seconds / (view.harvest_overdrive_seconds > 0 ? tuning::harvest_overdrive_multiplier : 1.F);
}
void Game::State::capture(float dt) {
  const float radius2 = view.capture_radius * view.capture_radius;
  for (auto& a : actors) {
    a.lifted = false;
    if (rank(Module::tractor_lens) == 3 && biological(a.view.kind) && !a.removed) {
      const float extension = view.capture_radius * (1 + tuning::lens_pull_extension);
      if (distance_squared(view.player, a.view.position) < extension * extension) {
        const float normal_speed = a.view.kind == EntityKind::soldier ? tuning::military_movement_speed[0] : tuning::biological_movement_speed[static_cast<std::size_t>(a.view.kind)];
        const float pull = std::min(tuning::lens_pull_speed, normal_speed * tuning::lens_pull_movement_cap);
        a.view.position = add(a.view.position, scaled(direction({view.player.x - a.view.position.x, view.player.z - a.view.position.z}), pull * dt));
      }
    }
  }
  for (auto& beam : view.beams) {
    auto* target = actor(beam.target_id);
    if (!target || !capture_eligible(*target)) {
      if (target && vehicle(target->view.kind) && target->view.capture_progress > 0) {
        target->view.capture_progress = 0;
        target->sweep_progress = 0;
        target->view.fire_recovery_seconds = tuning::capture_return_seconds;
      }
      beam.target_id = 0;
      continue;
    }
    if (distance_squared(view.player, target->view.position) > radius2) {
      target->grace += dt;
      if (target->grace > static_cast<float>(rank(Module::tractor_lens)) * tuning::lens_grace_step) {
        if (vehicle(target->view.kind) && target->view.capture_progress > 0)
          target->view.fire_recovery_seconds = tuning::capture_return_seconds;
        target->view.capture_progress = 0;
        target->sweep_progress = 0;
        beam.target_id = 0;
      }
      continue;
    }
    target->grace = 0;
    target->lifted = true;
  }
  std::erase_if(view.beams, [](const BeamView& b) { return b.target_id == 0; });
  while (used_beam_slots() < view.beam_capacity) {
    Actor* closest = nullptr;
    float best = radius2;
    for (auto& a : actors) {
      if (!capture_eligible(a) || a.lifted) {
        continue;
      }
      if (vehicle(a.view.kind) && used_beam_slots() + 2 > view.beam_capacity) {
        continue;
      }
      // An out-of-range retained beam still owns its target and slots.
      if (std::any_of(view.beams.begin(), view.beams.end(), [&a](const BeamView& b) { return b.target_id == a.view.id; })) {
        continue;
      }
      const float distance = distance_squared(view.player, a.view.position);
      if (distance <= best) {
        closest = &a;
        best = distance;
      }
    }
    if (!closest) {
      break;
    }
    closest->lifted = true;
    closest->grace = 0;
    view.beams.push_back({closest->view.id, closest->view.position, closest->view.capture_progress, vehicle(closest->view.kind)});
  }
  for (auto& beam : view.beams) {
    auto* target = actor(beam.target_id);
    if (!target) {
      beam.target_id = 0;
      continue;
    }
    if (beam.vehicle && vehicle_capture_cooldown > 0) {
      if (vehicle(target->view.kind) && target->view.capture_progress > 0)
        target->view.fire_recovery_seconds = tuning::capture_return_seconds;
      target->view.capture_progress = 0;
      target->lifted = false;
      beam.target_id = 0;
      continue;
    }
    beam.target = target->view.position;
    if (!target->lifted) {
      continue;
    }
    const Vec2 pilot_velocity{view.velocity.x, view.velocity.z - view.forward_speed};
    const float core = view.capture_radius * tuning::harvest_core_radius_fraction;
    const bool sweep = biological(target->view.kind) &&
        length_squared(pilot_velocity) >= tuning::harvest_minimum_pilot_speed * tuning::harvest_minimum_pilot_speed &&
        distance_squared(view.player, target->view.position) <= core * core;
    const float progress = dt / capture_seconds(*target) * (sweep ? tuning::harvest_sweep_multiplier : 1.F);
    // Qualification follows actual participation, so faster earned builds
    // cannot complete before an arbitrary minimum exposure time elapses.
    if (sweep) target->sweep_progress = std::min(tuning::harvest_sweep_fraction, target->sweep_progress + progress);
    target->view.capture_progress += progress;
    beam.progress = std::min(1.0F, target->view.capture_progress);
    if (target->view.capture_progress >= 1) {
      captured(*target);
      beam.target_id = 0;
    }
  }
  std::erase_if(view.beams, [](const BeamView& b) { return b.target_id == 0; });
}
void Game::State::captured(Actor& target) {
  present(PresentationEventKind::capture, target.view.position, target.view.id);
  target.removed = true;
  if (vehicle(target.view.kind)) {
    ++view.stats.vehicle_captures;
    view.xp += target.view.kind == EntityKind::tank ? tuning::tank_kill_or_capture_xp : tuning::jeep_kill_or_capture_xp;
    score += tuning::vehicle_score;
    vehicle_capture_cooldown = tuning::vehicle_capture_cooldown;
    return;
  }
  ++view.stats.captures;
  if (target.sweep_progress >= tuning::harvest_sweep_fraction) {
    present(PresentationEventKind::sweep_capture, target.view.position, target.view.id);
    // Overdrive is an earned burst, not a permanently refreshable mode.
    // Its fast captures still pay normally; a new chain starts after it ends.
    if (view.harvest_overdrive_seconds <= 0) {
      ++view.harvest_chain;
      view.harvest_chain_seconds = tuning::harvest_chain_window_seconds;
      if (view.harvest_chain >= tuning::harvest_chain_target) {
        view.harvest_chain = 0;
        view.harvest_chain_seconds = 0;
        view.harvest_overdrive_seconds = tuning::harvest_overdrive_seconds;
        present(PresentationEventKind::harvest_overdrive, view.player);
      }
    }
  }
  ++streak;
  view.xp += tuning::biological_capture_xp;
  const auto bounty = rank(Module::bounty_scanner);
  const float steps = std::floor(static_cast<float>(streak) / tuning::bounty_streak_interval);
  const float streak_bonus = bounty == 0 ? 0 : std::min(tuning::bounty_streak_cap + static_cast<float>(bounty - 1) * 0.05F, steps * (tuning::bounty_streak_base + static_cast<float>(bounty - 1) * 0.01F));
  const float base_score = target.view.kind == EntityKind::soldier ? tuning::biological_score[0] : tuning::biological_score[static_cast<std::size_t>(target.view.kind)];
  score += base_score * (1 + research(Research::specimen_value) / 100 + static_cast<float>(bounty) * tuning::bounty_step + streak_bonus);
  if (rank(Module::beam_splitter) == 3) {
    capture_burst = tuning::splitter_burst_seconds;
  }
  if (rank(Module::vector_thrusters) == 3) {
    movement_burst = tuning::movement_burst_seconds;
  }
  if (evolved(Evolution::employee_benefits)) {
    drone_burst = tuning::benefits_duration;
  }
  const float hull_before_repair = view.hull, shield_before_repair = view.shield;
  const auto repair = rank(Module::repair_swarm);
  if (repair > 0 && view.stats.captures % static_cast<unsigned>(tuning::repair_capture_interval) == 0) {
    view.hull = std::min(view.maximum_hull, view.hull + tuning::repair_base + static_cast<float>(repair - 1));
  }
  if (repair >= 2) {
    const float restored = std::min(static_cast<float>(repair - 1), std::max(0.0F, tuning::capture_shield_restoration_per_second_cap - capture_shield_budget));
    capture_shield_budget += restored;
    view.shield = std::min(view.maximum_shield, view.shield + restored);
  }
  if (view.hull > hull_before_repair || view.shield > shield_before_repair)
    present(PresentationEventKind::repair, view.player);
  if (target.view.kind == EntityKind::soldier && rank(Module::emp_ring) == 3 && disable_cooldown <= 0) {
    disable_cooldown = tuning::pulse_disable_cooldown;
    for (auto& enemy : actors) {
      if (military(enemy.view.kind) && enemy.view.kind != EntityKind::boss &&
          distance_squared(view.player, enemy.view.position) <= tuning::emergency_disable_radius * tuning::emergency_disable_radius) {
        enemy.view.disabled_seconds = tuning::pulse_disable_seconds;
      }
    }
  }
}
} // namespace human_resources::gameplay
