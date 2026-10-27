#include "ship_assembly.h"
#include <algorithm>
#include <cmath>
#include <human_resources/presentation/rotor_geometry.h>
#include <numbers>
#include <ship_components.generated.h>
namespace human_resources::product::detail {
namespace {
using gameplay::Evolution;
using gameplay::Module;
using presentation::Vec3;
constexpr float tau = 2 * std::numbers::pi_v<float>;
float rank_value(const gameplay::Snapshot& run, Module m) {
  return static_cast<float>(module_rank(run, m));
}
float research_fraction(const gameplay::Snapshot& run, gameplay::Research r) {
  const float value = std::max(0.F, run.research[static_cast<unsigned>(r)]);
  return value / (value + 25.F);
}
void hull(ShipAssembly& rig, const gameplay::Snapshot& run, ShipPose pose, float ring_angle) {
  const auto look = ship_appearance(run);
  fit_part(rig, "ufo-modular", pose, {});
  auto& canopy = fit_part(rig, look.canopy_mesh, pose, {0, .615F, 0});
  const float phase = rank_value(run, Module::phase_drive) / 3;
  canopy.tint = {1 - .22F * phase, 1 - .45F * phase, 1 + .65F * phase, 1};
  const float reinforcement = 1 + .12F * research_fraction(run, gameplay::Research::hull_reinforcement);
  fit_part(rig, "ship-collar", pose, {0, .58F + .4F * (reinforcement - 1), 0},
      {reinforcement, 1 + rank_value(run, Module::phase_drive) * .04F, reinforcement});
  auto& rotor = fit_part(rig, look.rotor_mesh, pose, {});
  rotor.rotor_style = {look.ring_growth, look.ring_tint[0], look.ring_tint[1], look.ring_tint[2]};
  const float cell_size = look.lamp_count == 6 ? 1.F : look.lamp_count == 12 ? .76F
                                                   : look.lamp_count == 18   ? .64F
                                                                             : .55F;
  const auto emp = module_rank(run, Module::emp_ring);
  const float r = presentation::rotor_radius(ship_lamp_radius, look.ring_growth);
  const float tilt = std::atan(ship_lamp_slope[emp] / (1 + look.ring_growth));
  const float capture = 1 + .12F * research_fraction(run, gameplay::Research::capture_speed);
  for (unsigned i = 0; i < look.lamp_count; ++i) {
    const float a = static_cast<float>(i) * tau / static_cast<float>(look.lamp_count) - ring_angle;
    fit_part(rig, "ufo-rim-lens", pose, {std::cos(a) * r, ship_lamp_height[emp], std::sin(a) * r},
        {cell_size * capture, 1, cell_size * capture}, {0, -a, -tilt});
  }
}
void mast(ShipAssembly& rig, const gameplay::Snapshot& run, ShipPose pose) {
  const auto look = ship_appearance(run);
  const auto shield = module_rank(run, Module::shield_generator);
  if (!shield) {
    const float value = research_fraction(run, gameplay::Research::specimen_value);
    const float capacity = research_fraction(run, gameplay::Research::shield_capacity);
    auto& antenna = fit_part(rig, "ufo-antenna", pose, {0, look.mast_height, 0}, {1 + .3F * value + .20F * capacity, 1 + .25F * capacity, 1 + .3F * value + .20F * capacity});
    antenna.tint = {1 + value, 1, 1 - .2F * value, 1};
    return;
  }
  if (shield) {
    fit_part(rig, "ship-shield-base", pose, {0, look.mast_height, 0}, {1 + .14F * static_cast<float>(shield), .75F, 1 + .14F * static_cast<float>(shield)});
    const float radius = .16F + .08F * static_cast<float>(shield);
    const float capacity = research_fraction(run, gameplay::Research::shield_capacity);
    for (unsigned i = 0; i < shield; ++i) {
      const float a = static_cast<float>(i) * tau / static_cast<float>(shield) - tau / 4;
      const Vec3 p{std::cos(a) * radius, look.mast_height + .36F, std::sin(a) * radius};
      fit_support(rig, pose, {0, look.mast_height + .10F, 0}, p, .10F);
      const float size = 1 + .12F * static_cast<float>(shield) + .20F * capacity;
      fit_part(rig, "ship-shield-crystal", pose, p, {size, size, size});
    }
  }

}
void capture_hardware(ShipAssembly& rig, const gameplay::Snapshot& run, ShipPose pose) {
  const auto look = ship_appearance(run);
  const auto split = module_rank(run, Module::beam_splitter), lens = module_rank(run, Module::tractor_lens), winch = module_rank(run, Module::heavy_winch);
  const bool vacuum = evolved(run, Evolution::great_vacuum);
  const float optical = 1 + .12F * static_cast<float>(lens) + .12F * research_fraction(run, gameplay::Research::capture_speed);
  if (!split && !vacuum) {
    fit_part(rig, "ufo-beam", pose, {0, .04F, 0}, {optical, 1, optical});
    rig.intake_count = 1;
    rig.intakes[0] = ship_socket(pose, {0, .04F, 0});
  } else {
    if (!winch) {
      const float depth = 1 + .15F * static_cast<float>(split);
      // The complete intake casing sits below the fixed hull and rotor. A
      // fitted Winch supplies this casing itself rather than overlapping it.
      fit_part(rig, "ship-capture-manifold", pose, {0, -.20F * depth - .04F, 0}, {optical, depth, optical});
    }
    rig.intake_count = std::clamp(run.beam_capacity, 1U, 6U);
    const float radius = presentation::rotor_radius(1.3F, look.ring_growth) + .18F;
    for (unsigned i = 0; i < rig.intake_count; ++i) {
      constexpr std::array<float, 6> six{-tau / 6, -tau / 4, -tau / 3, tau / 6, tau / 4, tau / 3};
      constexpr std::array<float, 4> four{-tau / 8, -3 * tau / 8, tau / 8, 3 * tau / 8};
      constexpr std::array<float, 3> three{-tau / 6, -tau / 3, tau / 4};
      const float a = rig.intake_count >= 5 ? six[i] : rig.intake_count == 4 ? four[i]
                                                   : rig.intake_count == 3   ? three[i]
                                                                             : -tau / 4 + static_cast<float>(i) * tau / static_cast<float>(rig.intake_count);
      const Vec3 root{std::cos(a) * .50F, .60F, std::sin(a) * .50F};
      const Vec3 knuckle{std::cos(a) * .56F, .80F, std::sin(a) * .56F};
      const Vec3 p{std::cos(a) * radius, .87F, std::sin(a) * radius};
      fit_support(rig, pose, root, knuckle, .12F);
      fit_support(rig, pose, knuckle, p, .12F);
      const float size = vacuum ? 1.22F : 1 + .06F * static_cast<float>(split);
      fit_part(rig, "ship-capture-port", pose, p, {size, size, size});
      rig.intakes[i] = ship_socket(pose, {p.x, p.y - .09F, p.z});
    }
  }
  if (winch) {
    const float size = 1 + .15F * static_cast<float>(winch);
    fit_part(rig, "ship-winch-housing", pose, {0, -.34F * (1 + .2F * static_cast<float>(winch)) - .04F, 0}, {size, 1 + .2F * static_cast<float>(winch), size});
    const float reach = presentation::rotor_radius(1.3F, look.ring_growth) + .24F;
    const float heavy = evolved(run, Evolution::hostile_acquisition) ? 1.3F : 1;
    for (float side : {-1.F, 1.F}) {
      const Vec3 root{side * .36F, -.16F, -.20F}, joint{side * .48F, -.36F, -.30F}, jaw{side * (.48F + .08F * static_cast<float>(winch)), -.36F, -reach};
      fit_support(rig, pose, root, joint, .18F);
      fit_support(rig, pose, joint, jaw, .16F + .025F * static_cast<float>(winch));
      fit_part(rig, "ship-winch-pivot", pose, jaw, {size, 1, size});
      fit_part(rig, side < 0 ? "ship-winch-jaw" : "ship-winch-jaw-right", pose, {jaw.x, jaw.y - .06F, jaw.z - .22F}, {size * heavy, size, 1 + .18F * static_cast<float>(winch)});
    }
  }
}
void engines(ShipAssembly& rig, const gameplay::Snapshot& run, ShipPose pose) {
  const unsigned rank = module_rank(run, Module::vector_thrusters);
  if (!rank)
    return;
  const float radius = presentation::rotor_radius(1.3F, ship_appearance(run).ring_growth);
  const float wide = .80F + .50F * static_cast<float>(rank), length = 1 + .1F * static_cast<float>(rank);
  for (float side : {-1.F, 1.F}) {
    const Vec3 pod{side * (radius + .60F), .90F, radius * .55F + .35F};
    fit_support(rig, pose, {side * .54F, .60F, .10F}, {side * .55F, .90F, .40F}, .18F);
    fit_support(rig, pose, {side * .55F, .90F, .40F}, {pod.x, pod.y, pod.z - .25F}, .18F);
    fit_part(rig, "ship-engine-pod", pose, pod, {wide, 1, length});
    for (unsigned i = 0; i < rank; ++i) {
      const float x = pod.x + (static_cast<float>(i) - static_cast<float>(rank - 1) / 2) * .28F;
      const float z = pod.z + .475F * length;
      fit_part(rig, "ship-engine-nozzle", pose, {x, pod.y, z}, {.65F, .65F, .85F});
      rig.exhausts[rig.exhaust_count] = ship_socket(pose, {x, pod.y, z + .2295F});
      rig.exhaust_ends[rig.exhaust_count++] = ship_socket(pose, {x, pod.y, z + .85F});
    }
  }
}
void maintenance(ShipAssembly& rig, const gameplay::Snapshot& run, ShipPose pose, bool reduced, float repair) {
  const auto rank = module_rank(run, Module::repair_swarm);
  for (unsigned i = 0; i < rank; ++i) {
    const float z = (static_cast<float>(i) - static_cast<float>(rank - 1) / 2) * .52F;
    // Maintenance has its own upper fixed shelf. Capture branches and engine
    // supports cross the lower .8-.9m plane; sharing that plane clips the docks
    // when Splitter III and the rear pods are installed together.
    const Vec3 bay{.84F, 1.25F, z};
    fit_support(rig, pose, {.45F, .60F, std::clamp(z, -.2F, .2F)}, bay, .16F);
    fit_part(rig, "ship-maintenance-bay", pose, bay);
    const float u = reduced ? 0 : std::sin(std::clamp(repair, 0.F, 1.F) * std::numbers::pi_v<float>);
    fit_part(rig, "ship-repair-unit", pose, {bay.x - .12F * u, bay.y + .07F + .16F * u, bay.z});
  }
}
void shield_field(ShipAssembly& rig, const gameplay::Snapshot& run, ShipPose pose) {
  if (!module_rank(run, Module::shield_generator) || run.shield <= 0)
    return;
  const float fraction = run.maximum_shield > 0 ? std::clamp(run.shield / run.maximum_shield, 0.F, 1.F) : 0;
  const float radius = std::max(1.7F, presentation::rotor_radius(1.3F, ship_appearance(run).ring_growth) + .1F);
  auto& shell = fit_part(rig, "shield-shell", pose, {0, .15F, 0}, {radius, radius, radius});
  shell.tint = std::array<float, 4>{.45F, .94F, 1, .22F + .38F * fraction};
  shell.effect = true;
  shell.shield_rim = true;
  shell.damage_reactive = false;
  shell.casts_shadow = false;
}
} // namespace
void assemble_ship_body(ShipAssembly& rig, const gameplay::Snapshot& run, ShipPose pose, bool reduced, float angle, float repair) {
  hull(rig, run, pose, angle);
  mast(rig, run, pose);
  capture_hardware(rig, run, pose);
  engines(rig, run, pose);
  maintenance(rig, run, pose, reduced, repair);
  shield_field(rig, run, pose);
}
} // namespace human_resources::product::detail
