#pragma once
#include <array>
#include <human_resources/product/gear_presentation.h>
#include <span>
namespace human_resources::product {
struct ShipAppearance final {
  float ring_growth{};
  std::array<float, 3> ring_tint{1, 1, 1};
  float rotor_speed{1.8F};
  unsigned lamp_count{6};
  std::string_view rotor_mesh{"ship-rotor-0"};
  std::string_view canopy_mesh{"ship-dome"};
  float mast_height{1.012F};
};
// Bounded cosmetic projection only. All geometry derives from installed state.
[[nodiscard]] ShipAppearance ship_appearance(const gameplay::Snapshot&);
[[nodiscard]] float drone_orbit_radius(const gameplay::Snapshot&, ShipPose);
[[nodiscard]] std::span<const std::string_view> ship_component_ids();
} // namespace human_resources::product
