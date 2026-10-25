#include "run_state.h"

namespace human_resources::gameplay {
namespace {
using namespace detail;
bool grounded(const Actor& actor) {
  return !actor.removed && biological(actor.view.kind) && !actor.lifted && actor.view.capture_progress == 0;
}
float radius(const Actor& actor) {
  return tuning::biological_body_radius[static_cast<std::size_t>(actor.view.kind)];
}
Vec2 normal(Vec2 delta, std::uint32_t first, std::uint32_t second) {
  if (length_squared(delta) > 0.00000001F) {
    return direction(delta);
  }
  // Coincident roots need a stable direction without consuming encounter RNG.
  const auto key = first * 1664525U + second * 1013904223U;
  return (key & 1U) ? Vec2{0, (key & 2U) ? 1.F : -1.F} : Vec2{(key & 2U) ? 1.F : -1.F, 0};
}
struct Motion final {
  Actor* actor{};
  Vec2 origin, preferred, position;
};
void displace(Motion& motion, Vec2 offset, float maximum) {
  const auto proposed = add(motion.position, offset);
  Vec2 total{proposed.x - motion.preferred.x, proposed.z - motion.preferred.z};
  if (length_squared(total) > maximum * maximum) {
    total = scaled(direction(total), maximum);
  }
  motion.position = add(motion.preferred, total);
  motion.position.x = std::clamp(motion.position.x, -tuning::field_half_width, tuning::field_half_width);
}
} // namespace

bool Game::State::place_biological(detail::Actor& target, float lateral_sample, float forward_sample, bool initial) {
  const float edge = tuning::field_half_width - radius(target);
  const Vec2 formation = target.view.position;
  const float jitter = initial ? .08F : .14F;
  for (unsigned attempt = 0; attempt < tuning::biological_spawn_candidates; ++attempt) {
    // Preserve the director's authored herd. Candidate offsets resolve only
    // local collisions; a blocked entry must not scatter across the whole field.
    const float sign = (attempt & 1U) ? 1.F : -1.F;
    target.view.position = {std::clamp(formation.x + jitter * (lateral_sample - .5F) +
        (attempt ? sign * radius(target) * static_cast<float>(attempt) : 0.F), -edge, edge),
        formation.z + jitter * (forward_sample - .5F) + static_cast<float>(attempt) * radius(target)};
    bool clear = true;
    for (const auto& other : actors) {
      if (!grounded(other)) {
        continue;
      }
      const float clearance = radius(target) + radius(other);
      if (distance_squared(target.view.position, other.view.position) < clearance * clearance) {
        clear = false;
        break;
      }
    }
    if (clear) {
      return true;
    }
  }
  return false;
}

void Game::State::move_biological(float dt) {
  if (!(dt > 0)) {
    return;
  }
  // Fixed passive capacity; no allocation or spatial framework.
  std::array<Motion, tuning::biological_active_cap> motions{};
  std::size_t count = 0;
  for (auto& actor : actors) {
    if (actor.view.kind >= EntityKind::soldier || !grounded(actor)) {
      continue;
    }
    if (count == motions.size()) {
      break;
    }
    const float offset = actor.view.position.x - view.player.x;
    const float side = std::abs(offset) > .1F ? (offset > 0 ? 1.F : -1.F) : (actor.view.id & 1U ? 1.F : -1.F);
    // Everyone keeps jogging forward. Only the local UFO threat redirects that
    // individual heading into escape; distant herds do not form global rails.
    const float fear = std::clamp(1 - std::sqrt(distance_squared(actor.view.position, view.player)) /
        tuning::tactics_civilian_fear_radius, 0.F, 1.F);
    const auto heading_key = actor.view.id * 2654435761U;
    const float travel = (2 * static_cast<float>(heading_key >> 8) / 16777215.F - 1) *
        tuning::tactics_civilian_travel_lateral_bias;
    const float lateral = travel * (1 - fear) + side * tuning::tactics_civilian_lateral_bias * fear;
    const float travel_side = lateral >= 0 ? 1.F : -1.F;
    // Near an edge, turn forward instead of bouncing back toward the threat.
    const float lateral_space = std::clamp((tuning::field_half_width - radius(actor) - travel_side * actor.view.position.x) / 1.5F, 0.F, 1.F);
    const float speed = tuning::biological_movement_speed[static_cast<std::size_t>(actor.view.kind)];
    const auto desired = scaled(direction({lateral * lateral_space, 1}), speed);
    const float turn = tuning::tactics_civilian_turn_acceleration * dt;
    actor.locomotion_velocity.x += std::clamp(desired.x - actor.locomotion_velocity.x, -turn, turn);
    actor.locomotion_velocity.z += std::clamp(desired.z - actor.locomotion_velocity.z, -turn, turn);
    auto preferred = add(actor.view.position, scaled(actor.locomotion_velocity, dt));
    preferred.x = std::clamp(preferred.x, -tuning::field_half_width, tuning::field_half_width);
    motions[count++] = {&actor, actor.view.position, preferred, preferred};
  }
  std::sort(motions.begin(), motions.begin() + static_cast<std::ptrdiff_t>(count), [](const Motion& a, const Motion& b) {
    return a.actor->view.id < b.actor->view.id;
  });
  const float maximum = tuning::biological_separation_speed * dt;
  for (unsigned pass = 0; pass < 2; ++pass) {
    for (std::size_t i = 0; i < count; ++i) {
      auto& a = motions[i];
      for (std::size_t j = i + 1; j < count; ++j) {
        auto& b = motions[j];
        const Vec2 delta{b.position.x - a.position.x, b.position.z - a.position.z};
        const float clearance = radius(*a.actor) + radius(*b.actor);
        const float distance = std::sqrt(length_squared(delta));
        if (distance >= clearance) {
          continue;
        }
        const auto push = scaled(normal(delta, a.actor->view.id, b.actor->view.id), (clearance - distance) * .5F);
        displace(a, scaled(push, -1), maximum);
        displace(b, push, maximum);
      }
      // Military motion, warning commits and contact rules remain untouched.
      // Passive bodies yield to uncaptured soldiers as fixed local obstacles.
      for (const auto& other : actors) {
        if (other.view.kind != EntityKind::soldier || !grounded(other)) {
          continue;
        }
        const Vec2 delta{a.position.x - other.view.position.x, a.position.z - other.view.position.z};
        const float clearance = radius(*a.actor) + radius(other);
        const float distance = std::sqrt(length_squared(delta));
        if (distance < clearance) {
          displace(a, scaled(normal(delta, a.actor->view.id, other.view.id), clearance - distance), maximum);
        }
      }
    }
  }
  for (std::size_t i = 0; i < count; ++i) {
    auto& motion = motions[i];
    motion.actor->view.position = motion.position;
    motion.actor->view.velocity = scaled({motion.position.x - motion.origin.x, motion.position.z - motion.origin.z}, 1 / dt);
  }
}
} // namespace human_resources::gameplay
