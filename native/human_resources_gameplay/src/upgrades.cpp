#include "run_state.h"
#include <iomanip>
#include <sstream>

namespace human_resources::gameplay {
namespace {
std::string number(float value) {
  std::ostringstream out;
  out << std::fixed << std::setprecision(std::fabs(value - std::round(value)) < 0.001F ? 0 : 2) << value;
  auto formatted = out.str();
  if (formatted.find('.') != std::string::npos) {
    while (formatted.back() == '0') formatted.pop_back();
    if (formatted.back() == '.') formatted.pop_back();
  }
  return formatted;
}
std::string change(std::string_view label, float before, float after, std::string_view unit) {
  return std::string(label) + ": " + number(before) + (unit.starts_with("%") ? "%" : "") + " -> " + number(after) + std::string(unit) + ".";
}
struct Candidate final {
  bool hardware{};
  std::size_t index{}, rarity{};
  float weight{};
};
} // namespace
bool Game::State::module_eligible(std::size_t index) const {
  if ((tuning::available_blueprints & (1U << index)) == 0 || view.modules[index] >= 3 || (setup.unlocked_blueprints & (1U << index)) == 0) {
    return false;
  }
  if (view.modules[index] == 0 && static_cast<unsigned>(std::count_if(view.modules.begin(), view.modules.end(), [](std::uint8_t r) { return r > 0; })) >= tuning::hardware_slots) {
    return false;
  }
  for (std::size_t i = 0; i < view.modules.size(); ++i) {
    if (view.modules[i] < tuning::prerequisites[index][i]) {
      return false;
    }
  }
  return true;
}
bool Game::State::research_eligible(std::size_t index) const {
  if (index == static_cast<std::size_t>(Research::emergency_repairs)) {
    return view.hull < view.maximum_hull;
  }
  return tuning::research_caps[index] <= 0 || view.research[index] < tuning::research_caps[index];
}
void Game::State::draw_choices() {
  std::array<bool, detail::module_count> used_modules{};
  std::array<bool, detail::research_count> used_research{};
  for (auto& choice : view.choices) {
    std::array<std::vector<Candidate>, 3> buckets;
    for (std::size_t i = 0; i < view.modules.size(); ++i) {
      if (!used_modules[i] && module_eligible(i)) {
        const std::size_t bucket = view.modules[i] > 0 ? 0 : static_cast<std::size_t>(tuning::module_rarity[i]);
        buckets[bucket].push_back({true, i, bucket, tuning::hardware_weight});
      }
    }
    for (std::size_t i = 0; i < view.research.size(); ++i) {
      if (!used_research[i] && research_eligible(i)) {
        for (std::size_t rarity = 0; rarity < 3; ++rarity) {
          buckets[rarity].push_back({false, i, rarity, tuning::research_weight});
        }
      }
    }
    float total = 0;
    for (std::size_t i = 0; i < 3; ++i) {
      if (!buckets[i].empty()) {
        total += tuning::rarity_weights[i];
      }
    }
    float roll = draft_unit() * total;
    std::size_t bucket = 0;
    for (; bucket < 2; ++bucket) {
      if (buckets[bucket].empty()) {
        continue;
      }
      if (roll < tuning::rarity_weights[bucket]) {
        break;
      }
      roll -= tuning::rarity_weights[bucket];
    }
    // Every draw has at least four uncapped research families.
    total = 0;
    for (const auto& candidate : buckets[bucket]) {
      total += candidate.weight;
    }
    roll = draft_unit() * total;
    auto selected = buckets[bucket].back();
    for (const auto& candidate : buckets[bucket]) {
      if (roll < candidate.weight) {
        selected = candidate;
        break;
      }
      roll -= candidate.weight;
    }
    choice = make_choice(selected.hardware, selected.index, selected.rarity);
    if (selected.hardware) {
      used_modules[selected.index] = true;
    } else {
      used_research[selected.index] = true;
    }
  }
}
UpgradeChoice Game::State::make_choice(bool hardware, std::size_t index, std::size_t rarity) const {
  UpgradeChoice choice;
  choice.hardware = hardware;
  choice.rarity = static_cast<Rarity>(rarity);
  if (!hardware) {
    choice.research = static_cast<Research>(index);
    choice.title = tuning::research_names[index];
    const unsigned bundle = index <= static_cast<std::size_t>(Research::specimen_value) ? 1 + picks / tuning::research_bundle_levels : 1;
    choice.gain = tuning::research_values[index][rarity] * static_cast<float>(bundle);
    if (tuning::research_caps[index] > 0) {
      choice.gain = std::min(choice.gain, tuning::research_caps[index] - view.research[index]);
    }
    choice.before = view.research[index];
    choice.after = choice.before + choice.gain;
    std::string label = choice.title, unit, qualifier;
    switch (choice.research) {
    case Research::weapon_damage:
      choice.before = view.weapon_damage;
      choice.after = choice.before + tuning::base_laser_damage * (1 + permanent(0)) * choice.gain / 100 * (evolved(Evolution::office_shredder) ? 1 + tuning::evolution_laser_bonus : 1);
      label = "Original laser damage";
      unit = " per shot";
      choice.effect_unit = " /shot";
      break;
    case Research::hull_reinforcement:
      choice.before = view.maximum_hull;
      choice.after = choice.before + choice.gain;
      label = "Maximum hull";
      qualifier = "restore " + number(choice.gain) + " hull";
      unit = "; " + qualifier;
      break;
    case Research::shield_capacity:
      choice.before = view.maximum_shield;
      choice.after = choice.before + choice.gain;
      label = "Shield capacity";
      qualifier = "fill only the new capacity";
      unit = "; " + qualifier;
      break;
    case Research::specimen_value:
      label = "Specimen score bonus";
      qualifier = "of base score; XP unchanged";
      unit = "% " + qualifier;
      choice.effect_unit = "%";
      break;
    case Research::capture_speed:
      label = "Capture speed bonus";
      qualifier = "of base rate";
      unit = "% " + qualifier;
      choice.effect_unit = "%";
      break;
    case Research::fire_rate:
      label = "Fire rate bonus";
      qualifier = "of base rate; upgrade cap " + number(tuning::firing_rate_maximum_base_multiplier * 100) + "% of base, before temporary Overdrive";
      unit = "% " + qualifier;
      choice.effect_unit = "%";
      break;
    case Research::emergency_repairs:
      choice.before = view.hull;
      choice.after = std::min(view.maximum_hull, view.hull + view.maximum_hull * choice.gain / 100);
      label = "Hull";
      qualifier = "immediate repair";
      unit = "; " + qualifier;
      break;
    case Research::count:
      break;
    }
    choice.effect_label = label;
    choice.conditions = qualifier.empty() ? "" : qualifier + ".";
    choice.description = change(label, choice.before, choice.after, unit);
    return choice;
  }
  choice.module = static_cast<Module>(index);
  choice.rarity = static_cast<Rarity>(tuning::module_rarity[index]);
  choice.next_rank = static_cast<unsigned>(view.modules[index]) + 1;
  choice.gain = 1;
  choice.title = tuning::module_names[index];
  // Project through the same evolution/snapshot rules used by execution. This
  // includes first-two recipe order and avoids per-card evolution exceptions.
  State projected = *this;
  ++projected.view.modules[index];
  projected.evolve();
  projected.sync();
  for (std::size_t i = 0; i < tuning::evolution_ingredients.size(); ++i) {
    if ((projected.view.evolution_bits & (1U << i)) != 0 && (view.evolution_bits & (1U << i)) == 0) {
      if (!choice.evolution_preview.empty()) {
        choice.evolution_preview += " + ";
      }
      choice.evolution_preview += tuning::evolution_names[i];
    }
  }
  const float before = static_cast<float>(view.modules[index]);
  const float after = before + 1;
  std::string label, unit, extra;
  switch (choice.module) {
  case Module::laser_array:
    choice.before = tuning::base_lasers + before;
    choice.after = choice.before + 1;
    label = "Independent lasers";
    unit = " total";
    extra = "New laser: " + number(projected.view.weapon_damage * tuning::added_laser_fraction) + " damage/shot (50% of original).";
    if (after == 3) {
      extra += " Repeated hits focus up to +20%.";
    }
    break;
  case Module::rail_lens:
    choice.before = before;
    choice.after = after;
    label = "Additional piercing targets";
    extra = "Each takes 40% of the initial hit; no extra chains.";
    break;
  case Module::arc_coil:
    choice.before = before == 0 ? 0 : (tuning::arc_chance_base + (before - 1) * tuning::arc_chance_step) * 100;
    choice.after = (tuning::arc_chance_base + (after - 1) * tuning::arc_chance_step) * 100;
    label = "Arc chance";
    unit = "%";
    extra = "Hits up to " + number(after) + (after == 1 ? " nearby enemy" : " different nearby enemies") + " at 30% damage each.";
    break;
  case Module::orbit_drones:
    choice.before = before;
    choice.after = after;
    label = "Orbit drones";
    extra = "Each: " + number(tuning::drone_damage * damage_multiplier()) + " damage, 2 shots/s before rate bonuses.";
    break;
  case Module::emp_ring:
    choice.before = before == 0 ? 0 : (tuning::pulse_damage_base + (before - 1) * tuning::pulse_damage_step) * damage_multiplier();
    choice.after = (tuning::pulse_damage_base + (after - 1) * tuning::pulse_damage_step) * damage_multiplier();
    label = "EMP pulse damage";
    extra = "Radius " + number(projected.evolved(Evolution::storm_drain) ? tuning::storm_radius : tuning::pulse_radius_base + (after - 1) * tuning::pulse_radius_step) + "m; cooldown " + number(tuning::pulse_cooldown_base - (after - 1)) + "s.";
    if (after == 3) {
      extra += " Soldier capture: 0.75s disable, 5s cooldown.";
    }
    break;
  case Module::beam_splitter:
    choice.before = static_cast<float>(view.beam_capacity);
    choice.after = static_cast<float>(projected.view.beam_capacity);
    label = "Simultaneous biological targets";
    if (after == 3) {
      extra = "Captures: +25% other beam speed for 2s.";
    }
    break;
  case Module::tractor_lens:
    choice.before = view.capture_radius;
    choice.after = projected.view.capture_radius;
    label = "Capture radius";
    unit = "m";
    extra = "Pickup range only; active beams stay locked until capture.";
    if (after == 3) {
      extra += " Pulls specimens from 15% beyond the ring.";
    }
    break;
  case Module::heavy_winch:
    choice.before = before == 0 ? 0 : tuning::jeep_capture_threshold[static_cast<std::size_t>(before) - 1];
    choice.after = projected.evolved(Evolution::hostile_acquisition) ? 100 : tuning::jeep_capture_threshold[static_cast<std::size_t>(after) - 1];
    label = "Jeep capture hull";
    unit = "%";
    extra = "Uses 2 beam slots; 8s vehicle cooldown.";
    if (after == 3) {
      extra += " Tanks <=" + number((projected.evolved(Evolution::hostile_acquisition) ? tuning::evolution_tank_threshold : tuning::tank_capture_threshold) * 100) + "% hull. Vehicle kills: +40% bio speed, 5s.";
    }
    break;
  case Module::shield_generator:
    choice.before = view.maximum_shield;
    choice.after = projected.view.maximum_shield;
    label = "Shield capacity";
    extra = "Fill new capacity only; recharge " + number(tuning::base_shield_regen_per_second + after * tuning::shield_recharge_step) + "/s after " + number(tuning::shield_delays[choice.next_rank - 1]) + "s safe.";
    break;
  case Module::repair_swarm:
    choice.before = before == 0 ? 0 : tuning::repair_base + before - 1;
    choice.after = tuning::repair_base + after - 1;
    label = "Hull restored every 12 captures";
    if (after >= 2) {
      extra = "Restore " + number(after - 1) + " shield per biological capture, capped at 4/s.";
    }
    break;
  case Module::vector_thrusters:
    choice.before = std::min(tuning::movement_maximum_base_multiplier, 1 + before * tuning::movement_step) * tuning::base_movement_metres_per_second;
    choice.after = std::min(tuning::movement_maximum_base_multiplier, 1 + after * tuning::movement_step) * tuning::base_movement_metres_per_second;
    label = "Free-flight speed";
    unit = "m/s";
    extra = "Capture movement penalty becomes " + number(std::max(0.0F, tuning::base_capture_movement_penalty_percent - after * tuning::capture_penalty_step * 100)) + "%.";
    if (after == 3) {
      extra += " Captures grant +12% movement for 3s.";
    }
    break;
  case Module::phase_drive:
    choice.before = before == 0 ? tuning::starter_dash_invulnerability : tuning::dash_invulnerability + (before - 1) * tuning::dash_invulnerability_step;
    choice.after = tuning::dash_invulnerability + (after - 1) * tuning::dash_invulnerability_step;
    label = "Dash invulnerability";
    unit = "s";
    extra = "Activate a 3m dash; cooldown " + number(tuning::dash_cooldown - (after - 1) * tuning::dash_cooldown_step) + "s.";
    break;
  case Module::overtime_reactor:
  case Module::reserve_battery:
  case Module::mirror_plating:
  case Module::bounty_scanner:
  case Module::count:
    break;
  }
  choice.effect_label = label;
  choice.effect_unit = unit;
  choice.conditions = extra;
  choice.description = change(label, choice.before, choice.after, unit) + (extra.empty() ? "" : " " + extra);

  return choice;
}
void Game::State::apply(const UpgradeChoice& choice) {
  if (choice.hardware) {
    ++view.modules[static_cast<std::size_t>(choice.module)];
    if (choice.module == Module::shield_generator) {
      view.shield += tuning::shield_capacity_step;
    }
    evolve();
    return;
  }
  if (choice.research == Research::emergency_repairs) {
    view.hull = std::min(view.maximum_hull, view.hull + view.maximum_hull * choice.gain / 100);
    return;
  }
  view.research[static_cast<std::size_t>(choice.research)] += choice.gain;
  if (choice.research == Research::hull_reinforcement) {
    view.hull += choice.gain;
  }
  if (choice.research == Research::shield_capacity) {
    view.shield += choice.gain;
  }
}
void Game::State::evolve() {
  for (std::size_t i = 0; i < tuning::evolution_ingredients.size() && view.stats.evolutions < tuning::maximum_evolutions; ++i) {
    if ((tuning::available_evolutions & (1U << i)) == 0) continue;
    const auto recipe = tuning::evolution_ingredients[i];
    if ((view.evolution_bits & (1U << i)) == 0 && view.modules[recipe[0]] == 3 && view.modules[recipe[1]] == 3) {
      view.evolution_bits |= 1U << i;
      ++view.stats.evolutions;
    }
  }
}
} // namespace human_resources::gameplay
