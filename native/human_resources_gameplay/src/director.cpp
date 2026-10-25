#include "run_state.h"

namespace human_resources::gameplay {
using namespace detail;
namespace {
struct EncounterWindow final {
  float soldier_multiplier{}, heavy_multiplier{};
  unsigned warning_cap{}, formation{};
  bool recovery{}, boss_ingress{};
};
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
  return result;
}
Vec2 tactical_target(const Snapshot& run, const Actor& actor) {
  const float side = actor.view.id & 1U ? 1.F : -1.F;
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
  return {std::clamp(run.player.x + side * flank, -edge, edge), run.player.z + range};
}
} // namespace

void Game::State::director(float dt) {
  const auto window = encounter_window(view.stats.active_seconds);
  const bool boss_alive = std::any_of(actors.begin(), actors.end(), [](const Actor& a) {
    return !a.removed && a.view.kind == EntityKind::boss;
  });
  const float minutes = static_cast<float>(view.stats.active_seconds / 60);
  const float rate = std::min(tuning::director_rate_multiplier_cap, 1 + minutes * tuning::director_rate_growth_per_minute) *
      tuning::director_pressure * tuning::encounter_military_pressure_multiplier;
  // Keep edible route density as actual world travel accelerates.
  biological_budget += dt * tuning::biological_spawn_per_second *
      std::max(1.F, view.forward_speed / tuning::forward_scroll_metres_per_second);
  // Quiet windows accumulate no reinforcements to dump on the next frame.
  if (!window.boss_ingress)
    soldier_budget += dt * tuning::director_soldier_rate * rate * window.soldier_multiplier;
  if (!window.boss_ingress && !boss_alive && view.stats.active_seconds >= tuning::enemy_start_second[1]) {
    jeep_budget += dt * tuning::director_jeep_rate * rate * window.heavy_multiplier;
  }
  if (!window.boss_ingress && !boss_alive && view.stats.active_seconds >= tuning::enemy_start_second[2]) {
    tank_budget += dt * tuning::director_tank_rate * rate * window.heavy_multiplier;
  }
  // Social pockets spend the existing per-specimen budget. Geometry and species
  // belong to this delivery, independent of interleaved military spawn slots.
  if (biological_budget >= static_cast<float>(tuning::encounter_cohort_size)) {
    biological_budget -= static_cast<float>(tuning::encounter_cohort_size);
    spawn_cohort();
  }
  const auto crossed = [this, dt](double second) {
    return view.stats.active_seconds >= second && view.stats.active_seconds - dt < second;
  };
  // Introductions are authored arrivals, not potentially minute-long waits for
  // fractional heavy budgets. Later arrivals retain the time-only endless rate.
  if (crossed(tuning::encounter_intro_escort_second)) {
    spawn(EntityKind::soldier);
    spawn(EntityKind::soldier);
  }
  for (const auto kind : {EntityKind::jeep, EntityKind::tank}) {
    if (crossed(tuning::enemy_start_second[military_index(kind)]))
      spawn(kind);
  }
  if (!window.boss_ingress) {
    if (soldier_budget >= 2) {
      soldier_budget -= 2;
      spawn(EntityKind::soldier);
      spawn(EntityKind::soldier);
    }
    if (window.heavy_multiplier > 0 && !boss_alive) {
      for (const auto kind : {EntityKind::jeep, EntityKind::tank}) {
        auto& budget = kind == EntityKind::jeep ? jeep_budget : tank_budget;
        if (budget >= 1) {
          budget -= 1;
          spawn(kind);
        }
      }
    }
  }
  if (view.stats.active_seconds >= next_boss) {
    spawn(EntityKind::boss);
    next_boss += tuning::director_boss_interval_seconds;
  }
}
void Game::State::spawn_cohort(bool initial, unsigned opening_group) {
  // One seed-bound choice per pocket, including when capacity blocks delivery.
  // Shapes describe a loose diamond, crescent or two pairs, never a single rail.
  constexpr std::array<std::array<Vec2, 4>, 3> shapes{{
      {{{-1, 1}, {0, 0}, {1, 1}, {0, 2}}},
      {{{-1, 0}, {-.5F, 1}, {.5F, 1.5F}, {1, .25F}}},
      {{{-1, 0}, {-.8F, 1}, {.8F, .4F}, {1, 1.4F}}}}};
  constexpr std::array<std::array<Vec2, 4>, 3> cattle_shapes{{
      {{{-1, 0}, {-1, 2}, {1, 0}, {1, 2}}},
      {{{-1, 0}, {-1, 1.95F}, {1, .05F}, {1, 2}}},
      {{{-1, .05F}, {-1, 2}, {1, 0}, {1, 1.95F}}}}};
  const auto composition = random();
  const auto shape = random() % shapes.size();
  const float lane_sample = random_unit();
  const float depth_sample = random_unit();
  const float side = initial ? (((opening_group + (setup.seed & 1U)) & 1U) ? 1.F : -1.F)
                             : ((composition & 8U) ? 1.F : -1.F);
  const auto family = composition % 6U;
  const auto first = (composition / 6U) % 4U;
  const auto second = (first + 1U + (composition / 24U) % 3U) % 4U;
  std::array<EntityKind, 4> species{};
  for (unsigned slot = 0; slot < tuning::encounter_cohort_size; ++slot) {
    // Whole herds, two social pairs or a farmer with one species of livestock.
    species[slot] = static_cast<EntityKind>(family < 4 ? family : family == 4 ? (slot < 2 ? first : second)
                                                                          : (slot == 0 ? 0U : 1U + first % 3U));
  }
  const bool cattle = std::find(species.begin(), species.end(), EntityKind::cow) != species.end();
  const bool near_food = initial && opening_group == 0;
  const float lane = near_food ? tuning::encounter_opening_lane_metres : tuning::encounter_herd_lane_metres +
      (2 * lane_sample - 1) * tuning::encounter_herd_lane_jitter_metres +
      (cattle ? tuning::encounter_cattle_lane_offset_metres : 0.F);
  const float lateral_step = cattle ? tuning::encounter_cattle_lateral_step : tuning::encounter_herd_lateral_step;
  const float ahead = initial ? tuning::encounter_opening_near +
      static_cast<float>(opening_group) * tuning::encounter_opening_group_step : tuning::enemy_spawn_ahead;
  for (unsigned slot = 0; slot < tuning::encounter_cohort_size; ++slot) {
    // Cattle use their full cooked ground footprint, rather than the smaller
    // inscribed contact core. Keep the same pocket depth and delivery budget.
    const auto offset = cattle ? cattle_shapes[shape][slot] : shapes[shape][slot];
    const Vec2 position{side * (lane + offset.x * lateral_step),
        view.runner_distance + ahead + offset.z * tuning::encounter_herd_forward_step +
            (near_food ? 0.F : depth_sample * tuning::encounter_herd_forward_step)};
    spawn(species[slot], initial, position);
  }
}
void Game::State::spawn(EntityKind kind, bool initial, std::optional<Vec2> pocket_position) {
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
  const auto slot = (view.director_spawn_attempts - 1) % tuning::encounter_cohort_size;
  a.view.position = {0, view.runner_distance + tuning::enemy_spawn_ahead};
  if (kind < EntityKind::soldier) {
    // Direct single-body spawns retain bounded samples; ordinary delivery always
    // supplies its own social pocket position above.
    a.view.position = pocket_position.value_or(Vec2{(2 * lateral_sample - 1) * tuning::encounter_herd_lane_metres,
        view.runner_distance + tuning::enemy_spawn_ahead});
    if (!place_biological(a, lateral_sample, forward_sample, initial)) {
      return;
    }
    a.view.hull = a.view.maximum_hull = 1;
    const float speed = tuning::biological_movement_speed[static_cast<std::size_t>(kind)];
    const float lateral = (2 * heading_sample - 1) * tuning::tactics_civilian_travel_lateral_bias;
    a.view.velocity = scaled(direction({lateral, 1}), speed);
    a.locomotion_velocity = a.view.velocity;
  } else {
    const auto index = military_index(kind);
    if (kind != EntityKind::soldier) {
      a.locomotion_velocity = {0, -tuning::military_movement_speed[index]};
      a.view.body_heading = 0;
    }
    if (kind == EntityKind::boss) {
      a.view.position.x = (2 * lateral_sample - 1) * tuning::encounter_boss_entry_half_width;
    } else {
      // The open route accounts for both bodies' real contact envelopes.
      // This is entry formation only; pursuit and committed shots stay live.
      const float inner = tuning::military_contact_radius[index] + tuning::player_radius + tuning::encounter_open_route_half_width;
      const auto formation = encounter_window(view.stats.active_seconds).formation;
      const bool right = formation == 1 || (formation == 2 && lateral_sample >= .5F);
      const float sample = formation == 2 ? std::fmod(lateral_sample * 2, 1.F) : lateral_sample;
      const float lateral = inner + (tuning::field_half_width - inner) * sample;
      a.view.position.z += static_cast<float>(slot % 2) * tuning::encounter_herd_forward_step;
      a.view.position.x = right ? lateral : -lateral;
    }
    place_military(a);
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
  const auto window = encounter_window(view.stats.active_seconds);
  const bool boss_alive = std::any_of(actors.begin(), actors.end(), [](const Actor& a) {
    return !a.removed && a.view.kind == EntityKind::boss;
  });
  const auto warning_cap = boss_alive ? tuning::encounter_boss_warning_cap : window.warning_cap;
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
    if (a.view.kind != EntityKind::soldier && length_squared(a.locomotion_velocity) > .0001F)
      a.view.body_heading = std::atan2(a.locomotion_velocity.x, -a.locomotion_velocity.z);
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
    const auto destination = tactical_target(view, a);
    const Vec2 route{destination.x - a.view.position.x, destination.z - a.view.position.z};
    const float slow = a.slow_timer > 0 ? (a.view.kind == EntityKind::boss ? tuning::storm_boss_slow : tuning::storm_slow) : 0;
    const bool shot_recovery = !infantry && a.view.fire_recovery_seconds <= 0 &&
        a.attack_timer > tuning::enemy_attack_period[index] - tuning::tactics_vehicle_fire_recovery;
    const bool planted = a.view.warning_seconds > 0 || (infantry && a.view.fire_recovery_seconds > 0) || shot_recovery;
    const float arrival = std::min(1.F, std::sqrt(length_squared(route)) / tuning::tactics_arrival_radius);
    const float speed = planted ? 0.F : tuning::military_movement_speed[index] * (1 - slow) * arrival;
    a.view.velocity = planted ? Vec2{} : military_velocity(a, scaled(direction(route), speed), dt);
    if (!infantry && length_squared(a.view.velocity) > .0001F) {
      a.locomotion_velocity = a.view.velocity;
      a.view.body_heading = std::atan2(a.view.velocity.x, -a.view.velocity.z);
    }
    a.view.position = add(a.view.position, scaled(a.view.velocity, dt));
    const float screen_station = a.view.position.z - view.runner_distance;
    const bool warning_visible = screen_station <= tuning::encounter_warning_entry_ahead &&
        screen_station >= -tuning::encounter_warning_retire_behind;
    if (distance < tuning::military_contact_radius[index] + tuning::player_radius) {
      const float growth = 1 + std::max(0.0F, static_cast<float>(view.stats.active_seconds / 60) - 2) * tuning::director_damage_growth_after_2min;
      hurt(tuning::enemy_damage[index] * growth);
    }
    if (a.view.warning_seconds > 0 && (!warning_visible || (captured_vehicle && (a.view.capture_progress > 0 || a.view.fire_recovery_seconds > 0)) ||
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
      a.attack_timer = std::max(0.F, a.attack_timer - dt);
      const bool boss = a.view.kind == EntityKind::boss;
      const unsigned own_cap = boss_alive && !boss ? warning_cap - 1 : warning_cap;
      const bool may_commit = (boss || !window.boss_ingress) && warning_visible;
      // Keep one slot available for the boss even when older infantry appear
      // earlier in the actor list. Recovery reduces pressure, never freezes AI.
      if (may_commit && warnings < own_cap && a.attack_timer <= 0 && a.view.fire_recovery_seconds <= 0 && distance < tuning::laser_range * 1.2F && warnings < warning_cap) {
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
  const auto cannon_pattern = cannon_geometry();
  const unsigned count = infantry ? attack.pellets : enemy.view.kind == EntityKind::boss ? cannon_pattern.boss_projectiles
                                                                                         : 1U;
  const float spread = infantry ? attack.spread_radians : cannon_pattern.boss_spread_radians;
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
