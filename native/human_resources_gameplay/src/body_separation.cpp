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
  const float near = initial ? 1.F : tuning::enemy_spawn_ahead * .55F;
  for (unsigned attempt = 0; attempt < tuning::biological_spawn_candidates; ++attempt) {
    // Fixed candidates reuse existing samples; blocked attempts never alter the RNG schedule.
    const float x_sample = lateral_sample + static_cast<float>(attempt) * .381966011F;
    const float z_sample = forward_sample + static_cast<float>(attempt) * .618033989F;
    target.view.position = {-edge + 2 * edge * (x_sample - std::floor(x_sample)),
        static_cast<float>(view.runner_distance) + near + (tuning::enemy_spawn_ahead - near) * (z_sample - std::floor(z_sample))};
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
  // Production passive population is capped at 28; no allocation or spatial framework.
  std::array<Motion, tuning::biological_active_cap> motions{};
  std::size_t count = 0;
  for (auto& actor : actors) {
    if (actor.view.kind >= EntityKind::soldier || !grounded(actor)) {
      continue;
    }
    if (count == motions.size()) {
      break;
    }
    auto preferred = add(actor.view.position, scaled(actor.wander_velocity, dt));
    if (std::fabs(preferred.x) > tuning::field_half_width) {
      preferred.x = std::clamp(preferred.x, -tuning::field_half_width, tuning::field_half_width);
      actor.wander_velocity.x = -actor.wander_velocity.x;
    }
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
