#pragma once
#include <array>
#include <human_resources/gameplay/gameplay.h>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>

namespace human_resources::product {
// Presentation names and units consume the already projected choice. Never
// recompute a benefit here: gameplay owns permanent bonuses and evolutions.
struct UpgradeMetric final {
  std::string_view label, unit;
};
inline unsigned hardware_count(const gameplay::Snapshot& run) {
  unsigned count = 0;
  for (const auto rank : run.modules)
    count += rank != 0;
  return count;
}
inline std::string choice_status(const gameplay::UpgradeChoice& choice) {
  if (!choice.hardware)
    return "FLIGHT RESEARCH";
  return choice.next_rank == 1 ? "NEW HARDWARE" : "RANK UP / " + std::to_string(choice.next_rank) + " OF 3";
}
inline UpgradeMetric choice_metric(const gameplay::UpgradeChoice& choice) {
  constexpr std::array<UpgradeMetric, static_cast<unsigned>(gameplay::Module::count)> hardware{{{"LASERS", ""}, {"PIERCING TARGETS", ""}, {"ARC CHANCE", "%"}, {"DRONES", ""},
      {"PULSE DAMAGE", ""}, {"CAPTURE BEAMS", ""}, {"CAPTURE REACH", "m"}, {"JEEP CAPTURE HULL", "%"},
      {"FIRE RATE / BEAM", "%"}, {"DAMAGE / IDLE BEAM", "%"}, {"SHIELD CAPACITY", ""},
      {"HULL / 12 CAPTURES", ""}, {"REFLECT CHANCE", "%"}, {"FLIGHT SPEED", "m/s"},
      {"DASH PROTECTION", "s"}, {"BIO SCORE BONUS", "%"}}};
  constexpr std::array<UpgradeMetric, static_cast<unsigned>(gameplay::Research::count)> research{{{"LASER DAMAGE", ""}, {"MAXIMUM HULL", ""}, {"SHIELD CAPACITY", ""}, {"SCORE BONUS", "%"},
      {"CAPTURE SPEED BONUS", "%"}, {"FIRE RATE BONUS", "%"}, {"HULL RESTORED", ""}}};
  const auto index = choice.hardware ? static_cast<unsigned>(choice.module) : static_cast<unsigned>(choice.research);
  if (choice.hardware)
    return index < hardware.size() ? hardware[index] : UpgradeMetric{};
  return index < research.size() ? research[index] : UpgradeMetric{};
}
inline std::string metric_number(float value) {
  std::ostringstream stream;
  stream << std::fixed << std::setprecision(2) << value;
  auto result = stream.str();
  while (result.ends_with('0'))
    result.pop_back();
  if (result.ends_with('.'))
    result.pop_back();
  return result;
}
inline std::string choice_change(const gameplay::UpgradeChoice& choice) {
  return metric_number(choice.before) + " > " + metric_number(choice.after) + std::string(choice_metric(choice).unit);
}
} // namespace human_resources::product
