#include "ship_assembly.h"
#include <human_resources/gameplay/tuning.h>
#include <algorithm>
#include <cmath>
#include <human_resources/presentation/rotor_geometry.h>
#include <human_resources/product/ship_appearance.h>
#include <human_resources/product/visibility.h>
#include <numbers>
#include <ship_components.generated.h>
#include <stdexcept>
namespace human_resources::product {
namespace detail {
unsigned module_rank(const gameplay::Snapshot& run, gameplay::Module m) {
  const auto index = static_cast<unsigned>(m);
  return index < run.modules.size() && (gameplay::tuning::available_blueprints & (1U << index)) ? std::min(3U, static_cast<unsigned>(run.modules[index])) : 0;
}
bool evolved(const gameplay::Snapshot& run, gameplay::Evolution e) {
  return (run.evolution_bits & (1U << static_cast<unsigned>(e))) != 0;
}
presentation::StyleInstance& fit_part(ShipAssembly& rig, std::string_view mesh, ShipPose pose,
    presentation::Vec3 local, presentation::Vec3 scale, presentation::Vec3 rotation) {
  if (rig.count == rig.parts.size())
    throw std::logic_error("Physical ship exceeds its bounded part inventory");
  const auto parent = presentation::model_matrix({.rotation = {pose.pitch, 0, pose.bank}});
  const auto relative = presentation::model_matrix({.rotation = rotation});
  const auto combined = presentation::multiply(parent, relative);
  const presentation::Vec3 angles{std::asin(std::clamp(-combined[6], -1.F, 1.F)),
      std::atan2(combined[2], combined[10]), std::atan2(combined[4], combined[5])};
  auto& part = rig.parts[rig.count++];
  part = {.mesh = mesh, .position = ship_socket(pose, local), .scale = {pose.scale * scale.x, pose.scale * scale.y, pose.scale * scale.z}, .rotation = angles, .polished_hull = true, .damage_reactive = true};
  return part;
}
void fit_support(ShipAssembly& rig, ShipPose pose, presentation::Vec3 a, presentation::Vec3 b, float width) {
  const float x = b.x - a.x, y = b.y - a.y, z = b.z - a.z;
  const float length = std::hypot(std::hypot(x, z), y);
  if (length < .001F)
    return;
  // The authored chamfered structural member is one metre along local Z.
  fit_part(rig, "ship-pylon", pose, {(a.x + b.x) / 2, (a.y + b.y) / 2, (a.z + b.z) / 2},
      {width / .18F, width / .16F, length}, {-std::atan2(y, std::hypot(x, z)), std::atan2(x, z), 0});
}
ShipAssembly ship_assembly(const gameplay::Snapshot& run, ShipPose pose, bool reduced, float ring_angle, float repair) {
  ShipAssembly rig;
  assemble_ship_body(rig, run, pose, reduced, ring_angle, repair);
  assemble_ship_weapons(rig, run, pose);
  // Measure the actual consumed physical parts in world XZ, including the
  // current bank/pitch. Shader-widened rotor bounds use the identical policy.
  for (const auto& part : rig.instances()) {
    if (part.effect)
      continue;
    auto bounds = visibility_mesh_bounds(part.mesh);
    if (!bounds)
      throw std::logic_error("Physical ship component has no cooked bounds");
    presentation::expand_rotor_bounds(bounds->minimum, bounds->maximum, part.rotor_style[0]);
    const auto m = presentation::model_matrix(part);
    for (float x : {bounds->minimum.x, bounds->maximum.x})
      for (float y : {bounds->minimum.y, bounds->maximum.y})
        for (float z : {bounds->minimum.z, bounds->maximum.z}) {
          const float xx = m[0] * x + m[1] * y + m[2] * z + m[3] - pose.position.x;
          const float zz = m[8] * x + m[9] * y + m[10] * z + m[11] - pose.position.z;
          rig.footprint_radius = std::max(rig.footprint_radius, std::hypot(xx, zz));
        }
  }
  assemble_ship_drones(rig, run, pose, reduced);
  return rig;
}
} // namespace detail
ShipAppearance ship_appearance(const gameplay::Snapshot& run) {
  using gameplay::Module;
  ShipAppearance look;
  look.ring_growth = detail::ship_ring_growth * static_cast<float>(detail::module_rank(run, Module::tractor_lens));
  const auto emp = detail::module_rank(run, Module::emp_ring);
  constexpr std::array<std::string_view, 4> rotors{"ship-rotor-0", "ship-rotor-1", "ship-rotor-2", "ship-rotor-3"};
  look.rotor_mesh = rotors[emp];
  const float charge = static_cast<float>(emp) / 3;
  look.ring_tint = {1 + 5 * charge, 1 - .38F * charge, 1};
  look.lamp_count = 6 + 6 * emp;
  const auto phase = detail::module_rank(run, Module::phase_drive);
  constexpr std::array<std::string_view,4> canopies{"ship-dome","ship-dome-phase-1","ship-dome-phase-2","ship-dome-phase-3"};
  look.canopy_mesh = canopies[phase];
  look.mast_height = .615F + .401F * (1 - .10F * static_cast<float>(phase)) - .004F;
  look.rotor_speed += 1.2F * std::clamp(run.research[static_cast<unsigned>(gameplay::Research::fire_rate)] / 50.F, 0.F, 1.F);
  if (run.harvest_overdrive_seconds > 0)
    look.rotor_speed += 3;
  return look;
}
std::span<const std::string_view> ship_component_ids() {
  return detail::ship_component_ids;
}
float drone_orbit_radius(const gameplay::Snapshot& run, ShipPose pose) {
  return detail::ship_assembly(run, pose, true, 0).drone_radius;
}
} // namespace human_resources::product
