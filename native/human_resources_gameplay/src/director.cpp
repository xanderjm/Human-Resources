#include "run_state.h"

namespace human_resources::gameplay {
using namespace detail;

void Game::State::director(float dt) {
  const float minutes = static_cast<float>(view.stats.active_seconds / 60);
  const float rate = std::min(tuning::director_rate_multiplier_cap, 1 + minutes * tuning::director_rate_growth_per_minute) * tuning::director_pressure;
  biological_budget += dt * tuning::biological_spawn_per_second;
  soldier_budget += dt * tuning::director_soldier_rate * rate;
  if (view.stats.active_seconds >= tuning::enemy_start_second[1]) {
    jeep_budget += dt * tuning::director_jeep_rate * rate;
  }
  if (view.stats.active_seconds >= tuning::enemy_start_second[2]) {
    tank_budget += dt * tuning::director_tank_rate * rate;
  }
  auto consume = [this](float& budget, EntityKind kind) {
    if (budget >= 1) {
      budget -= 1;
      spawn(kind);
    }
  };
  if (biological_budget >= 1) {
    biological_budget -= 1;
    spawn(static_cast<EntityKind>(random() % 4));
  }
  consume(soldier_budget, EntityKind::soldier);
  consume(jeep_budget, EntityKind::jeep);
  consume(tank_budget, EntityKind::tank);
  if (view.stats.active_seconds >= next_boss) {
    spawn(EntityKind::boss);
    next_boss += tuning::director_boss_interval_seconds;
  }
}
void Game::State::spawn(EntityKind kind, bool initial) {
  // Consume the same encounter samples even when its bounded population is full.
  const float lateral_sample = random_unit();
  const float forward_sample = random_unit();
  const float timing_sample = random_unit();
  const float heading_sample = random_unit();
  ++view.director_spawn_attempts;
  view.director_schedule_hash = (view.director_schedule_hash ^ random_state ^ static_cast<unsigned>(kind)) * 1099511628211ULL;
  const auto count = static_cast<unsigned>(std::count_if(actors.begin(), actors.end(), [kind](const Actor& a) {
    return !a.removed && (kind < EntityKind::soldier ? a.view.kind < EntityKind::soldier : a.view.kind == kind);
  }));
  const auto limit = kind < EntityKind::soldier ? tuning::biological_active_cap : static_cast<unsigned>(tuning::enemy_active_cap[military_index(kind)]);
  if (count >= limit) {
    return;
  }
  Actor a;
  a.view.id = next_id++;
  a.view.kind = kind;
  if (kind == EntityKind::soldier)
    a.view.infantry_role = static_cast<InfantryRole>(1 + std::min(2U, static_cast<unsigned>(heading_sample * 3)));
  if (kind == EntityKind::jeep) a.view.minigun_barrel = static_cast<std::uint8_t>(a.view.id % 6U);
  a.view.position = {-tuning::field_half_width + 2 * tuning::field_half_width * lateral_sample,
      view.runner_distance + (initial ? 1.0F : tuning::enemy_spawn_ahead * 0.55F) + (tuning::enemy_spawn_ahead - (initial ? 1.0F : tuning::enemy_spawn_ahead * 0.55F)) * forward_sample};
  if (kind < EntityKind::soldier) {
    if (!place_biological(a, lateral_sample, forward_sample, initial)) {
      return;
    }
    a.view.hull = a.view.maximum_hull = 1;
    const float speed = tuning::biological_movement_speed[static_cast<std::size_t>(kind)];
    const float angle = -3.14159265F + 6.2831853F * heading_sample;
    a.view.velocity = {std::sin(angle) * speed, std::cos(angle) * speed};
    a.wander_velocity = a.view.velocity;
  } else {
    const auto index = military_index(kind);
    const float growth = 1 + std::max(0.0F, static_cast<float>(view.stats.active_seconds / 60) - 2) * tuning::director_health_growth_after_2min;
    const float boss_index = static_cast<float>(std::max(0.0, (view.stats.active_seconds - tuning::director_boss_first_second) / tuning::director_boss_interval_seconds));
    const float hp = kind == EntityKind::boss ? tuning::director_boss_base_hp * (1 + boss_index * tuning::director_boss_health_index_coefficient) : tuning::enemy_hp[index] * growth;
    a.view.hull = a.view.maximum_hull = hp;
    const float period = kind == EntityKind::soldier ? infantry_attack(a.view.infantry_role).cooldown_seconds : tuning::enemy_attack_period[index];
    a.attack_timer = period * (0.5F + 0.5F * timing_sample);
  }
  actors.push_back(a);
}
void Game::State::enemies(float dt) {
  unsigned warnings = static_cast<unsigned>(std::count_if(actors.begin(), actors.end(), [](const Actor& a) { return a.view.warning_seconds > 0; }));
  for (auto& a : actors) {
    if (a.removed) {
      continue;
    }
    a.age += dt;
    a.slow_timer = std::max(0.0F, a.slow_timer - dt);
    a.focus_timer = std::max(0.0F, a.focus_timer - dt);
    if (a.focus_timer == 0) {
      a.focused_hits = 0;
    }
    if (a.view.position.z < view.runner_distance - tuning::enemy_retire_behind ||
        a.view.position.z > view.runner_distance + tuning::enemy_spawn_ahead * 3) {
      a.removed = true;
      continue;
    }
    if (a.view.kind < EntityKind::soldier) {
      continue;
    }
    const auto index = military_index(a.view.kind);
    if (a.age > tuning::enemy_lifetime[index] && a.view.kind != EntityKind::boss) {
      a.removed = true;
      continue;
    }
    a.view.disabled_seconds = std::max(0.0F, a.view.disabled_seconds - dt);
    const bool minigun = a.view.kind == EntityKind::jeep;
    const bool captured_vehicle = minigun || a.view.kind == EntityKind::tank;
    const bool cannon = a.view.kind == EntityKind::tank || a.view.kind == EntityKind::boss;
    if (captured_vehicle) a.view.fire_recovery_seconds = std::max(0.F, a.view.fire_recovery_seconds - dt);
    if (a.lifted || a.view.disabled_seconds > 0) {
      a.view.warning_seconds = 0;
      if (!captured_vehicle || a.lifted) a.view.fire_recovery_seconds = 0;
      a.view.velocity = {};
      continue;
    }
    const bool infantry = a.view.kind == EntityKind::soldier;
    const auto attack = infantry_attack(a.view.infantry_role);
    if (!captured_vehicle) a.view.fire_recovery_seconds = std::max(0.F, a.view.fire_recovery_seconds - dt);
    const float distance = std::sqrt(distance_squared(view.player, a.view.position));
    const auto toward = direction({view.player.x - a.view.position.x, view.player.z - a.view.position.z});
    const float slow = a.slow_timer > 0 ? (a.view.kind == EntityKind::boss ? tuning::storm_boss_slow : tuning::storm_slow) : 0;
    const bool planted = infantry && (a.view.warning_seconds > 0 || a.view.fire_recovery_seconds > 0);
    const float speed = planted ? 0.F : tuning::military_movement_speed[index] * (1 - slow) * (distance > 6 ? 1.0F : 0.25F);
    a.view.velocity = scaled(toward, speed);
    a.view.position = add(a.view.position, scaled(a.view.velocity, dt));
    if (distance < tuning::military_contact_radius[index] + tuning::player_radius) {
      const float growth = 1 + std::max(0.0F, static_cast<float>(view.stats.active_seconds / 60) - 2) * tuning::director_damage_growth_after_2min;
      hurt(tuning::enemy_damage[index] * growth);
    }
    if (a.view.warning_seconds > 0 && ((captured_vehicle && (a.view.capture_progress > 0 || a.view.fire_recovery_seconds > 0)) ||
        (minigun && !jeep_minigun_shot(a.view, a.aim, a.view.minigun_barrel).valid) ||
        (cannon && !vehicle_cannon_shot(a.view,a.aim).valid))) {
      a.view.warning_seconds = 0;
      a.attack_timer = tuning::enemy_attack_period[index];
      if (warnings > 0) --warnings;
      continue;
    }
    if (a.view.warning_seconds > 0) {
      a.view.warning_seconds = std::max(0.0F, a.view.warning_seconds - dt);
      if (a.view.warning_seconds == 0) {
        shoot(a);
        if (warnings > 0) {
          --warnings;
        }
      }
    } else {
      a.attack_timer -= dt;
      if (a.attack_timer <= 0 && a.view.fire_recovery_seconds <= 0 && distance < tuning::laser_range * 1.2F && warnings < tuning::warning_active_cap) {
        const float warning = infantry ? attack.warning_seconds : tuning::attack_warning_seconds[index];
        const float shot_speed = infantry ? attack.projectile_speed : tuning::projectile_speed[index];
        const float flight = distance / shot_speed;
        const float lead = (warning + flight) * tuning::attack_prediction_fraction;
        a.aim = add(view.player, scaled(view.velocity, lead));
        a.aim.x = std::clamp(a.aim.x, -tuning::field_half_width, tuning::field_half_width);
        if (minigun) {
          const auto barrel = static_cast<std::uint8_t>((a.view.minigun_barrel + 1U) % 6U);
          if (a.view.capture_progress > 0 || a.view.fire_recovery_seconds > 0 || !jeep_minigun_shot(a.view, a.aim, barrel).valid) {
            a.attack_timer = tuning::enemy_attack_period[index];
            continue;
          }
          a.view.minigun_barrel = barrel;
        }
        if (cannon && (a.view.capture_progress > 0 || a.view.fire_recovery_seconds > 0 || !vehicle_cannon_shot(a.view,a.aim).valid)) {
          a.attack_timer=tuning::enemy_attack_period[index];
          continue;
        }
        a.view.warning_seconds = warning;
        a.view.warning_aim = a.aim; // warning commits the shot; movement can dodge it
        present(PresentationEventKind::warning_lock, a.view.position, a.view.id);
        a.attack_timer = infantry ? attack.cooldown_seconds : tuning::enemy_attack_period[index];
        if (infantry)
          a.view.velocity = {}; // Deliberate planted aim; movement resumes after authored fire recovery.
        ++warnings;
      }
    }
  }
  move_biological(dt);
}
void Game::State::shoot(Actor& enemy) {
  const auto index = military_index(enemy.view.kind);
  const bool infantry = enemy.view.kind == EntityKind::soldier;
  const auto attack = infantry_attack(enemy.view.infantry_role);
  const bool minigun = enemy.view.kind == EntityKind::jeep;
  const bool cannon = enemy.view.kind==EntityKind::tank || enemy.view.kind==EntityKind::boss;
  const auto shell=vehicle_cannon_shot(enemy.view,enemy.aim);
  if(cannon && (enemy.view.capture_progress>0 || enemy.view.fire_recovery_seconds>0 || !shell.valid)) return;
  const auto gun = jeep_minigun_shot(enemy.view, enemy.aim, enemy.view.minigun_barrel);
  if (minigun && (enemy.view.capture_progress > 0 || enemy.view.fire_recovery_seconds > 0 || !gun.valid)) return;
  const auto muzzle = infantry_shot(enemy.view.infantry_role, enemy.view.position, enemy.aim);
  if (infantry && !muzzle.valid)
    return;
  const auto origin = infantry ? muzzle.origin : minigun ? gun.origin : cannon ? shell.origin : enemy.view.position;
  const auto toward = infantry ? muzzle.direction : minigun ? gun.direction : cannon ? shell.direction : direction({enemy.aim.x - origin.x, enemy.aim.z - origin.z});
  const unsigned count = infantry ? attack.pellets : enemy.view.kind == EntityKind::boss ? 3U : 1U;
  const float spread = infantry ? attack.spread_radians : .22F;
  bool fired = false;
  for (unsigned i = 0; i < count && bullets.size() < tuning::projectile_active_cap; ++i) {
    const float angle = count == 1 ? 0 : (static_cast<float>(i) - static_cast<float>(count - 1) * .5F) * spread;
    const Vec2 direction_to{toward.x * std::cos(angle) - toward.z * std::sin(angle), toward.x * std::sin(angle) + toward.z * std::cos(angle)};
    Projectile p;
    p.view.id = next_id++;
    p.source = enemy.view.id;
    p.view.position = p.view.launch_position = origin;
    const float shot_speed = infantry ? attack.projectile_speed : tuning::projectile_speed[index];
    p.view.velocity = scaled(direction_to, shot_speed);
    p.view.radius = tuning::projectile_radius[index];
    p.view.infantry_role = enemy.view.infantry_role;
    p.view.kind = minigun ? EnemyProjectileKind::jeep_minigun : infantry ?
        (enemy.view.infantry_role == InfantryRole::rocketeer ? EnemyProjectileKind::infantry_rocket : EnemyProjectileKind::infantry_round) : enemy.view.kind==EntityKind::boss ? EnemyProjectileKind::boss_missile : EnemyProjectileKind::vehicle_missile;
    p.view.muzzle_index = minigun ? enemy.view.minigun_barrel : 0;
    p.view.launch_direction = toward;
    p.view.launch_height = infantry ? muzzle.height : minigun ? gun.height : cannon ? shell.height : 2.2F;
    const float flight = std::sqrt(distance_squared(origin, enemy.aim)) / shot_speed;
    p.view.rise_per_second = (infantry || minigun || cannon) && flight > 0 ? std::max(0.F, tuning::projectile_target_height - p.view.launch_height) / flight : 0;
    const float growth = 1 + std::max(0.0F, static_cast<float>(view.stats.active_seconds / 60) - 2) * tuning::director_damage_growth_after_2min;
    p.view.damage = tuning::enemy_damage[index] * growth * (infantry ? attack.damage_fraction : 1.F);
    bullets.push_back(p);
    fired = true;
  }
  if (fired) {
    if (infantry)
      enemy.view.fire_recovery_seconds = attack.recovery_seconds;
    present(PresentationEventKind::enemy_shot, origin, enemy.view.id);
  }
}
} // namespace human_resources::gameplay
