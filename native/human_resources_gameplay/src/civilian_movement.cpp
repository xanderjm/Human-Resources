#include "run_state.h"

namespace human_resources::gameplay {
using namespace detail;
Vec2 Game::State::civilian_heading(Actor& actor, float dt) {
  const auto kind = actor.view.kind;
  const auto index = static_cast<std::size_t>(kind);
  auto& escape = actor.civilian_escape;
  const auto key = actor.view.id * 2654435761U;
  const float drift = (2 * static_cast<float>(key >> 8) / 16777215.F - 1) * tuning::tactics_civilian_travel_lateral_bias;
  const Actor* leader = &actor;
  bool afraid = distance_squared(actor.view.position, view.player) < tuning::civilian_fear_radius[index] * tuning::civilian_fear_radius[index];
  if (kind == EntityKind::sheep && actor.group_id) {
    for (const auto& other : actors) {
      if (other.removed || other.lifted || other.view.kind != kind || other.group_id != actor.group_id ||
          distance_squared(actor.view.position, other.view.position) > tuning::civilian_flock_radius * tuning::civilian_flock_radius) continue;
      if (other.view.id < leader->view.id) leader = &other;
      afraid = afraid || distance_squared(other.view.position, view.player) < tuning::civilian_fear_radius[index] * tuning::civilian_fear_radius[index];
    }
  }
  escape.alert_seconds = afraid ? std::min(16.F, escape.alert_seconds + dt) : 0;
  escape.remaining_seconds = std::max(0.F, escape.remaining_seconds - dt);
  const float delay = tuning::civilian_reaction_seconds[index] +
      (leader != &actor ? tuning::civilian_flock_follow_seconds : 0.F);
  if (afraid && escape.alert_seconds >= delay && escape.remaining_seconds == 0) {
    const float away = actor.view.position.x - view.player.x;
    const float side = std::abs(away) > .1F ? (away > 0 ? 1.F : -1.F) : (actor.view.id & 1U ? 1.F : -1.F);
    const float spread = kind == EntityKind::pig ? (actor.view.id & 1U ? 1.5F : .65F) : 1.F;
    Vec2 selected = direction({side * tuning::civilian_lateral_bias[index] * spread, 1});
    if (kind == EntityKind::farmer) {
      // Choose once per escape leg. An open opposite-side route can beat a
      // crowded outward edge, without changing direction every rendered frame.
      float best = -std::numeric_limits<float>::infinity();
      for (float lateral : {side * tuning::civilian_lateral_bias[index], 0.F, -side * tuning::civilian_lateral_bias[index]}) {
        const auto heading = direction({lateral, 1});
        const auto future = add(actor.view.position, scaled(heading, tuning::biological_movement_speed[index]));
        if (std::abs(future.x) + tuning::biological_clearance_radius[index] > tuning::field_half_width) continue;
        float score = std::sqrt(distance_squared(future, view.player));
        for (const auto& other : actors) {
          if (&other == &actor || other.removed || other.lifted || other.view.kind >= EntityKind::soldier) continue;
          const float gap = tuning::biological_clearance_radius[index] + tuning::biological_clearance_radius[static_cast<std::size_t>(other.view.kind)];
          score -= 3 * std::max(0.F, gap + .5F - std::sqrt(distance_squared(future, other.view.position)));
        }
        if (score > best) { best = score; selected = heading; }
      }
    } else if (leader != &actor && length_squared(leader->civilian_escape.heading) > .5F) {
      selected = leader->civilian_escape.heading;
    }
    escape.heading = selected;
    escape.remaining_seconds = tuning::civilian_hold_seconds[index];
  }
  auto heading = escape.remaining_seconds > 0 ? escape.heading : direction({drift, 1});
  if (kind == EntityKind::sheep && !afraid && actor.group_id) {
    Vec2 centre{};
    unsigned neighbours = 0;
    for (const auto& other : actors) {
      if (&other == &actor || other.removed || other.lifted || other.view.kind != kind || other.group_id != actor.group_id ||
          distance_squared(actor.view.position, other.view.position) > tuning::civilian_flock_radius * tuning::civilian_flock_radius) continue;
      centre = add(centre, other.view.position); ++neighbours;
    }
    if (neighbours) heading = direction({drift + std::clamp((centre.x / static_cast<float>(neighbours) - actor.view.position.x) * .15F, -.2F, .2F), 1});
  }
  // Gradually straighten at an edge; never bounce a fleeing body back at the UFO.
  const float side = heading.x >= 0 ? 1.F : -1.F;
  const float room = std::clamp((tuning::field_half_width - tuning::biological_clearance_radius[index] - side * actor.view.position.x) / 1.5F, 0.F, 1.F);
  return direction({heading.x * room, heading.z});
}
} // namespace human_resources::gameplay
