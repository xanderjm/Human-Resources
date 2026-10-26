#include "run_state.h"

namespace human_resources::gameplay {
namespace {
using namespace detail;
float radius(const Actor& actor) {
  return tuning::military_body_radius[military_index(actor.view.kind)];
}
bool obstacle(const Actor& actor) {
  // Keep the ground projection reserved throughout capture. A cancelled beam
  // must not return a body into an approach lane occupied while it was lifted.
  return !actor.removed && military(actor.view.kind);
}
float segment_distance_squared(Vec2 point, Vec2 origin, Vec2 travel) {
  const Vec2 offset{point.x - origin.x, point.z - origin.z};
  const float square = length_squared(travel);
  const float time = square > .00000001F ? std::clamp((offset.x * travel.x + offset.z * travel.z) / square, 0.F, 1.F) : 0.F;
  return distance_squared(point, add(origin, scaled(travel, time)));
}
Vec2 rotated(Vec2 value, float angle) {
  const float c = std::cos(angle), s = std::sin(angle);
  return {value.x * c - value.z * s, value.x * s + value.z * c};
}
} // namespace

void Game::State::place_military(Actor& target) {
  const float edge = tuning::field_half_width - radius(target);
  const auto entry = target.view.position;
  // Retain every scheduled arrival and the authored open central route. Search
  // only forward at ingress, never teleport an already live/committed actor.
  const float side = entry.x >= 0 ? 1.F : -1.F;
  const float inner = target.view.kind == EntityKind::boss ? 0.F : tuning::military_contact_radius[military_index(target.view.kind)] + tuning::player_radius + tuning::encounter_open_route_half_width;
  const auto lanes = target.view.kind == EntityKind::boss ? std::array<float, 5>{entry.x, tuning::encounter_boss_entry_half_width,
                                                                -tuning::encounter_boss_entry_half_width, 0, 0}
                                                          : std::array<float, 5>{std::clamp(entry.x, -edge, edge), side * edge,
                                                                -side * edge, side * inner, -side * inner};
  for (unsigned row = 0; row < tuning::military_spawn_rows; ++row) {
    for (float lane : lanes) {
      target.view.position = {lane,
          entry.z + static_cast<float>(row) * (2 * radius(target) + tuning::military_spacing_margin)};
      bool clear = true;
      for (const auto& other : actors) {
        if (!obstacle(other))
          continue;
        const float gap = radius(target) + radius(other) + tuning::military_spacing_margin;
        if (distance_squared(target.view.position, other.view.position) < gap * gap) {
          clear = false;
          break;
        }
      }
      if (clear)
        return;
    }
  }
  // Full-cap ingress is tested across fixed seeds. Preserve a scheduled arrival
  // if future tuning exhausts the bounded search, instead of dropping pressure.
}

Vec2 Game::State::military_velocity(const Actor& actor, Vec2 preferred, float dt, Vec2 carry) const {
  if (!(dt > 0) || (length_squared(preferred) < .00000001F && length_squared(carry) < .00000001F))
    return {};
  const float own_radius = radius(actor);
  const float edge = tuning::field_half_width - own_radius;
  // A tiny bounded heading search supplies local obstacle avoidance. Fixed actor
  // order and ID tie-breaking consume neither encounter RNG nor saved state.
  constexpr std::array<float, 9> turns{0, .39269908F, -.39269908F, .78539816F, -.78539816F,
      1.17809725F, -1.17809725F, 1.57079633F, -1.57079633F};
  const float speed = std::max(.0001F, std::sqrt(length_squared(preferred)));
  const auto desired = direction(preferred);
  const auto previous = direction({actor.view.velocity.x - carry.x, actor.view.velocity.z - carry.z});
  Vec2 best{};
  float best_score = -100000.F;
  for (std::size_t candidate = 0; candidate <= turns.size(); ++candidate) {
    // Boss carry is not steerable speed. Test the actual world sweep after
    // rotating only its relative steering; the final candidate stops in world.
    const auto velocity = candidate == turns.size() ? Vec2{} : add(carry, rotated(preferred, turns[candidate]));
    const auto travel = scaled(velocity, dt);
    const auto position = add(actor.view.position, travel);
    // Capture pull can release a soldier beyond its whole-body edge. Permit
    // ordinary inward travel, never increase that pre-existing excursion.
    if (std::abs(position.x) > std::max(edge, std::abs(actor.view.position.x)) + .00001F)
      continue;
    float proximity = 0;
    bool clear = true;
    for (const auto& other : actors) {
      if (&other == &actor || !obstacle(other))
        continue;
      const float gap = own_radius + radius(other) + tuning::military_spacing_margin;
      const float before = distance_squared(actor.view.position, other.view.position);
      const float swept = segment_distance_squared(other.view.position, actor.view.position, travel);
      // A malformed/test initial overlap may move outward at ordinary speed;
      // no corrective root displacement and no further penetration is allowed.
      if (swept < std::min(gap * gap, before) - .00001F) {
        clear = false;
        break;
      }
      const auto forecast = scaled(velocity, tuning::military_spacing_lookahead);
      const float future = std::sqrt(segment_distance_squared(other.view.position, actor.view.position, forecast));
      proximity += std::max(0.F, gap + .35F - future);
    }
    if (!clear)
      continue;
    // Stopping in world is a fallback, not extra steerable upstream speed.
    // Otherwise carry can make this candidate beat genuine acceleration.
    const Vec2 relative = candidate == turns.size() ? Vec2{} : Vec2{velocity.x - carry.x, velocity.z - carry.z};
    const auto heading = direction(relative);
    float score = (relative.x * desired.x + relative.z * desired.z) / speed - 3 * proximity;
    // Prefer continuing a chosen side around a neighbor instead of alternating
    // left/right every tick. This is derived from already-persisted velocity.
    score += .25F * (heading.x * previous.x + heading.z * previous.z);
    if (candidate < turns.size() && turns[candidate] != 0)
      score += ((actor.view.id & 1U) == (turns[candidate] > 0) ? .001F : 0.F);
    if (score > best_score) {
      best_score = score;
      best = velocity;
    }
  }
  return best;
}

detail::InfantryMotion Game::State::infantry_motion(const Actor& actor, Vec2 destination, float dt) const {
  auto facing = direction(actor.locomotion_velocity);
  if (length_squared(facing) == 0) facing = {0, -1};
  if (!(dt > 0)) return {{}, facing};
  const float edge = std::max(tuning::field_half_width - radius(actor), std::abs(actor.view.position.x));
  const Vec2 route{destination.x - actor.view.position.x, destination.z - actor.view.position.z};
  const auto desired = length_squared(route) > .00000001F ? direction(route) : facing;
  const float maximum = tuning::military_movement_speed[0] * (actor.slow_timer > 0 ? 1 - tuning::storm_slow : 1);
  const float requested = maximum * std::min(1.F, std::sqrt(length_squared(route)) / tuning::tactics_arrival_radius);
  const auto clear = [&](Vec2 travel) {
    if (std::abs(actor.view.position.x + travel.x) > edge + .00001F) return false;
    for (const auto& other : actors) {
      if (&other == &actor || !obstacle(other)) continue;
      const float gap = radius(actor) + radius(other) + tuning::military_spacing_margin;
      const float before = distance_squared(actor.view.position, other.view.position);
      if (segment_distance_squared(other.view.position, actor.view.position, travel) < std::min(gap * gap, before) - .00001F)
        return false;
    }
    return true;
  };
  Vec2 guide = facing;
  float best = -100000;
  const auto consider = [&](Vec2 heading, bool positive_turn) {
    if (!clear(scaled(heading, maximum * dt))) return;
    float proximity = 0;
    const auto forecast = scaled(heading, maximum * tuning::military_spacing_lookahead);
    const float future_x = actor.view.position.x + forecast.x;
    proximity += std::max(0.F, std::abs(future_x) - edge);
    for (const auto& other : actors) {
      if (&other == &actor || !obstacle(other)) continue;
      const float gap = radius(actor) + radius(other) + tuning::military_spacing_margin;
      proximity += std::max(0.F, gap + .35F - std::sqrt(segment_distance_squared(other.view.position, actor.view.position, forecast)));
    }
    const float score = heading.x * desired.x + heading.z * desired.z - 3 * proximity +
        .4F * (heading.x * facing.x + heading.z * facing.z) + .001F * ((actor.view.id & 1U) == positive_turn);
    if (score > best) { best = score; guide = heading; }
  };
  // Steering chooses a guide; it no longer becomes instantaneous translation
  // or whole-root yaw. A stopped person can pivot toward a rear opening too.
  for (unsigned i = 0; i < 16; ++i) {
    const int offset = i == 0 ? 0 : static_cast<int>((i + 1) / 2) * (i & 1U ? 1 : -1);
    consider(rotated(desired, static_cast<float>(offset) * .39269908F), offset > 0);
  }
  // A field/body tangent can be narrower than the desired-relative samples.
  // Preserve exact boundary-parallel openings and a still-clear physical step;
  // otherwise every sample can be blocked while a person has room to turn.
  for (const auto heading : {Vec2{0, -1}, Vec2{0, 1}, facing})
    consider(heading, desired.x * heading.z - desired.z * heading.x > 0);
  if (best == -100000) return {{}, facing};
  facing = infantry_turn(facing, guide, dt);
  const float alignment = std::max(0.F, facing.x * guide.x + facing.z * guide.z);
  const float goal = requested * alignment * alignment;
  const float previous = std::sqrt(length_squared(actor.view.velocity));
  const float change = (goal < previous ? tuning::infantry_braking : tuning::infantry_acceleration) * dt;
  const float speed = previous + std::clamp(goal - previous, -change, change);
  const auto velocity = scaled(facing, speed);
  // A human may plant and keep pivoting; the circular ground reservation does
  // not change with that turn. Actual accepted travel still owns the full sweep.
  return {clear(scaled(velocity, dt)) ? velocity : Vec2{}, facing};
}

detail::VehicleMotion Game::State::vehicle_motion(const Actor& actor, Vec2 destination, float dt) const {
  auto facing = direction(actor.locomotion_velocity);
  if (length_squared(facing) == 0) facing = direction(actor.view.velocity);
  if (length_squared(facing) == 0) facing = {0, -1};
  if (!(dt > 0) || !vehicle(actor.view.kind)) return {{}, facing, actor.escape};
  const auto dot = [](Vec2 a, Vec2 b) { return a.x * b.x + a.z * b.z; };
  const float nominal = tuning::military_movement_speed[military_index(actor.view.kind)];
  const auto route_direction = [&](Vec2 position, Vec2 forward) {
    const Vec2 route{destination.x - position.x, destination.z - position.z};
    return length_squared(route) > .00000001F ? direction(route) : forward;
  };
  const auto requested_speed = [&](Vec2 position, float slow) {
    const float arrival = std::min(1.F, std::sqrt(distance_squared(destination, position)) / tuning::tactics_arrival_radius);
    return nominal * (slow > 0 ? 1 - tuning::storm_slow : 1) * arrival;
  };
  const auto desired = route_direction(actor.view.position, facing);
  const float horizon = std::max(tuning::vehicle_recovery_clearance_metres,
      nominal * tuning::military_spacing_lookahead + nominal * nominal / (2 * tuning::vehicle_braking));
  const float allowed = std::max(tuning::field_half_width - radius(actor), std::abs(actor.view.position.x));
  const auto clearance = [&](Vec2 origin, Vec2 heading) {
    float clear = horizon;
    if (heading.x > .00001F) clear = std::min(clear, (allowed - origin.x) / heading.x);
    if (heading.x < -.00001F) clear = std::min(clear, (-allowed - origin.x) / heading.x);
    for (const auto& other : actors) {
      if (&actor == &other || !obstacle(other)) continue;
      const Vec2 offset{other.view.position.x - origin.x, other.view.position.z - origin.z};
      const float along = dot(offset, heading);
      if (along <= 0) continue;
      const float before = length_squared(offset);
      const float gap = radius(actor) + radius(other) + tuning::military_spacing_margin;
      const float across = std::max(0.F, before - along * along);
      const float square = std::min(gap * gap, before);
      if (across < square - .000001F)
        clear = std::min(clear, along - std::sqrt(std::max(0.F, square - across)));
    }
    return std::max(0.F, clear);
  };
  const auto next_speed = [&](float before, float goal) {
    if (goal * before < 0) goal = 0;
    const float acceleration = std::abs(goal) < std::abs(before) ? tuning::vehicle_braking : tuning::vehicle_acceleration;
    return before + std::clamp(goal - before, -acceleration * dt, acceleration * dt);
  };
  struct Step final { VehicleMotion motion; bool clear; };
  const auto braking_travel = [&](float speed) {
    // Include this step, then exactly the discrete straight brake available on
    // following ticks. A turned root must retain this entire stopping corridor.
    const float decrement = tuning::vehicle_braking * dt;
    const float steps = std::ceil(std::abs(speed) / decrement);
    return std::max(0.F, dt * (steps * std::abs(speed) - decrement * steps * (steps - 1) * .5F));
  };
  const auto ordinary_guide = [&](Vec2 position, Vec2 forward) {
    const auto wanted = route_direction(position, forward);
    Vec2 guide = wanted;
    float best_score = -1000;
    for (int candidate = 0; candidate < 16; ++candidate) {
      const int offset = candidate == 0 ? 0 : ((candidate + 1) / 2) * (candidate & 1 ? 1 : -1);
      const auto heading = rotated(wanted, static_cast<float>(offset) * .39269908F);
      const float score = 3 * clearance(position, heading) / horizon + dot(heading, wanted) +
          .25F * dot(heading, forward) + .001F * ((actor.view.id & 1U) == (offset > 0));
      if (score > best_score) { best_score = score; guide = heading; }
    }
    return guide;
  };
  struct OrdinaryStep final { Step step; bool blocked; };
  const auto ordinary_step = [&](Vec2 position, Vec2 forward, float previous_speed, float slow) {
    const float requested = requested_speed(position, slow);
    const auto guide = ordinary_guide(position, forward);
    const float forward_clear = clearance(position, forward);
    const bool blocked = requested > .0001F && (forward_clear < tuning::vehicle_recovery_clearance_metres || dot(forward, guide) < -.25F);
    const float admitted = std::sqrt(2 * tuning::vehicle_braking * std::max(0.F, forward_clear - .02F));
    const float speed = next_speed(previous_speed, blocked ? 0.F : std::min(requested, admitted));
    const float angle = std::atan2(forward.x * guide.z - forward.z * guide.x, dot(forward, guide));
    const float turn_limit = blocked ? 0.F : std::min(tuning::vehicle_turn_radians_per_second * dt,
        std::abs(speed) * dt / tuning::vehicle_minimum_turn_radius_metres);
    const auto heading = direction(rotated(forward, std::clamp(angle, -turn_limit, turn_limit)));
    const auto safe = [&](Vec2 next_facing, float next) {
      return clearance(position, next < 0 ? scaled(next_facing, -1) : next_facing) + .00001F >= braking_travel(next);
    };
    if (safe(heading, speed))
      return OrdinaryStep{{{scaled(heading, speed), heading, {}}, true}, blocked};
    // The previous accepted frame reserved a straight brake. Do not rotate
    // into a short clear segment that leaves no room for the following brake.
    const float braking = next_speed(previous_speed, 0);
    const bool clear = safe(forward, braking);
    return OrdinaryStep{{{clear ? scaled(forward, braking) : Vec2{}, forward, {}}, clear}, blocked};
  };
  // This exact fixed-step transition is both the primitive simulator and the
  // executor. Admission therefore includes actual ramp, turn, braking drift and
  // exit travel; no sampled curve is replaced by another guide during motion.
  const auto primitive_step = [&](Vec2 position, Vec2 forward, float speed, VehicleEscape escape, float slow) {
    const float maximum = nominal * (slow > 0 ? 1 - tuning::storm_slow : 1);
    const float goal = escape.phase == VehicleEscapePhase::reverse_turn ? -maximum * tuning::vehicle_reverse_speed_fraction :
        escape.phase == VehicleEscapePhase::forward_exit ? maximum : 0;
    const float actual = next_speed(speed, goal);
    float turn = 0;
    if (escape.phase == VehicleEscapePhase::reverse_turn) {
      const float limit = std::min(tuning::vehicle_turn_radians_per_second * dt,
          std::abs(actual) * dt / tuning::vehicle_minimum_turn_radius_metres);
      turn = std::clamp(escape.turn_remaining, -limit, limit);
    }
    const auto heading = rotated(forward, turn);
    const auto velocity = scaled(heading, actual);
    if (clearance(position, actual < 0 ? scaled(heading, -1) : heading) + .00001F < std::abs(actual) * dt)
      return Step{{{}, forward, escape}, false};
    if (escape.phase == VehicleEscapePhase::reverse_turn) {
      escape.turn_remaining -= turn;
      if (std::abs(escape.turn_remaining) < .000001F) {
        escape.turn_remaining = 0;
        escape.phase = VehicleEscapePhase::reverse_brake;
      }
    } else if (escape.phase == VehicleEscapePhase::reverse_brake && std::abs(actual) < .00001F) {
      escape.phase = VehicleEscapePhase::forward_exit;
    } else if (escape.phase == VehicleEscapePhase::forward_exit) {
      escape.forward_remaining = std::max(0.F, escape.forward_remaining - std::abs(actual) * dt);
      if (escape.forward_remaining == 0) escape = {};
    }
    return Step{{velocity, heading, escape}, true};
  };
  const float previous_speed = dot(actor.view.velocity, facing);
  if (actor.escape.phase != VehicleEscapePhase::none) {
    const auto step = primitive_step(actor.view.position, facing, previous_speed, actor.escape, actor.slow_timer);
    // Only a changed constraint invalidates a committed primitive. Hold without
    // consuming its progress, then admit a new primitive from the real rest pose.
    return step.clear ? step.motion : VehicleMotion{{}, facing, {}};
  }
  const auto ordinary = ordinary_step(actor.view.position, facing, previous_speed, actor.slow_timer);
  if (ordinary.blocked && std::abs(previous_speed) < .00001F) {
    VehicleEscape selected;
    float selected_score = -1000;
    // Sixteen finite local primitives, with impossible prefixes pruned. This
    // is a back-turn/stop/forward-exit choice, not a general navigation search.
    for (float side : {-1.F, 1.F}) for (unsigned turn = 1; turn <= 8; ++turn) {
      const VehicleEscape candidate{VehicleEscapePhase::reverse_turn, side * static_cast<float>(turn) * .39269908F,
          tuning::vehicle_recovery_clearance_metres};
      auto escape = candidate;
      auto position = actor.view.position, heading = facing;
      float speed = 0;
      for (unsigned tick = 0; tick < 600; ++tick) {
        const auto step = primitive_step(position, heading, speed, escape, std::max(0.F, actor.slow_timer - static_cast<float>(tick) * dt));
        if (!step.clear) break;
        position = add(position, scaled(step.motion.velocity, dt));
        heading = step.motion.facing; speed = dot(step.motion.velocity, heading); escape = step.motion.escape;
        if (escape.phase != VehicleEscapePhase::none) continue;
        // A primitive must hand back to the very same runnable ordinary
        // transition, including destination/arrival and physical braking tail.
        // A short exit still inside its blocked region merely repeats recovery.
        const auto handoff = ordinary_step(position, direction(heading), speed,
            std::max(0.F, actor.slow_timer - static_cast<float>(tick + 1) * dt));
        if (handoff.blocked || !handoff.step.clear || length_squared(handoff.step.motion.velocity) < .00000001F) break;
        const Vec2 travel{position.x - actor.view.position.x, position.z - actor.view.position.z};
        const float score = dot(travel, desired) + .25F * dot(heading, desired) - .05F * static_cast<float>(tick + 1) * dt +
            .001F * ((actor.view.id & 1U) == (side > 0));
        if (score > selected_score) { selected_score = score; selected = candidate; }
        break;
      }
    }
    if (selected.phase == VehicleEscapePhase::none) return {{}, facing, {}};
    return primitive_step(actor.view.position, facing, 0, selected, actor.slow_timer).motion;
  }
  return ordinary.step.motion;
}
} // namespace human_resources::gameplay
