#include "ship_assembly.h"
#include <algorithm>
#include <cmath>
#include <human_resources/presentation/rotor_geometry.h>
#include <human_resources/product/visibility.h>
#include <numbers>
#include <ship_components.generated.h>
namespace human_resources::product::detail {
namespace {
using gameplay::Evolution;
using gameplay::Module;
using presentation::Vec3;
constexpr float tau = 2 * std::numbers::pi_v<float>;
constexpr std::array<std::string_view, 3> barrels{"ship-laser-barrel-1", "ship-laser-barrel-2", "ship-laser-barrel-3"};
constexpr std::array<std::string_view, 3> electrodes{"ship-electrode-1", "ship-electrode-2", "ship-electrode-3"};
Vec3 source_point(const presentation::StyleInstance& part, Vec3 p) {
  const auto m = presentation::model_matrix(part);
  return {m[0] * p.x + m[1] * p.y + m[2] * p.z + m[3], m[4] * p.x + m[5] * p.y + m[6] * p.z + m[7], m[8] * p.x + m[9] * p.y + m[10] * p.z + m[11]};
}
Vec3 shot_target(const gameplay::Snapshot& run, gameplay::ShotKind kind, unsigned index, Vec3 mount) {
  for (const auto& shot : run.shots)
    if (shot.kind == kind && shot.emitter_index == index)
      return {shot.to.x, .65F, run.runner_distance - shot.to.z + 2.3F};
  return {mount.x, mount.y, mount.z - 10};
}
float mesh_sphere(std::string_view id) {
  const auto bounds = visibility_mesh_bounds(id);
  if (!bounds)
    return 0;
  const float x = std::max(std::abs(bounds->minimum.x), std::abs(bounds->maximum.x));
  const float y = std::max(std::abs(bounds->minimum.y), std::abs(bounds->maximum.y));
  const float z = std::max(std::abs(bounds->minimum.z), std::abs(bounds->maximum.z));
  return std::hypot(std::hypot(x, z), y);
}
} // namespace
void assemble_ship_weapons(ShipAssembly& rig, const gameplay::Snapshot& run, ShipPose pose) {
  const unsigned rail = module_rank(run, Module::rail_lens), arc = module_rank(run, Module::arc_coil);
  rig.laser_count = std::clamp(run.laser_count, 1U, 4U);
  const float damage = std::max(0.F, run.research[static_cast<unsigned>(gameplay::Research::weapon_damage)]);
  const bool exposed = module_rank(run, Module::laser_array) || rail || arc || damage > 0;
  const float power = 1 + .20F * damage / (damage + 25);
  for (unsigned i = 0; i < rig.laser_count; ++i) {
    const bool right = (i & 1U) != 0;
    Vec3 local{right ? .36F : -.36F, -.16F, .29F};
    float size = rig.laser_count > 2 ? .72F : 1.F;
    if (exposed) {
      const float ring = presentation::rotor_radius(1.3F, ship_appearance(run).ring_growth);
      const unsigned side_count = right ? rig.laser_count / 2 : (rig.laser_count + 1) / 2;
      const float pair = side_count == 2 ? (i < 2 ? -.20F : .20F) : 0;
      local = {(right ? 1.F : -1.F) * (ring + .38F + pair), -.36F, 0};
      // Sponsons attach to the original fixed-hull sockets and carry the real
      // weapons outside/below the spinning annulus, where their upper surfaces
      // remain visible and down-aiming barrels have free working clearance.
      const Vec3 root{right ? .36F : -.36F, -.16F, .29F};
      fit_support(rig, pose, root, local, .18F);
    }
    const Vec3 mount = ship_socket(pose, local);
    const auto target = shot_target(run, gameplay::ShotKind::laser, i, mount);
    const float dx = target.x - mount.x, dy = target.y - mount.y, dz = target.z - mount.z;
    const Vec3 aim{-std::atan2(dy, std::hypot(dx, dz)), std::atan2(dx, dz), pose.bank};
    const auto mesh = rail ? barrels[rail - 1] : std::string_view{"ufo-laser"};
    auto& gun = fit_part(rig, mesh, ShipPose{mount, pose.scale * size, 0, 0}, {}, {power, power, 1}, aim);
    const float barrel = rail ? .95F + .16F * static_cast<float>(rail) : .65F;
    auto muzzle = source_point(gun, {0, rail ? .07F : 0, barrel});
    if (arc) {
      const auto root = source_point(gun, {0, rail ? .07F : 0, barrel - .06F});
      auto& head = fit_part(rig, electrodes[arc - 1], ShipPose{root, pose.scale * size, 0, 0}, {}, {1, 1, 1}, aim);
      muzzle = source_point(head, {0, .07F, .10F + .04F * static_cast<float>(arc) + .18F * (1 + .3F * static_cast<float>(arc))});
    }
    if (evolved(run, Evolution::office_shredder)) {
      // A physical focusing sleeve, sharing this weapon's axis and muzzle.
      const auto root = source_point(gun, {0, 0, barrel - .14F});
      auto& sleeve = fit_part(rig, "ufo-rim-lens", ShipPose{root, pose.scale * size, 0, 0}, {}, {1.3F, 1.4F, 1.3F},
          {aim.x + std::numbers::pi_v<float> / 2, aim.y, aim.z});
      sleeve.tint = {1.5F, 1, .6F, 1};
    }
    rig.laser_muzzles[i] = muzzle;
    // Reserve full turret reach, so the drone orbit cannot shrink/expand with
    // each target or let a later inward/outward barrel sweep through a drone.
    const float reach = mesh_sphere(mesh) * pose.scale * size * std::max(power, 1.F) +
                        (arc ? mesh_sphere(electrodes[arc - 1]) * pose.scale * size : 0);
    const auto offset = Vec3{mount.x - pose.position.x, mount.y - pose.position.y, mount.z - pose.position.z};
    rig.footprint_radius = std::max(rig.footprint_radius, std::hypot(std::hypot(offset.x, offset.z), offset.y) + reach);
  }
}
void assemble_ship_drones(ShipAssembly& rig, const gameplay::Snapshot& run, ShipPose pose, bool reduced) {
  rig.drone_count = module_rank(run, Module::orbit_drones);
  const bool elite = evolved(run, Evolution::employee_benefits);
  const auto mesh = elite ? std::string_view{"ship-drone-elite"} : std::string_view{"ship-drone"};
  const float extent = mesh_sphere(mesh) * ship_drone_scale;
  const float ship_gap = 2 * extent * ship_drone_ship_gap;
  const float neighbor_gap = 2 * extent * ship_drone_neighbor_gap;
  const float chord = 2 * extent + neighbor_gap;
  const float neighbor_radius = rig.drone_count > 1 ? chord / (2 * std::sin(std::numbers::pi_v<float> / static_cast<float>(rig.drone_count))) : 0;
  rig.drone_radius = std::max(rig.footprint_radius + extent + ship_gap, neighbor_radius);
  const float phase = reduced ? 0 : static_cast<float>(std::fmod(run.stats.active_seconds * .85, tau));
  for (unsigned i = 0; i < rig.drone_count; ++i) {
    const float a = phase + static_cast<float>(i) * tau / static_cast<float>(rig.drone_count);
    const Vec3 p{pose.position.x + std::cos(a) * rig.drone_radius, pose.position.y + .22F, pose.position.z + std::sin(a) * rig.drone_radius};
    const auto target = shot_target(run, gameplay::ShotKind::drone, i, p);
    const float dx = target.x - p.x, dy = target.y - p.y, dz = target.z - p.z;
    auto& drone = fit_part(rig, mesh, ShipPose{p, ship_drone_scale, 0, 0}, {}, {1, 1, 1},
        {-std::atan2(dy, std::hypot(dx, dz)), std::atan2(dx, dz), 0});
    drone.damage_reactive = false;
    rig.drones[i] = p;
    rig.drone_muzzles[i] = source_point(drone, {0, .50F, .9025F});
  }
}
} // namespace human_resources::product::detail
