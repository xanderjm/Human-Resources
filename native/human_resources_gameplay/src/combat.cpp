#include "run_state.h"

namespace human_resources::gameplay {
using namespace detail;

float Game::State::damage_multiplier() const {
  const auto reserve = rank(Module::reserve_battery);
  const auto idle = view.beam_capacity - std::min(view.beam_capacity, used_beam_slots());
  const float per = reserve > 0 ? tuning::reserve_base + static_cast<float>(reserve - 1) * tuning::reserve_step : 0;
  const float bonus = std::min(static_cast<float>(idle) * per, 2 * per);
  return (1 + permanent(0)) * (1 + research(Research::weapon_damage) / 100 + bonus);
}
float Game::State::fire_multiplier() const {
  const auto overtime = rank(Module::overtime_reactor);
  const float per = overtime > 0 ? tuning::overtime_base + static_cast<float>(overtime - 1) * tuning::overtime_step : 0;
  const float bonus = std::min(static_cast<float>(view.beams.size()) * per, 3 * per);
  const float upgrades = std::min(tuning::firing_rate_maximum_base_multiplier, 1 + research(Research::fire_rate) / 100 + bonus);
  return upgrades * (view.harvest_overdrive_seconds > 0 ? tuning::harvest_overdrive_multiplier : 1.F);
}
void Game::State::combat(float dt) {
  laser_clock -= dt;
  drone_clock -= dt;
  pulse_clock -= dt;
  mirrorball_clock -= dt;
  if (laser_clock <= 0) {
    laser_fire();
    laser_clock += 1 / (tuning::base_laser_shots_per_second * fire_multiplier());
  }
  if (rank(Module::orbit_drones) > 0 && drone_clock <= 0) {
    drone_fire();
    drone_clock += 1 / (tuning::drone_rate * fire_multiplier());
  }
  if (rank(Module::emp_ring) > 0 && pulse_clock <= 0) {
    pulse();
    pulse_clock = tuning::pulse_cooldown_base - static_cast<float>(rank(Module::emp_ring) - 1);
  }
  if (evolved(Evolution::mirrorball) && mirrorball_clock <= 0) {
    mirrorball_clock = tuning::mirrorball_period;
    std::array<Actor*, 4> targets{};
    unsigned count = 0;
    for (auto& target : actors) {
      if (!target.removed && military(target.view.kind) && distance_squared(view.player, target.view.position) <= tuning::laser_range * tuning::laser_range) {
        targets[count++] = &target;
        if (count == targets.size()) {
          break;
        }
      }
    }
    if (count > 0) {
      const float damage = std::min(reflected_damage, tuning::mirrorball_damage_cap * (1 + research(Research::weapon_damage) / 100)) / static_cast<float>(count);
      for (unsigned i = 0; i < count; ++i) {
        hit(*targets[i], damage);
        if (damage > 0 && view.shots.size() < 64)
          view.shots.push_back({view.player, targets[i]->view.position, tuning::laser_flash_seconds, true, ShotKind::mirrorball});
      }
      if (damage > 0)
        present(PresentationEventKind::mirrorball, view.player, 0, tuning::laser_range);
      reflected_damage = 0;
    }
  }
}
void Game::State::laser_fire() {
  std::array<std::uint32_t, 4> targeted{};
  bool volley_announced = false;
  for (unsigned laser = 0; laser < view.laser_count; ++laser) {
    Actor* closest = nullptr;
    float best = tuning::laser_range * tuning::laser_range;
    for (auto& target : actors) {
      if (target.removed || !military(target.view.kind) || target.lifted) {
        continue;
      }
      if (std::find(targeted.begin(), targeted.end(), target.view.id) != targeted.end()) {
        continue;
      }
      const float distance = distance_squared(view.player, target.view.position);
      if (distance < best) {
        closest = &target;
        best = distance;
      }
    }
    // Independently mounted lasers can concentrate when fewer targets exist.
    if (!closest) {
      for (auto& target : actors) {
        if (!target.removed && military(target.view.kind) && !target.lifted &&
            distance_squared(view.player, target.view.position) < best) {
          closest = &target;
          best = distance_squared(view.player, target.view.position);
        }
      }
    }
    if (!closest) {
      continue;
    }
    if (!volley_announced) {
      present(PresentationEventKind::laser_volley, view.player);
      volley_announced = true;
    }
    targeted[laser] = closest->view.id;
    const float mount = laser == 0 ? 1 : tuning::added_laser_fraction;
    float focus = 1;
    if (rank(Module::laser_array) == 3) {
      closest->focused_hits = std::min(tuning::focus_hits, closest->focused_hits + 1);
      closest->focus_timer = tuning::focus_expiry;
      focus += tuning::focus_bonus * closest->focused_hits / tuning::focus_hits;
    }
    const float evolution = evolved(Evolution::office_shredder) ? 1 + tuning::evolution_laser_bonus : 1;
    const float damage = tuning::base_laser_damage * damage_multiplier() * mount * focus * evolution;
    if (view.shots.size() < 64) {
      view.shots.push_back({view.player, closest->view.position, tuning::laser_flash_seconds, false, ShotKind::laser, static_cast<std::uint8_t>(laser)});
    }
    hit(*closest, damage);
    secondary_hits(*closest, damage);
  }
}
void Game::State::secondary_hits(Actor& primary, float damage) {
  const Vec2 ray = {primary.view.position.x - view.player.x, primary.view.position.z - view.player.z};
  const auto ray_direction = direction(ray);
  const float primary_distance = std::sqrt(length_squared(ray));
  unsigned pierced = 0;
  unsigned arced = 0;
  const auto coil = rank(Module::arc_coil);
  const float chance = coil > 0 ? tuning::arc_chance_base + static_cast<float>(coil - 1) * tuning::arc_chance_step : 0;
  const bool arc_roll = coil > 0 && combat_unit() < chance;
  for (auto& target : actors) {
    if (target.removed || !military(target.view.kind) || target.view.id == primary.view.id || target.lifted) {
      continue;
    }
    const Vec2 offset = {target.view.position.x - view.player.x, target.view.position.z - view.player.z};
    const float along = offset.x * ray_direction.x + offset.z * ray_direction.z;
    const float cross = std::fabs(offset.x * ray_direction.z - offset.z * ray_direction.x);
    if (pierced < rank(Module::rail_lens) && along > primary_distance && along < tuning::laser_range && cross < tuning::pierce_alignment_metres) {
      hit(target, damage * tuning::pierce_fraction);
      ++pierced;
      if (view.shots.size() < 64) {
        view.shots.push_back({primary.view.position, target.view.position, tuning::laser_flash_seconds, true, ShotKind::rail});
      }
    }
    if (!target.removed && arc_roll && arced < coil && distance_squared(primary.view.position, target.view.position) <= tuning::arc_range * tuning::arc_range) {
      hit(target, damage * tuning::arc_fraction);
      ++arced;
      if (view.shots.size() < 64) {
        view.shots.push_back({primary.view.position, target.view.position, tuning::laser_flash_seconds, true, ShotKind::arc});
      }
    }
  }
}
void Game::State::drone_fire() {
  for (unsigned drone = 0; drone < rank(Module::orbit_drones); ++drone) {
    Actor* closest = nullptr;
    float best = tuning::laser_range * tuning::laser_range;
    for (auto& target : actors) {
      if (target.removed || !military(target.view.kind) || target.lifted) {
        continue;
      }
      const float distance = distance_squared(view.player, target.view.position);
      if (distance < best) {
        best = distance;
        closest = &target;
      }
    }
    if (closest) {
      const float benefits = drone_burst > 0 ? 1 + tuning::benefits_damage_bonus : 1;
      hit(*closest, tuning::drone_damage * damage_multiplier() * benefits);
      if (view.shots.size() < 64) {
        view.shots.push_back({view.player, closest->view.position, tuning::laser_flash_seconds, true, ShotKind::drone, static_cast<std::uint8_t>(drone)});
      }
    }
  }
}
void Game::State::pulse() {
  const float offset = static_cast<float>(rank(Module::emp_ring) - 1);
  const float radius = evolved(Evolution::storm_drain) ? tuning::storm_radius : tuning::pulse_radius_base + offset * tuning::pulse_radius_step;
  present(PresentationEventKind::emp_pulse, view.player, 0, radius);
  const float damage = (tuning::pulse_damage_base + offset * tuning::pulse_damage_step) * damage_multiplier();
  for (auto& target : actors) {
    if (!target.removed && military(target.view.kind) && distance_squared(view.player, target.view.position) <= radius * radius) {
      hit(target, damage);
      if (evolved(Evolution::storm_drain)) {
        target.slow_timer = tuning::storm_slow_seconds;
      }
      if (view.shots.size() < 64) {
        view.shots.push_back({view.player, target.view.position, tuning::laser_flash_seconds, true, ShotKind::emp});
      }
    }
  }
}
void Game::State::projectiles(float dt) {
  for (auto& bullet : bullets) {
    if (bullet.removed) {
      continue;
    }
    bullet.age += dt;
    bullet.view.position = add(bullet.view.position, scaled(bullet.view.velocity, dt));
    if (bullet.age > tuning::projectile_lifetime_seconds) {
      bullet.removed = true;
      continue;
    }
    const float radius = tuning::player_radius + bullet.view.radius;
    if (distance_squared(bullet.view.position, view.player) > radius * radius) {
      continue;
    }
    bullet.removed = true;
    if (view.invulnerability_seconds > 0) {
      continue;
    }
    if (evolved(Evolution::employee_benefits) && intercept_cooldown <= 0 && view.shield >= tuning::benefits_shield_cost) {
      view.shield -= tuning::benefits_shield_cost;
      intercept_cooldown = tuning::benefits_intercept_period;
      present(PresentationEventKind::intercept, bullet.view.position);
      continue;
    }
    const float chance = static_cast<float>(rank(Module::mirror_plating)) * tuning::reflect_step;
    if (view.shield > 0 && chance > 0 && combat_unit() < chance) {
      present(PresentationEventKind::reflect, bullet.view.position);
      if (auto* source = actor(bullet.source)) {
        hit(*source, bullet.view.damage);
        if (view.shots.size() < 64)
          view.shots.push_back({bullet.view.position, source->view.position, tuning::laser_flash_seconds, true, ShotKind::reflection});
      }
      reflected_damage = std::min(reflected_damage + bullet.view.damage, tuning::mirrorball_damage_cap * (1 + research(Research::weapon_damage) / 100));
      continue;
    }
    hurt(bullet.view.damage, DamageCause::projectile, bullet.source);
  }
}
void Game::State::hurt(float damage, DamageCause cause, std::uint32_t source_id) {
  if (view.invulnerability_seconds > 0 || view.hull <= 0) {
    return;
  }
  time_since_hit = 0;
  view.invulnerability_seconds = tuning::hit_invulnerability_seconds;
  const float absorbed = std::min(view.shield, damage);
  view.shield -= absorbed;
  if (absorbed > 0)
    present(PresentationEventKind::shield_hit, view.player, source_id, 0, cause);
  const float hull_damage = damage - absorbed;
  if (hull_damage > 0) {
    present(PresentationEventKind::hull_hit, view.player, source_id, 0, cause);
    view.hull = std::max(0.0F, view.hull - hull_damage);
    streak = 0;
  }
}
void Game::State::hit(Actor& enemy, float damage) {
  if (enemy.removed || damage <= 0) {
    return;
  }
  enemy.view.hull = std::max(0.0F, enemy.view.hull - damage);
  if (enemy.view.hull <= 0) {
    defeated(enemy);
  } else
    present(PresentationEventKind::impact, enemy.view.position, enemy.view.id);
}
void Game::State::defeated(Actor& enemy) {
  if (enemy.removed) {
    return;
  }
  enemy.removed = true;
  present(PresentationEventKind::defeat, enemy.view.position, enemy.view.id);
  const auto index = military_index(enemy.view.kind);
  score += tuning::enemy_score[index];
  switch (enemy.view.kind) {
  case EntityKind::soldier:
    ++view.stats.soldier_kills;
    view.xp += tuning::soldier_kill_xp;
    break;
  case EntityKind::jeep:
    ++view.stats.jeep_kills;
    view.xp += tuning::jeep_kill_or_capture_xp;
    break;
  case EntityKind::tank:
    ++view.stats.tank_kills;
    view.xp += tuning::tank_kill_or_capture_xp;
    break;
  case EntityKind::boss:
    ++view.stats.boss_kills;
    view.xp += tuning::boss_kill_xp;
    break;
  default:
    break;
  }
  if (vehicle(enemy.view.kind) && rank(Module::heavy_winch) == 3) {
    vehicle_burst = tuning::vehicle_bio_burst_seconds;
  }
}
} // namespace human_resources::gameplay
