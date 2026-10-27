#include "encounter_rules.h"

namespace human_resources::gameplay {
using namespace detail;
void Game::State::director(float dt) {
  const float seconds = static_cast<float>(view.stats.active_seconds);
  const auto window = encounter_window(seconds);
  const bool boss_alive = std::any_of(actors.begin(), actors.end(), [](const Actor& a) {
    return !a.removed && a.view.kind == EntityKind::boss;
  });
  const float rate = std::min(tuning::director_rate_multiplier_cap,
      1 + seconds / 60 * tuning::director_rate_growth_per_minute) *
      tuning::director_pressure * tuning::encounter_military_pressure_multiplier;
  const float pocket = static_cast<float>(tuning::encounter_cohort_size);
  // A blocked delivery retains one pocket, never a queue of invisible backlog.
  biological_budget = std::min(pocket, biological_budget + dt * tuning::biological_spawn_per_second *
      std::max(1.F, view.forward_speed / tuning::forward_scroll_metres_per_second));
  if (biological_budget >= pocket && spawn_cohort()) biological_budget -= pocket;

  if (seconds >= next_boss) {
    if (!boss_alive) {
      const auto before = actors.size();
      spawn(EntityKind::boss);
      if (actors.size() > before) next_boss += tuning::director_boss_interval_seconds;
    }
    return;
  }
  if (window.boss_ingress || boss_alive) return;

  const bool military_enabled = seconds >= tuning::population_rifle_reveal_seconds;
  if (military_enabled && !window.recovery) {
    soldier_budget = std::min(4.F, soldier_budget + dt * tuning::director_soldier_rate * rate * window.soldier_multiplier);
    if (seconds >= tuning::enemy_start_second[1])
      jeep_budget = std::min(1.F, jeep_budget + dt * tuning::director_jeep_rate * rate * window.heavy_multiplier);
    if (seconds >= tuning::enemy_start_second[2])
      tank_budget = std::min(1.F, tank_budget + dt * tuning::director_tank_rate * rate * window.heavy_multiplier);
  }
  const auto live_groups = [this] {
    std::array<std::uint32_t, 32> groups{};
    unsigned count = 0;
    for (const auto& a : actors) {
      if (a.removed || a.lifted || !military(a.view.kind) || a.view.kind == EntityKind::boss) continue;
      const auto id = a.group_id ? a.group_id : a.view.id;
      if (std::find(groups.begin(), groups.begin() + count, id) == groups.begin() + count) {
        if (count == groups.size()) return count;
        groups[count++] = id;
      }
    }
    return count;
  };
  const unsigned limit = seconds < tuning::population_single_formation_until_seconds ? 1U :
      seconds < tuning::enemy_start_second[2] ? tuning::population_early_formation_cap : tuning::population_late_formation_cap;
  if (live_groups() >= limit) return;

  // Successful introductions own bits, so a full population defers rather than
  // loses a reveal. They are authored allowances, not free recurring escorts.
  const std::array<float, 5> reveal{tuning::population_rifle_reveal_seconds,
      tuning::population_shotgun_reveal_seconds, tuning::enemy_start_second[1],
      tuning::population_rocket_reveal_seconds, tuning::enemy_start_second[2]};
  constexpr std::array<FormationKind, 5> arrivals{FormationKind::scout, FormationKind::closing,
      FormationKind::jeep_escort, FormationKind::rocket_screen, FormationKind::tank_advance};
  for (unsigned i = 0; i < reveal.size(); ++i) {
    if (seconds >= reveal[i] && (revealed_threats & (1U << i)) == 0) {
      if (spawn_formation(arrivals[i], true)) {
        revealed_threats = static_cast<std::uint8_t>(revealed_threats | (1U << i));
        soldier_budget = 0; // Intro replaces the pending ordinary arrival.
        if (i == 2) jeep_budget = 0;
        if (i == 4) tank_budget = 0;
      }
      return;
    }
  }
  if (window.recovery || !military_enabled) return;
  if (window.heavy_multiplier > 0 && tank_budget >= 1 && soldier_budget >= 2 &&
      spawn_formation(FormationKind::tank_advance)) {
    tank_budget -= 1; soldier_budget -= 2; return;
  }
  if (window.heavy_multiplier > 0 && jeep_budget >= 1 && soldier_budget >= 2 &&
      spawn_formation(FormationKind::jeep_escort)) {
    jeep_budget -= 1; soldier_budget -= 2; return;
  }
  if (soldier_budget >= 4 && seconds >= tuning::enemy_start_second[2] && window.formation == 2 &&
      spawn_formation(FormationKind::pincer)) { soldier_budget -= 4; return; }
  if (soldier_budget >= 3 && seconds >= tuning::population_rocket_reveal_seconds && window.formation == 1 &&
      spawn_formation(FormationKind::rocket_screen)) { soldier_budget -= 3; return; }
  if (soldier_budget >= 2) {
    const auto pattern = seconds >= tuning::population_shotgun_reveal_seconds && window.formation == 0 ?
        FormationKind::closing : FormationKind::patrol;
    if (spawn_formation(pattern)) soldier_budget -= 2;
  }
}
bool Game::State::spawn_cohort(bool initial, unsigned opening_group) {
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
  constexpr auto farmer = EntityKind::farmer, cow = EntityKind::cow,
      sheep = EntityKind::sheep, pig = EntityKind::pig;
  constexpr std::array<std::array<EntityKind, 4>, 6> pockets{{
      {{sheep, sheep, sheep, sheep}}, {{pig, pig, pig, pig}},
      {{farmer, farmer, farmer, farmer}}, {{cow, cow, cow, cow}},
      {{farmer, farmer, cow, cow}}, {{farmer, sheep, sheep, pig}}}};
  const bool cattle_allowed = !initial && view.stats.active_seconds >= tuning::population_cow_reveal_seconds;
  unsigned family = static_cast<unsigned>(composition % 3U);
  if (cattle_allowed) {
    float draw = static_cast<float>(composition % 100U);
    for (family = 0; family + 1 < pockets.size(); ++family) {
      if (draw < tuning::population_late_pocket_weights[family]) break;
      draw -= tuning::population_late_pocket_weights[family];
    }
  } else if (family == 2 && composition & 4U) family = 5;
  if (pockets_since_easy >= tuning::population_easy_pocket_interval - 1)
    family = static_cast<unsigned>(composition & 1U);
  auto species = pockets[family];
  if (initial && opening_group == 0) species = {farmer, sheep, sheep, pig};
  const bool first_cattle = cattle_allowed && !cattle_introduced;
  if (first_cattle) species = {farmer, sheep, cow, sheep};
  const auto before = actors.size();
  const auto identity = next_id;
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
  if (actors.size() != before + species.size()) {
    actors.resize(before); next_id = identity;
    return false;
  }
  for (std::size_t i = before; i < actors.size(); ++i) actors[i].group_id = next_group;
  ++next_group;
  const bool easy = std::find(species.begin(), species.end(), sheep) != species.end() ||
      std::find(species.begin(), species.end(), pig) != species.end();
  pockets_since_easy = easy ? 0 : static_cast<std::uint8_t>(pockets_since_easy + 1);
  if (first_cattle) cattle_introduced = true;
  return true;
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
    a.view.infantry_role = static_cast<InfantryRole>(1 + std::min(2U,
        static_cast<unsigned>(heading_sample * static_cast<float>(1 +
            (view.stats.active_seconds >= tuning::population_shotgun_reveal_seconds ? 1U : 0U) +
            (view.stats.active_seconds >= tuning::population_rocket_reveal_seconds ? 1U : 0U)))));
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
    a.locomotion_velocity = {0, -tuning::military_movement_speed[index]};
    a.view.body_heading = 0;
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
    if (pocket_position) a.view.position = *pocket_position;
    else place_military(a);
    const float growth = 1 + std::max(0.0F, static_cast<float>(view.stats.active_seconds / 60) - 2) * tuning::director_health_growth_after_2min;
    const float boss_index = static_cast<float>(std::max(0.0, (view.stats.active_seconds - tuning::director_boss_first_second) / tuning::director_boss_interval_seconds));
    const float hp = kind == EntityKind::boss ? tuning::director_boss_base_hp * (1 + boss_index * tuning::director_boss_health_index_coefficient) : tuning::enemy_hp[index] * growth;
    a.view.hull = a.view.maximum_hull = hp;
    const float period = kind == EntityKind::soldier ? infantry_attack(a.view.infantry_role).cooldown_seconds : tuning::enemy_attack_period[index];
    a.attack_timer = period * (0.5F + 0.5F * timing_sample);
  }
  actors.push_back(a);
}
} // namespace human_resources::gameplay
