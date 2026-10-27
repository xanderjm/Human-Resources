#include "encounter_rules.h"

namespace human_resources::gameplay {
using namespace detail;
namespace detail {
EncounterWindow encounter_window(double seconds) {
  const double phase = std::fmod(seconds, tuning::encounter_period_seconds);
  EncounterWindow result;
  result.formation = static_cast<unsigned>(seconds / (tuning::encounter_period_seconds / 4)) % 3;
  result.recovery = phase >= tuning::encounter_build_seconds + tuning::encounter_assault_seconds;
  if (phase < tuning::encounter_build_seconds) {
    result.soldier_multiplier = tuning::encounter_build_soldier_multiplier;
    result.warning_cap = tuning::encounter_build_warning_cap;
  } else if (!result.recovery) {
    result.soldier_multiplier = tuning::encounter_assault_soldier_multiplier;
    result.heavy_multiplier = tuning::encounter_assault_heavy_multiplier;
    result.warning_cap = tuning::encounter_assault_warning_cap;
  } else {
    result.soldier_multiplier = tuning::encounter_recovery_soldier_multiplier;
    result.warning_cap = tuning::encounter_recovery_warning_cap;
  }
  const double first = tuning::director_boss_first_second;
  const double quiet = tuning::encounter_boss_ingress_quiet_seconds;
  if (seconds < first) {
    result.boss_ingress = first - seconds <= quiet;
  } else {
    const double since = std::fmod(seconds - first, tuning::director_boss_interval_seconds);
    result.boss_ingress = since < quiet || tuning::director_boss_interval_seconds - since <= quiet;
  }
  if (seconds < tuning::population_shotgun_reveal_seconds) result.warning_cap = 1;
  else if (seconds < tuning::population_rocket_reveal_seconds) result.warning_cap = std::min(2U, result.warning_cap);
  return result;
}
} // namespace detail
namespace {
Vec2 tactical_target(const Snapshot& run, const Actor& actor, FlightBounds bounds) {
  if (actor.formation.active) return {actor.formation.slot.x, run.runner_distance + actor.formation.slot.z};
  const float side = actor.group_id ? (actor.formation.slot.x >= 0 ? 1.F : -1.F) : (actor.view.id & 1U ? 1.F : -1.F);
  float range = tuning::tactics_boss_range;
  float flank = tuning::tactics_flank_offset;
  switch (actor.view.kind) {
  case EntityKind::soldier:
    if (actor.view.infantry_role == InfantryRole::shotgunner) {
      range = tuning::tactics_shotgun_range;
      flank *= .35F;
    } else {
      range = std::max(actor.view.infantry_role == InfantryRole::rocketeer ? tuning::tactics_rocket_range : tuning::tactics_rifle_range,
          run.capture_radius + tuning::tactics_capture_standoff_buffer);
    }
    break;
  case EntityKind::jeep:
    range = tuning::tactics_jeep_range;
    break;
  case EntityKind::tank:
    range = tuning::tactics_tank_range;
    break;
  default:
    // The boss sweeps a visible front rather than sitting on the player.
    flank *= std::sin(actor.age * .55F);
    break;
  }
  const auto index = military_index(actor.view.kind);
  const float edge = tuning::field_half_width - tuning::military_body_radius[index];
  const float radius = tuning::military_body_radius[index];
  const float back = std::min(0.F, std::max(bounds.backward, -tuning::encounter_warning_retire_behind) + radius);
  const float front = std::max(0.F, std::min(bounds.forward, tuning::encounter_warning_entry_ahead) - radius);
  return {std::clamp(run.player.x + side * flank, -edge, edge),
      std::clamp(run.player.z + range, run.runner_distance + back, run.runner_distance + front)};
}
} // namespace

void Game::State::infantry_update(Actor& actor, float dt, InfantryAttackWindow window) {
  const auto attack = infantry_attack(actor.view.infantry_role);
  auto& pose = actor.infantry_aim;
  actor.view.fire_recovery_seconds = std::max(0.F, actor.view.fire_recovery_seconds - dt);
  const float station = actor.view.position.z - view.runner_distance;
  const bool visible = station <= tuning::encounter_warning_entry_ahead && station >= -tuning::encounter_warning_retire_behind;
  if (actor.view.warning_seconds > 0) {
    actor.view.velocity = {};
    if (!visible || !infantry_shot(actor.view.infantry_role, actor.view.position, actor.aim).valid) {
      actor.view.warning_seconds = 0;
      actor.attack_timer = tuning::enemy_attack_period[0];
      pose = {};
      return;
    }
    pose.pose_seconds += dt;
    actor.view.warning_seconds = std::max(0.F, actor.view.warning_seconds - dt);
    if (actor.view.warning_seconds == 0) { shoot(actor); pose = {}; }
    return;
  }
  actor.attack_timer = std::max(0.F, actor.attack_timer - dt);
  const float distance = std::sqrt(distance_squared(view.player, actor.view.position));
  const bool permitted = window.permitted && !actor.formation.active && visible && actor.view.capture_progress == 0 &&
      actor.view.fire_recovery_seconds <= 0 && distance < tuning::laser_range * 1.2F;
  if (!permitted) pose = {};
  if (permitted && (pose.preparing || window.available > 0)) {
    // Prepare within the existing cooldown budget, refreshing the provisional
    // point throughout turning/slot waits. Only a real warning locks the point.
    auto point = view.player;
    point.x = std::clamp(point.x, -tuning::field_half_width, tuning::field_half_width);
    const auto shot = infantry_shot(actor.view.infantry_role, actor.view.position, point);
    if (shot.valid) {
      const float yaw = std::atan2(shot.direction.x, -shot.direction.z) - attack.weapon_yaw_radians;
      const Vec2 target{std::sin(yaw), -std::cos(yaw)};
      auto facing = direction(actor.locomotion_velocity);
      if (length_squared(facing) == 0) facing = {0, -1};
      const float angle = std::abs(std::atan2(facing.x * target.z - facing.z * target.x, facing.x * target.x + facing.z * target.z));
      if (pose.preparing || actor.attack_timer <= angle / tuning::infantry_turn_radians_per_second + dt) {
        if (pose.preparing) pose.pose_seconds += dt;
        else pose = {true, 0};
        actor.aim = actor.view.warning_aim = point;
        actor.view.velocity = {};
        actor.locomotion_velocity = infantry_turn(facing, target, dt);
        actor.view.body_heading = std::atan2(actor.locomotion_velocity.x, -actor.locomotion_velocity.z);
        if (actor.attack_timer == 0 && angle <= tuning::infantry_turn_radians_per_second * dt && window.available > 0) {
          pose.preparing = false;
          actor.view.warning_seconds = attack.warning_seconds;
          actor.attack_timer = attack.cooldown_seconds;
          present(PresentationEventKind::warning_lock, actor.view.position, actor.view.id);
        }
        return;
      }
    } else pose = {};
  }
  if (actor.view.fire_recovery_seconds > 0) { actor.view.velocity = {}; return; }
  const auto motion = infantry_motion(actor, tactical_target(view, actor, flight_bounds), dt);
  actor.view.velocity = motion.velocity;
  actor.locomotion_velocity = motion.facing;
  actor.view.body_heading = std::atan2(motion.facing.x, -motion.facing.z);
  actor.view.position = add(actor.view.position, scaled(motion.velocity, dt));
}

void Game::State::enemies(float dt) {
  const auto window = encounter_window(view.stats.active_seconds);
  const bool boss_alive = std::any_of(actors.begin(), actors.end(), [](const Actor& a) {
    return !a.removed && a.view.kind == EntityKind::boss;
  });
  const auto warning_cap = boss_alive ? tuning::encounter_boss_warning_cap : window.warning_cap;
  // Derive occupancy at admission so removal, disable and cancellation release
  // their reservation immediately, including early-return actor paths.
  const auto occupied_slots = [&] {
    return static_cast<unsigned>(std::count_if(actors.begin(), actors.end(), reserves_threat_slot));
  };
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
    if (!military(a.view.kind) && !a.lifted && (a.view.position.z < view.runner_distance - tuning::enemy_retire_behind ||
        a.view.position.z > view.runner_distance + tuning::enemy_spawn_ahead * 3)) {
      a.removed = true;
      continue;
    }
    if (a.view.kind < EntityKind::soldier) {
      continue;
    }
    if (a.formation.active && !a.lifted) {
      const Vec2 slot{a.formation.slot.x, view.runner_distance + a.formation.slot.z};
      if (distance_squared(a.view.position, slot) <= tuning::tactics_arrival_radius * tuning::tactics_arrival_radius ||
          a.view.position.z <= slot.z) a.formation.active = false;
    }
    const auto index = military_index(a.view.kind);
    if (a.view.kind != EntityKind::soldier && a.view.kind != EntityKind::boss && length_squared(a.locomotion_velocity) > .0001F)
      a.view.body_heading = std::atan2(a.locomotion_velocity.x, -a.locomotion_velocity.z);
    a.view.disabled_seconds = std::max(0.0F, a.view.disabled_seconds - dt);
    if (a.view.kind == EntityKind::boss) {
      boss_update(a, dt);
      continue;
    }
    const bool minigun = a.view.kind == EntityKind::jeep;
    const bool captured_vehicle = minigun || a.view.kind == EntityKind::tank;
    const bool cannon = a.view.kind == EntityKind::tank || a.view.kind == EntityKind::boss;
    if (captured_vehicle) a.view.fire_recovery_seconds = std::max(0.F, a.view.fire_recovery_seconds - dt);
    if (a.lifted || a.view.disabled_seconds > 0) {
      a.view.warning_seconds = 0;
      a.infantry_aim = {};
      if (!captured_vehicle || a.lifted) a.view.fire_recovery_seconds = 0;
      a.view.velocity = {};
      continue;
    }
    const bool infantry = a.view.kind == EntityKind::soldier;
    if (infantry) {
      const float contact_distance = distance_squared(view.player, a.view.position);
      const unsigned own_cap = boss_alive ? warning_cap - 1 : warning_cap;
      const auto occupied = occupied_slots();
      // A preparation owns its slot until cancellation/fire, even if a phase
      // change lowers the admission cap below existing occupied reservations.
      const auto available = a.infantry_aim.preparing ? 1U : occupied < own_cap ? own_cap - occupied : 0;
      infantry_update(a, dt, {!window.boss_ingress && !boss_preparing(), available});
      const float contact_radius = tuning::military_contact_radius[0] + tuning::player_radius;
      if (contact_distance < contact_radius * contact_radius) {
        const float growth = 1 + std::max(0.F, static_cast<float>(view.stats.active_seconds / 60) - 2) * tuning::director_damage_growth_after_2min;
        hurt(tuning::enemy_damage[0] * growth, DamageCause::contact, a.view.id);
      }
      continue;
    }
    const float distance = std::sqrt(distance_squared(view.player, a.view.position));
    const auto destination = tactical_target(view, a, flight_bounds);
    const bool shot_recovery = a.view.fire_recovery_seconds <= 0 &&
        a.attack_timer > tuning::enemy_attack_period[index] - tuning::tactics_vehicle_fire_recovery;
    const bool planted = a.view.warning_seconds > 0 || shot_recovery;
    if (planted) {
      a.view.velocity = {};
    } else {
      const auto motion = vehicle_motion(a, destination, dt);
      a.view.velocity = motion.velocity;
      a.locomotion_velocity = motion.facing;
      a.escape = motion.escape;
      a.view.body_heading = std::atan2(motion.facing.x, -motion.facing.z);
    }
    a.view.position = add(a.view.position, scaled(a.view.velocity, dt));
    const float screen_station = a.view.position.z - view.runner_distance;
    const bool warning_visible = screen_station <= tuning::encounter_warning_entry_ahead &&
        screen_station >= -tuning::encounter_warning_retire_behind;
    if (distance < tuning::military_contact_radius[index] + tuning::player_radius) {
      const float growth = 1 + std::max(0.0F, static_cast<float>(view.stats.active_seconds / 60) - 2) * tuning::director_damage_growth_after_2min;
      hurt(tuning::enemy_damage[index] * growth, DamageCause::contact, a.view.id);
    }
    if (a.view.warning_seconds > 0 && (!warning_visible || (captured_vehicle && (a.view.capture_progress > 0 || a.view.fire_recovery_seconds > 0)) ||
        (minigun && !jeep_minigun_shot(a.view, a.aim, a.view.minigun_barrel).valid) ||
        (cannon && !vehicle_cannon_shot(a.view,a.aim).valid))) {
      a.view.warning_seconds = 0;
      a.attack_timer = tuning::enemy_attack_period[index];
      continue;
    }
    if (a.view.warning_seconds > 0) {
      a.view.warning_seconds = std::max(0.0F, a.view.warning_seconds - dt);
      if (a.view.warning_seconds == 0) {
        shoot(a);
      }
    } else {
      a.attack_timer = std::max(0.F, a.attack_timer - dt);
      const unsigned own_cap = boss_alive ? warning_cap - 1 : warning_cap;
      const auto occupied = occupied_slots();
      const bool may_commit = !a.formation.active && !window.boss_ingress && warning_visible && !boss_preparing();
      // Keep one slot available for the boss even when older infantry appear
      // earlier in the actor list. Recovery reduces pressure, never freezes AI.
      if (may_commit && occupied < own_cap && a.attack_timer <= 0 && a.view.fire_recovery_seconds <= 0 && distance < tuning::laser_range * 1.2F && occupied < warning_cap) {
        const float warning = tuning::attack_warning_seconds[index];
        a.aim = view.player;
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
        a.attack_timer = tuning::enemy_attack_period[index];
      }
    }
  }
  move_biological(dt);
}
void Game::State::shoot(Actor& enemy) {
  if (enemy.view.kind == EntityKind::boss || boss_leading()) return;
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
  const unsigned count = infantry ? attack.pellets : 1U;
  const float spread = infantry ? attack.spread_radians : 0;
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
