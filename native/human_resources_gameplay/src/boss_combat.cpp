#include "run_state.h"
#include <numbers>

namespace human_resources::gameplay {
using namespace detail;
namespace {
std::uint16_t ticks(float seconds) {
  return static_cast<std::uint16_t>(std::lround(seconds * tuning::fixed_hz));
}
void phase(BossState& state, BossPhase kind, float seconds) {
  state.phase = kind;
  state.remaining_ticks = state.total_ticks = ticks(seconds);
}
float wrap(float angle) { return std::remainder(angle, 2 * std::numbers::pi_v<float>); }
const Actor* owner(const auto& state) {
  if (!state.boss) return nullptr;
  const auto found = std::find_if(state.actors.begin(), state.actors.end(), [&](const Actor& actor) {
    return !actor.removed && actor.view.kind == EntityKind::boss && actor.view.id == state.boss->actor_id;
  });
  return found == state.actors.end() ? nullptr : &*found;
}
std::optional<BossAttackPlan> eligible(const auto& state, const Actor& actor, float delay = 0) {
  const auto& boss = *state.boss;
  const float warning = boss.next_kind == BossAttackKind::fan ? tuning::boss_fan_warning_seconds : tuning::boss_rake_warning_seconds;
  const float planted = warning + (boss.next_kind == BossAttackKind::rake ? 2 * tuning::boss_rake_interval_seconds : 0);
  const float runner = state.view.runner_distance + state.view.forward_speed * delay;
  const Vec2 player{std::clamp(state.view.player.x + state.view.velocity.x * delay, -tuning::field_half_width, tuning::field_half_width),
      std::clamp(state.view.player.z + state.view.velocity.z * delay, runner - tuning::player_backward_allowance, runner + tuning::player_forward_allowance)};
  const float station = actor.view.position.z - runner;
  const float low = -tuning::encounter_warning_retire_behind + tuning::scroll_maximum_metres_per_second *
      (planted + 1 / tuning::fixed_hz);
  if (station < low || station > tuning::encounter_warning_entry_ahead ||
      distance_squared(actor.view.position, player) >= tuning::laser_range * tuning::laser_range * 1.44F)
    return {};
  auto frame = actor.view;
  frame.body_heading = boss.body_heading;
  const auto direct = vehicle_cannon_shot(frame, player);
  if (!direct.valid || distance_squared(direct.origin, player) < tuning::boss_minimum_source_range * tuning::boss_minimum_source_range)
    return {};
  if (state.bullets.size() + 3 > tuning::projectile_active_cap)
    return {};
  return boss_attack_plan(frame, player, state.view.velocity, boss.next_kind);
}
bool ready_to_brake(const auto& state, const Actor& actor) {
  // The actual N-tick linear brake integrates v*(N-1)/2 fixed steps.
  // Test that future physical source, not a looser current-range threshold.
  auto stopped = actor;
  const float travel_seconds = static_cast<float>(ticks(tuning::boss_brake_seconds) - 1U) / (2 * tuning::fixed_hz);
  stopped.view.position = add(stopped.view.position, scaled(stopped.view.velocity, travel_seconds));
  return eligible(state, stopped, tuning::boss_brake_seconds).has_value();
}
bool finite(Vec2 value) { return std::isfinite(value.x) && std::isfinite(value.z); }
bool near(float a, float b) { return std::isfinite(a) && std::isfinite(b) && std::abs(a - b) <= .00002F; }
bool near(Vec2 a, Vec2 b) { return near(a.x, b.x) && near(a.z, b.z); }
bool valid_plan(const BossAttackPlan& plan) {
  if (plan.kind > BossAttackKind::rake || !finite(plan.locked_root) ||
      !std::isfinite(plan.initial_turret_yaw) || std::abs(plan.initial_turret_yaw) > 7 ||
      !std::isfinite(plan.locked_body_heading) || std::abs(plan.locked_body_heading) > 4 ||
      plan.warning_seconds != (plan.kind == BossAttackKind::fan ? tuning::boss_fan_warning_seconds : tuning::boss_rake_warning_seconds) ||
      plan.salvo_seconds != (plan.kind == BossAttackKind::rake ? 2 * tuning::boss_rake_interval_seconds : 0)) return false;
  EntityView locked;
  locked.kind = EntityKind::boss; locked.position = plan.locked_root;
  locked.body_heading = plan.locked_body_heading;
  for (unsigned i = 0; i < plan.shells.size(); ++i) {
    const auto& shell = plan.shells[i];
    const auto physical = vehicle_cannon_shot(locked, shell.aim);
    if (!shell.shot.valid || !physical.valid || !finite(shell.trajectory) ||
        !near(length_squared(shell.trajectory), 1) || !near(shell.shot.origin, physical.origin) ||
        !near(shell.shot.direction, physical.direction) || !near(shell.shot.height, physical.height) ||
        !near(shell.shot.yaw_radians, physical.yaw_radians) ||
        shell.release_offset != (plan.kind == BossAttackKind::fan ? 0 : static_cast<float>(i) * tuning::boss_rake_interval_seconds)) return false;
    if (plan.kind == BossAttackKind::rake) {
      if (!near(shell.trajectory, shell.shot.direction)) return false;
    } else {
      if (!near(shell.aim, plan.shells[1].aim)) return false;
      const float angle = (static_cast<float>(i) - 1) * tuning::boss_fan_spread_radians;
      const Vec2 ray{physical.direction.x * std::cos(angle) - physical.direction.z * std::sin(angle),
          physical.direction.x * std::sin(angle) + physical.direction.z * std::cos(angle)};
      if (!near(shell.trajectory, ray)) return false;
    }
  }
  if (plan.kind == BossAttackKind::rake) {
    const Vec2 centre{plan.shells[1].aim.x - plan.locked_root.x, plan.shells[1].aim.z - plan.locked_root.z};
    const float c = std::cos(tuning::boss_rake_spread_radians), s = std::sin(tuning::boss_rake_spread_radians);
    const Vec2 left{plan.locked_root.x + centre.x * c - centre.z * s, plan.locked_root.z + centre.x * s + centre.z * c};
    const Vec2 right{plan.locked_root.x + centre.x * c + centre.z * s, plan.locked_root.z - centre.x * s + centre.z * c};
    if (!((near(plan.shells[0].aim, left) && near(plan.shells[2].aim, right)) ||
          (near(plan.shells[0].aim, right) && near(plan.shells[2].aim, left)))) return false;
  }
  return true;
}
} // namespace
bool Game::State::boss_leading() const {
  return boss && owner(*this) && (boss->phase == BossPhase::warning || boss->phase == BossPhase::salvo);
}
bool Game::State::boss_preparing() const {
  const auto* actor = owner(*this);
  if (!actor || actor->view.disabled_seconds > 0) return false;
  if (boss_leading() || boss->phase == BossPhase::braking ||
      (boss->phase == BossPhase::reposition && boss->remaining_ticks <= ticks(tuning::boss_brake_seconds))) return true;
  return boss->phase == BossPhase::approach && actor->attack_timer <= 0 && ready_to_brake(*this, *actor);
}
void Game::State::boss_release(Actor& actor) {
  auto& state = *boss;
  if (!state.plan || state.next_release >= state.plan->shells.size() || actor.removed) return;
  const auto& shell = state.plan->shells[state.next_release];
  // Admission quiesces other warnings first, then protects three free slots.
  // A violated invariant cancels the whole remainder rather than a middle ray.
  if (bullets.size() >= tuning::projectile_active_cap) {
    state.plan.reset(); state.next_release = 0; phase(state, BossPhase::approach, 0); return;
  }
  Projectile bullet;
  bullet.source = actor.view.id; bullet.view.id = next_id++;
  bullet.view.kind = EnemyProjectileKind::boss_missile;
  bullet.view.position = bullet.view.launch_position = shell.shot.origin;
  bullet.view.launch_direction = shell.shot.direction;
  bullet.view.launch_height = shell.shot.height;
  bullet.view.velocity = scaled(shell.trajectory, tuning::projectile_speed[3]);
  bullet.view.radius = tuning::projectile_radius[3];
  const float growth = 1 + std::max(0.F, static_cast<float>(view.stats.active_seconds / 60) - 2) * tuning::director_damage_growth_after_2min;
  bullet.view.damage = tuning::enemy_damage[3] * growth;
  const float flight = std::sqrt(distance_squared(shell.shot.origin, shell.aim)) / tuning::projectile_speed[3];
  bullet.view.rise_per_second = flight > 0 ? std::max(0.F, tuning::projectile_target_height - shell.shot.height) / flight : 0;
  bullets.push_back(bullet);
  present(PresentationEventKind::enemy_shot, shell.shot.origin, actor.view.id);
  ++state.next_release;
}
void Game::State::boss_update(Actor& actor, float dt) {
  if (!boss || boss->actor_id != actor.view.id) {
    boss.emplace(); boss->actor_id = actor.view.id;
    boss->behind = view.player.z - view.runner_distance > tuning::boss_station_switch_high;
    boss->flank_right = view.player.x <= 0;
    boss->body_heading = actor.view.body_heading.value_or(0);
  }
  auto& state = *boss;
  actor.view.body_heading = state.body_heading;
  if (!(dt > 0)) return;
  if (actor.view.disabled_seconds > 0) {
    actor.view.velocity = {}; actor.view.warning_seconds = 0; actor.view.fire_recovery_seconds = 0;
    state.plan.reset(); state.next_release = 0; phase(state, BossPhase::approach, 0); return;
  }
  const float range = std::sqrt(distance_squared(view.player, actor.view.position));
  if (range < tuning::military_contact_radius[3] + tuning::player_radius) {
    const float growth = 1 + std::max(0.F, static_cast<float>(view.stats.active_seconds / 60) - 2) * tuning::director_damage_growth_after_2min;
    hurt(tuning::enemy_damage[3] * growth, DamageCause::contact, actor.view.id);
  }
  actor.attack_timer = std::max(0.F, actor.attack_timer - dt);
  const auto braking = ticks(tuning::boss_brake_seconds);
  const bool old_warning = std::any_of(actors.begin(), actors.end(), [&](const Actor& other) {
    return !other.removed && other.view.id != actor.view.id && other.view.warning_seconds > 0;
  });
  if ((state.phase == BossPhase::reposition && state.remaining_ticks <= braking) ||
      (state.phase == BossPhase::approach && actor.attack_timer <= 0 && !old_warning && ready_to_brake(*this, actor)))
    phase(state, BossPhase::braking, tuning::boss_brake_seconds);
  if (state.phase == BossPhase::warning || state.phase == BossPhase::salvo) {
    actor.view.velocity = {};
    if (state.remaining_ticks > 0) --state.remaining_ticks;
    if (state.phase == BossPhase::warning && state.remaining_ticks == 0) {
      const bool rake = state.plan->kind == BossAttackKind::rake;
      if (rake) phase(state, BossPhase::salvo, state.plan->salvo_seconds);
      do { boss_release(actor); } while (state.plan && !rake && state.next_release < 3);
    } else if (state.phase == BossPhase::salvo) {
      const auto elapsed = state.total_ticks - state.remaining_ticks;
      while (state.plan && state.next_release < 3 && elapsed >= ticks(state.plan->shells[state.next_release].release_offset))
        boss_release(actor);
    }
    if (state.plan && state.next_release == 3) {
      state.next_kind = state.plan->kind == BossAttackKind::fan ? BossAttackKind::rake : BossAttackKind::fan;
      phase(state, BossPhase::recovery, tuning::boss_recovery_seconds);
    }
  } else if (state.phase == BossPhase::braking) {
    const float remaining = static_cast<float>(state.remaining_ticks);
    actor.view.velocity = scaled(actor.view.velocity, (remaining - 1) / remaining);
    actor.view.position = add(actor.view.position, scaled(actor.view.velocity, dt));
    if (--state.remaining_ticks == 0) {
      actor.view.velocity = {};
      auto plan = eligible(*this, actor);
      if (plan && !old_warning) {
        state.plan = std::move(plan); state.next_release = 0;
        phase(state, BossPhase::warning, state.plan->warning_seconds);
        present(PresentationEventKind::warning_lock, actor.view.position, actor.view.id);
      } else phase(state, BossPhase::approach, 0);
    }
  } else {
    const float station = view.player.z - view.runner_distance;
    if (!state.behind && station > tuning::boss_station_switch_high) state.behind = true;
    if (state.behind && station < tuning::boss_station_switch_low) state.behind = false;
    const float edge = tuning::field_half_width - tuning::military_body_radius[3];
    float flank = state.flank_right ? tuning::tactics_flank_offset : -tuning::tactics_flank_offset;
    if (std::abs(view.player.x + flank) > edge) { state.flank_right = !state.flank_right; flank = -flank; }
    float x = std::clamp(view.player.x + flank, -edge, edge);
    float destination = std::clamp(station + (state.behind ? -1.F : 1.F) * tuning::boss_standoff_metres,
        tuning::boss_station_minimum, tuning::boss_station_maximum);
    const float current = actor.view.position.z - view.runner_distance;
    if ((destination - station) * (current - station) < 0 &&
        std::abs(actor.view.position.x - view.player.x) < tuning::military_body_radius[3] + tuning::player_radius + tuning::boss_bypass_margin)
      destination = current;
    const float station_z = view.runner_distance + destination;
    const auto occupied = [&](Vec2 target, bool all_bodies = false) {
      const Vec2 route{target.x - actor.view.position.x, target.z - actor.view.position.z};
      const float square = length_squared(route);
      for (const auto& other : actors) {
        if (&other == &actor || other.removed || !(all_bodies ? military(other.view.kind) : vehicle(other.view.kind))) continue;
        const Vec2 offset{other.view.position.x - actor.view.position.x, other.view.position.z - actor.view.position.z};
        const float along = square > .000001F ? std::clamp((offset.x * route.x + offset.z * route.z) / square, 0.F, 1.F) : 0;
        const float gap = tuning::military_body_radius[3] + tuning::military_body_radius[military_index(other.view.kind)] + tuning::military_spacing_margin;
        if (distance_squared(other.view.position, add(actor.view.position, scaled(route, along))) < gap * gap) return true;
      }
      return false;
    };
    // Prefer an entirely clear station route. A blocked diagonal can instead
    // admit a horizontal detour without changing any occupied body root.
    const float alternative = view.player.x - flank;
    const bool blocked = occupied({x, station_z});
    const bool alternative_legal = std::abs(alternative) <= edge;
    const bool alternative_blocked = !alternative_legal || occupied({alternative, station_z});
    bool lateral_bypass = false;
    if (blocked && !alternative_blocked) {
      state.flank_right = !state.flank_right; x = alternative;
    } else if (blocked && alternative_blocked) {
      // Two blocked station diagonals need not block the horizontal corridor
      // underneath them. Retain the stored lateral destination until reached;
      // only then choose the opposite clear side, so the detour cannot toggle.
      if (alternative_legal && std::abs(x - actor.view.position.x) <= tuning::tactics_arrival_radius &&
          !occupied({alternative, actor.view.position.z})) {
        state.flank_right = !state.flank_right; x = alternative;
      }
      lateral_bypass = !occupied({x, actor.view.position.z});
    }
    const Vec2 route{x - actor.view.position.x, lateral_bypass ? 0.F : station_z - actor.view.position.z};
    const float arrival = std::min(1.F, std::sqrt(length_squared(route)) / tuning::tactics_arrival_radius);
    const float slow = actor.slow_timer > 0 ? tuning::storm_boss_slow : 0;
    auto relative = scaled(direction(route), tuning::military_movement_speed[3] * arrival * (1 - slow));
    const Vec2 carry{0, lateral_bypass ? 0.F : view.forward_speed};
    const auto desired = add(relative, carry);
    const float change = tuning::boss_movement_acceleration * dt;
    relative = {actor.view.velocity.x + std::clamp(desired.x - actor.view.velocity.x, -change, change),
        actor.view.velocity.z + std::clamp(desired.z - actor.view.velocity.z, -change, change) - carry.z};
    if (lateral_bypass) {
      // The whole lateral corridor is already admitted. Follow the bounded
      // world transition, including the real forward braking drift, rather
      // than rotating a carry-relative velocity back toward the field edge.
      const auto clear_step = [&](Vec2 velocity) {
        const auto next = add(actor.view.position, scaled(velocity, dt));
        return std::abs(next.x) <= std::max(edge, std::abs(actor.view.position.x)) + .00001F && !occupied(next, true);
      };
      auto velocity = relative;
      if (!clear_step(velocity)) {
        velocity = {actor.view.velocity.x + std::clamp(-actor.view.velocity.x, -change, change),
            actor.view.velocity.z + std::clamp(-actor.view.velocity.z, -change, change)};
        if (!clear_step(velocity)) velocity = {};
      }
      actor.view.velocity = velocity;
    } else {
      actor.view.velocity = military_velocity(actor, relative, dt, carry);
    }
    actor.view.position = add(actor.view.position, scaled(actor.view.velocity, dt));
    const float toward = std::atan2(view.player.x - actor.view.position.x, -(view.player.z - actor.view.position.z));
    state.body_heading = wrap(state.body_heading + std::clamp(wrap(toward - state.body_heading),
        -tuning::boss_body_turn_radians_per_second * dt, tuning::boss_body_turn_radians_per_second * dt));
    if (state.remaining_ticks > 0 && --state.remaining_ticks == 0 && state.phase == BossPhase::recovery)
      phase(state, BossPhase::reposition, state.plan->kind == BossAttackKind::fan ? tuning::boss_fan_reposition_seconds : tuning::boss_rake_reposition_seconds);
  }
  actor.view.body_heading = state.body_heading;
  actor.view.warning_seconds = state.phase == BossPhase::warning ? static_cast<float>(state.remaining_ticks) / tuning::fixed_hz : 0;
  actor.view.fire_recovery_seconds = state.phase == BossPhase::recovery ? static_cast<float>(state.remaining_ticks) / tuning::fixed_hz : 0;
}
bool Game::State::valid_boss() const {
  const auto count = std::count_if(actors.begin(), actors.end(), [](const Actor& actor) { return !actor.removed && actor.view.kind == EntityKind::boss; });
  if (!boss) return count == 0;
  const auto* actor = owner(*this);
  const auto& state = *boss;
  if (count != 1 || !actor || state.next_kind > BossAttackKind::rake || state.phase > BossPhase::salvo ||
      state.remaining_ticks > state.total_ticks || state.next_release > 3 ||
      !std::isfinite(state.body_heading) || std::abs(state.body_heading) > 4 || (state.plan && !valid_plan(*state.plan))) return false;
  if (!state.plan && state.next_release != 0) return false;
  const float warning = state.phase == BossPhase::warning ? static_cast<float>(state.remaining_ticks) / tuning::fixed_hz : 0;
  const float recovery = state.phase == BossPhase::recovery ? static_cast<float>(state.remaining_ticks) / tuning::fixed_hz : 0;
  if (!near(actor->view.warning_seconds, warning) || !near(actor->view.fire_recovery_seconds, recovery)) return false;
  const bool completed = !state.plan || (state.next_release == 3 && state.next_kind != state.plan->kind);
  const bool planted = state.phase == BossPhase::warning || state.phase == BossPhase::salvo;
  if (planted) {
    if (!state.plan || state.plan->kind != state.next_kind || !near(actor->view.position, state.plan->locked_root) ||
        !near(state.body_heading, state.plan->locked_body_heading) || length_squared(actor->view.velocity) != 0 ||
        bullets.size() + 3 - state.next_release > tuning::projectile_active_cap) return false;
    if (std::any_of(actors.begin(), actors.end(), [&](const Actor& other) {
      return !other.removed && other.view.id != state.actor_id && other.view.warning_seconds > 0;
    })) return false;
  }
  switch (state.phase) {
  case BossPhase::approach: return completed && state.total_ticks == 0 && state.remaining_ticks == 0;
  case BossPhase::braking: return completed && state.total_ticks == ticks(tuning::boss_brake_seconds) && state.remaining_ticks > 0;
  case BossPhase::warning: return state.total_ticks == ticks(state.plan->warning_seconds) && state.remaining_ticks > 0 && state.next_release == 0;
  case BossPhase::salvo: {
    const auto elapsed = state.total_ticks - state.remaining_ticks;
    return state.plan->kind == BossAttackKind::rake && state.total_ticks == ticks(state.plan->salvo_seconds) && state.remaining_ticks > 0 &&
        state.next_release == (elapsed >= ticks(tuning::boss_rake_interval_seconds) ? 2U : 1U);
  }
  case BossPhase::recovery: return state.plan && state.next_release == 3 && state.next_kind != state.plan->kind &&
      state.total_ticks == ticks(tuning::boss_recovery_seconds) && state.remaining_ticks > 0;
  case BossPhase::reposition: return state.plan && state.next_release == 3 && state.next_kind != state.plan->kind &&
      state.total_ticks == ticks(state.plan->kind == BossAttackKind::fan ? tuning::boss_fan_reposition_seconds : tuning::boss_rake_reposition_seconds) &&
      state.remaining_ticks >= ticks(tuning::boss_brake_seconds);
  }
  return false;
}
} // namespace human_resources::gameplay
