#pragma once
#include <array>
#include <human_resources/product/ship_appearance.h>
#include <span>

namespace human_resources::product::detail {
inline constexpr std::size_t ship_part_capacity = 160;
struct ShipAssembly final {
  std::array<presentation::StyleInstance, ship_part_capacity> parts;
  std::size_t count{};
  std::array<presentation::Vec3, 4> laser_muzzles{};
  std::array<presentation::Vec3, 3> drone_muzzles{}, drones{};
  std::array<presentation::Vec3, 6> intakes{}, exhausts{}, exhaust_ends{};
  unsigned laser_count{}, drone_count{}, intake_count{}, exhaust_count{};
  float footprint_radius{}, drone_radius{};
  [[nodiscard]] std::span<const presentation::StyleInstance> instances() const {
    return {parts.data(), count};
  }
};
// Complete measured physical rig; bounded scratch, no heap allocation or rules.
// The same assembly supplies visible models, laser/drone muzzle origins,
// intake sockets and a pose-aware drone exclusion envelope.
[[nodiscard]] ShipAssembly ship_assembly(const gameplay::Snapshot&, ShipPose, bool reduced_motion,
    float ring_angle, float repair_response = 0);
void assemble_ship_body(ShipAssembly&, const gameplay::Snapshot&, ShipPose, bool reduced_motion,
    float ring_angle, float repair_response);
void assemble_ship_weapons(ShipAssembly&, const gameplay::Snapshot&, ShipPose);
void assemble_ship_drones(ShipAssembly&, const gameplay::Snapshot&, ShipPose, bool reduced_motion);
// Rigid hull-relative component with a true local scale/rotation.
presentation::StyleInstance& fit_part(ShipAssembly&, std::string_view mesh, ShipPose,
    presentation::Vec3 local, presentation::Vec3 scale = {1, 1, 1}, presentation::Vec3 rotation = {});
void fit_support(ShipAssembly&, ShipPose, presentation::Vec3 from, presentation::Vec3 to, float width = .18F);
[[nodiscard]] unsigned module_rank(const gameplay::Snapshot&, gameplay::Module);
[[nodiscard]] bool evolved(const gameplay::Snapshot&, gameplay::Evolution);
} // namespace human_resources::product::detail
