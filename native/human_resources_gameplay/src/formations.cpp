#include "run_state.h"

namespace human_resources::gameplay {
using namespace detail;
namespace {
struct Member final {
  EntityKind kind{EntityKind::soldier};
  InfantryRole role{InfantryRole::rifleman};
  Vec2 offset;
};
struct Formation final { std::array<Member, 4> members{}; unsigned count{}; };
Formation composition(FormationKind kind, bool introduction) {
  const Member left{EntityKind::soldier, InfantryRole::rifleman, {-.85F, 0}};
  const Member right{EntityKind::soldier, InfantryRole::rifleman, {.85F, 2.4F}};
  switch (kind) {
  case FormationKind::scout: return {{{left}}, 1};
  case FormationKind::patrol: return {{{left, right}}, 2};
  case FormationKind::closing:
    return {{{{EntityKind::soldier, InfantryRole::shotgunner, {-.65F, 0}},
        {EntityKind::soldier, InfantryRole::rifleman, {.65F, 2.4F}}}}, 2};
  case FormationKind::rocket_screen:
    return {{{left, {EntityKind::soldier, InfantryRole::rifleman, {.85F, 0}},
        {EntityKind::soldier, InfantryRole::rocketeer, {0, 3}}}}, 3};
  case FormationKind::jeep_escort:
  case FormationKind::tank_advance:
    return {{{{kind == FormationKind::jeep_escort ? EntityKind::jeep : EntityKind::tank, InfantryRole::none, {}},
        {EntityKind::soldier, InfantryRole::rifleman, {-1.2F, 3.3F}},
        {EntityKind::soldier, InfantryRole::rifleman, {1.2F, 3.3F}}}}, introduction ? 1U : 3U};
  case FormationKind::pincer:
    return {{{left, right, {EntityKind::soldier, InfantryRole::rifleman, {-.85F, 6}},
        {EntityKind::soldier, InfantryRole::shotgunner, {.85F, 8.4F}}}}, 4};
  }
  return {};
}
} // namespace
bool Game::State::spawn_formation(FormationKind kind, bool introduction) {
  const auto group = composition(kind, introduction);
  if (group.count == 0) return false;
  std::array<unsigned, 4> needed{}, present{};
  for (unsigned i = 0; i < group.count; ++i) ++needed[military_index(group.members[i].kind)];
  for (const auto& a : actors) if (!a.removed && military(a.view.kind)) ++present[military_index(a.view.kind)];
  for (unsigned i = 0; i < needed.size(); ++i)
    if (needed[i] + present[i] > static_cast<unsigned>(tuning::enemy_active_cap[i])) return false;
  const float side = random_unit() < .5F ? -1.F : 1.F;
  std::array<Vec2, 4> positions{};
  bool admitted = false;
  for (unsigned row = 0; row < tuning::military_spawn_rows && !admitted; ++row) {
    admitted = true;
    for (unsigned i = 0; i < group.count; ++i) {
      const auto& member = group.members[i];
      const float own_side = kind == FormationKind::pincer && i >= 2 ? -side : side;
      const float radius = tuning::military_body_radius[military_index(member.kind)];
      positions[i] = {own_side * (tuning::population_formation_lane + member.offset.x),
          view.runner_distance + tuning::enemy_spawn_ahead + member.offset.z + static_cast<float>(row) * 4};
      if (std::abs(positions[i].x) + radius > tuning::field_half_width ||
          std::abs(positions[i].x) - radius < tuning::encounter_open_route_half_width + tuning::player_radius)
        admitted = false;
      const auto overlaps = [&](Vec2 other, float other_radius) {
        const float gap = radius + other_radius + tuning::military_spacing_margin;
        return distance_squared(positions[i], other) < gap * gap;
      };
      for (const auto& other : actors)
        if (!other.removed && military(other.view.kind) && overlaps(other.view.position,
            tuning::military_body_radius[military_index(other.view.kind)])) admitted = false;
      for (unsigned j = 0; j < i; ++j)
        if (overlaps(positions[j], tuning::military_body_radius[military_index(group.members[j].kind)])) admitted = false;
    }
  }
  if (!admitted) return false;
  const auto before = actors.size();
  const auto identity = next_id;
  for (unsigned i = 0; i < group.count; ++i) {
    const auto& member = group.members[i];
    spawn(member.kind, false, positions[i]);
    if (actors.size() != before + i + 1) {
      actors.resize(before); next_id = identity; return false;
    }
    auto& actor = actors.back();
    if (member.kind == EntityKind::soldier)
      actor.attack_timer *= infantry_attack(member.role).cooldown_seconds / infantry_attack(actor.view.infantry_role).cooldown_seconds;
    actor.view.infantry_role = member.role;
    actor.group_id = next_group;
    const float depth = kind == FormationKind::pincer && i >= 2 ? member.offset.z - 6 : member.offset.z;
    actor.formation = {{positions[i].x, tuning::population_formation_station + depth}, true};
    actor.attack_timer += static_cast<float>(i) * tuning::population_formation_stagger_seconds;
  }
  ++next_group;
  return true;
}
} // namespace human_resources::gameplay
