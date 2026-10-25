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

Vec2 Game::State::military_velocity(const Actor& actor, Vec2 preferred, float dt) const {
  if (!(dt > 0) || length_squared(preferred) < .00000001F)
    return {};
  const float own_radius = radius(actor);
  const float edge = tuning::field_half_width - own_radius;
  // A tiny bounded heading search supplies local obstacle avoidance. Fixed actor
  // order and ID tie-breaking consume neither encounter RNG nor saved state.
  constexpr std::array<float, 9> turns{0, .39269908F, -.39269908F, .78539816F, -.78539816F,
      1.17809725F, -1.17809725F, 1.57079633F, -1.57079633F};
  const float speed = std::sqrt(length_squared(preferred));
  const auto desired = direction(preferred);
  const auto previous = direction(actor.view.velocity);
  Vec2 best{};
  float best_score = -100000.F;
  for (std::size_t candidate = 0; candidate <= turns.size(); ++candidate) {
    const auto velocity = candidate == turns.size() ? Vec2{} : rotated(preferred, turns[candidate]);
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
    const auto heading = direction(velocity);
    float score = (velocity.x * desired.x + velocity.z * desired.z) / speed - 3 * proximity;
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
} // namespace human_resources::gameplay
