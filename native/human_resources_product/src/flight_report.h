#pragma once
#include <human_resources/product/presentation.h>
#include <string>

namespace human_resources::product {
// Read-only report projection. Recorded rewards stay authoritative on old,
// recovered and bonus-bearing results; no account mutations occur here.
struct FlightReport final {
  std::uint64_t score{}, captures{}, credits{}, bank{}, best_score{};
  double seconds{};
  bool preview{}, pending{}, guest{}, recovered{};
  std::string heading, status, reward_label;
  std::string blueprint_heading, blueprint_name, blueprint_detail;
  float blueprint_fraction{};
  std::string hangar_name, hangar_detail, hangar_action{"HANGAR"};
  float hangar_fraction{};
  std::string summary;
};
[[nodiscard]] FlightReport flight_report(const View& view);
} // namespace human_resources::product
