#include <algorithm>
#include <cmath>
#include <human_resources/product/gear_presentation.h>
#include <human_resources/product/scene_effects.h>
#include <numbers>
namespace human_resources::product {
namespace {
using gameplay::Evolution;
using gameplay::Module;
using presentation::StyleFrame;
using presentation::StyleInstance;
using presentation::Vec3;
constexpr float tau = 2 * std::numbers::pi_v<float>;
unsigned rank(const gameplay::Snapshot& run, Module module) {
  return std::min(3U, static_cast<unsigned>(run.modules[static_cast<std::size_t>(module)]));
}
bool evolved(const gameplay::Snapshot& run, Evolution e) {
  return (run.evolution_bits & (1U << static_cast<unsigned>(e))) != 0;
}
Vec3 world(ShipPose pose, Vec3 local) {
  const float c = std::cos(pose.bank), s = std::sin(pose.bank);
  return {pose.position.x + pose.scale * (c * local.x - s * local.y), pose.position.y + pose.scale * (s * local.x + c * local.y), pose.position.z + pose.scale * local.z};
}
void add(StyleFrame& frame, std::string_view mesh, Vec3 p, Vec3 scale = {1, 1, 1}, Vec3 rotation = {}, std::array<float, 4> tint = {1, 1, 1, 1}, bool effect = false) {
  frame.instances.push_back({.mesh = mesh, .position = p, .scale = scale, .rotation = rotation, .tint = tint, .effect = effect, .casts_shadow = !effect});
}
void module(StyleFrame& frame, std::string_view mesh, ShipPose pose, Vec3 socket, float size = 1, float yaw = 0) {
  // Compose parent Rz bank with local Ry aim, then decompose the scene's RyRxRz
  // convention. Both pivot position and module orientation follow the hull.
  const float x = std::asin(std::clamp(-std::sin(pose.bank) * std::sin(yaw), -1.F, 1.F));
  const float y = std::atan2(std::cos(pose.bank) * std::sin(yaw), std::cos(yaw));
  const float z = std::atan2(std::sin(pose.bank) * std::cos(yaw), std::cos(pose.bank));
  const float s = pose.scale * size;
  add(frame, mesh, world(pose, socket), {s, s, s}, {x, y, z});
}
Vec3 laser_socket(unsigned index, unsigned count) {
  const bool right = (index & 1U) != 0;
  // Each source socket carries one module; ranks II/III fit a compact two-barrel
  // cluster around that same mount, below the rotating annulus.
  const float offset = count > 2 && (!right || count > 3) ? (index < 2 ? -.13F : .13F) : 0;
  return {(right ? .36F : -.36F) + offset, -.16F, .29F};
}
Vec3 drone_position(const gameplay::Snapshot& run, ShipPose pose, unsigned index, bool reduced) {
  const auto count = std::max(1U, rank(run, Module::orbit_drones));
  const float phase = reduced ? 0 : static_cast<float>(run.stats.active_seconds) * .85F;
  const float a = phase + static_cast<float>(index) * tau / static_cast<float>(count);
  return {pose.position.x + std::cos(a) * 2.15F, pose.position.y + .22F, pose.position.z + std::sin(a) * 2.15F};
}
void ray(StyleFrame& f, Vec3 a, Vec3 b, float thickness, std::array<float, 4> tint) {
  const float dx = b.x - a.x, dy = b.y - a.y, dz = b.z - a.z;
  const float horizontal = std::hypot(dx, dz), length = std::hypot(horizontal, dy);
  if (length < .001F)
    return;
  add(f, "cube", {(a.x + b.x) * .5F, (a.y + b.y) * .5F, (a.z + b.z) * .5F}, {thickness, thickness, length * .5F}, {-std::atan2(dy, horizontal), std::atan2(dx, dz), 0}, tint, true);
}
Vec3 point(gameplay::Vec2 p, float distance, float y) {
  return {p.x, y, distance - p.z + 2.3F};
}
void rim(StyleFrame& frame, ShipPose pose, float opacity, std::array<float, 4> color) {
  color[3] = opacity;
  add(frame, "shield-shell", {pose.position.x, pose.position.y + .22F, pose.position.z}, {1.7F * pose.scale, 1.7F * pose.scale, 1.7F * pose.scale}, {}, color, true);
  frame.instances.back().shield_rim = true;
}
} // namespace
void append_ship_gear(StyleFrame& frame, const gameplay::Snapshot& run, ShipPose pose, bool reduced_motion) {
  const bool shield = rank(run, Module::shield_generator) > 0;
  module(frame, shield ? "ufo-shield" : "ufo-antenna", pose, {0, 1.012F, 0});
  // Baseline has a real capture beam and laser even before the first choice.
  module(frame, "ufo-beam", pose, {0, .04F, 0}, 1 + .07F * static_cast<float>(rank(run, Module::beam_splitter)));
  const unsigned count = std::clamp(run.laser_count, 1U, 4U);
  for (unsigned i = 0; i < count; ++i) {
    const auto mount = world(pose, laser_socket(i, count));
    Vec3 aim{mount.x, mount.y, mount.z - 1};
    for (const auto& shot : run.shots)
      if (shot.kind == gameplay::ShotKind::laser && shot.emitter_index == i) {
        aim = {shot.to.x, .65F, run.runner_distance - shot.to.z + 2.3F};
        break;
      }
    const float dx = aim.x - mount.x, dy = aim.y - mount.y, dz = aim.z - mount.z;
    const float size = pose.scale * (count > 2 ? .67F : 1.F);
    add(frame, "ufo-laser", mount, {size, size, size}, {-std::atan2(dy, std::hypot(dx, dz)), std::atan2(dx, dz), pose.bank});
  }
  for (unsigned i = 0; i < rank(run, Module::orbit_drones); ++i) {
    const auto p = drone_position(run, pose, i, reduced_motion);
    add(frame, "ufo-modular", p, {.27F, .27F, .27F});
    const Vec3 mount{p.x, p.y - .04F, p.z};
    Vec3 aim{p.x, p.y, p.z - 1};
    for (const auto& shot : run.shots)
      if (shot.kind == gameplay::ShotKind::drone && shot.emitter_index == i) {
        aim = {shot.to.x, .65F, run.runner_distance - shot.to.z + 2.3F};
        break;
      }
    add(frame, "ufo-laser", mount, {.35F, .35F, .35F}, {-std::atan2(aim.y - mount.y, std::hypot(aim.x - mount.x, aim.z - mount.z)), std::atan2(aim.x - mount.x, aim.z - mount.z), 0});
  }
  if ((shield || rank(run, Module::mirror_plating) > 0) && run.shield > 0) {
    const float fraction = run.maximum_shield > 0 ? std::clamp(run.shield / run.maximum_shield, 0.F, 1.F) : 0;
    rim(frame, pose, .22F + .38F * fraction, rank(run, Module::mirror_plating) > 0 ? std::array<float, 4>{.75F, .82F, 1, 1} : std::array<float, 4>{.45F, .94F, 1, 1});
  }
  // Capture hardware retains the one authored intake socket. Added optics are
  // visible fixed-hull collars, never fictitious rotor-mounted beam sources.
  if (rank(run, Module::tractor_lens) > 0 || rank(run, Module::heavy_winch) > 0) {
    const float r = .53F + .045F * static_cast<float>(rank(run, Module::tractor_lens));
    add(frame, "ring", world(pose, {0, -.07F, 0}), {r, 1, r}, {0, 0, pose.bank}, {.35F, .97F, .73F, .82F}, true);
    if (rank(run, Module::heavy_winch) > 0)
      for (float side : {-1.F, 1.F})
        module(frame, "ufo-rim-lens", pose, {side * .42F, -.09F, 0}, .5F);
  }
  if (evolved(run, Evolution::great_vacuum))
    for (unsigned i = 0; i < 6; ++i) {
      const float a = static_cast<float>(i) * tau / 6;
      module(frame, "ufo-rim-lens", pose, {std::cos(a) * .56F, -.12F, std::sin(a) * .56F}, .28F);
    }
}
void append_gear_shots(StyleFrame& frame, const gameplay::Snapshot& run, ShipPose pose, float distance, bool reduced_motion) {
  for (const auto& shot : run.shots) {
    if (shot.kind == gameplay::ShotKind::emp)
      continue; // Actual area event owns the field, never radial laser spokes.
    Vec3 a = point(shot.from, distance, .65F), b = point(shot.to, distance, .65F);
    auto color = std::array<float, 4>{.55F, .93F, 1, .96F};
    float width = .024F;
    if (shot.kind == gameplay::ShotKind::laser) {
      const auto socket = laser_socket(std::min(3U, static_cast<unsigned>(shot.emitter_index)), std::clamp(run.laser_count, 1U, 4U));
      const auto mount = world(pose, socket);
      const float length = std::hypot(std::hypot(b.x - mount.x, b.z - mount.z), b.y - mount.y);
      const float barrel = (run.laser_count > 2 ? .67F : 1.F) * pose.scale * .65F;
      const float u = length > .001F ? barrel / length : 0;
      a = {mount.x + (b.x - mount.x) * u, mount.y + (b.y - mount.y) * u, mount.z + (b.z - mount.z) * u};
      if (evolved(run, Evolution::office_shredder))
        width = .045F;
    } else if (shot.kind == gameplay::ShotKind::drone) {
      a = drone_position(run, pose, std::min(2U, static_cast<unsigned>(shot.emitter_index)), reduced_motion);
      a.y -= .04F;
      const float length = std::hypot(std::hypot(b.x - a.x, b.z - a.z), b.y - a.y);
      const float u = length > .001F ? .65F * .35F / length : 0;
      a = {a.x + (b.x - a.x) * u, a.y + (b.y - a.y) * u, a.z + (b.z - a.z) * u};
      color = {.90F, 1, .45F, .96F};
      width = .018F;
    } else if (shot.kind == gameplay::ShotKind::arc) {
      const Vec3 mid{(a.x + b.x) * .5F + .19F, (a.y + b.y) * .5F + .16F, (a.z + b.z) * .5F};
      ray(frame, a, mid, .022F, {.55F, .76F, 1, .94F});
      ray(frame, mid, b, .022F, {.55F, .76F, 1, .94F});
      continue;
    } else if (shot.kind == gameplay::ShotKind::rail) {
      color = {.8F, 1, .94F, .95F};
      width = .013F;
    } else if (shot.kind == gameplay::ShotKind::reflection || shot.kind == gameplay::ShotKind::mirrorball) {
      color = {.86F, .75F, 1, .96F};
      width = .036F;
    }
    ray(frame, a, b, width * 2.5F, {color[0], color[1], color[2], .22F});
    ray(frame, a, b, width, color);
  }
}
void append_gear_effects(StyleFrame& frame, const gameplay::Snapshot& run, const SceneEffects* effects, ShipPose pose, float distance, bool reduced_motion) {
  if (run.drone_burst_seconds > 0 && evolved(run, Evolution::employee_benefits))
    for (unsigned i = 0; i < rank(run, Module::orbit_drones); ++i) {
      const auto p = drone_position(run, pose, i, reduced_motion);
      add(frame, "ring", p, {.48F, 1, .48F}, {}, {.9F, 1, .45F, .8F}, true);
    }
  const unsigned reactor = rank(run, Module::overtime_reactor), reserve = rank(run, Module::reserve_battery);
  unsigned occupied = 0;
  for (const auto& beam : run.beams)
    occupied += beam.vehicle ? 2U : 1U;
  if ((reactor && !run.beams.empty()) || (reserve && occupied < run.beam_capacity) || run.capture_burst_seconds > 0)
    add(frame, "ring", world(pose, {0, -.2F, 0}), {.4F, 1, .4F}, {0, 0, pose.bank}, {.8F, 1, .45F, .8F}, true);
  if (rank(run, Module::vector_thrusters) > 0 && std::hypot(run.velocity.x, run.velocity.z) > .5F) {
    const float length = run.movement_burst_seconds > 0 ? .85F : .45F;
    for (float side : {-1.F, 1.F}) {
      const auto a = world(pose, {side * .55F, -.16F, .45F});
      ray(frame, a, {a.x, a.y, a.z + length}, .045F, {.4F, .8F, 1, .65F});
    }
  }
  for (const auto& beam : run.beams) {
    const auto p = point(beam.target, distance, .10F);
    if (rank(run, Module::bounty_scanner) > 0 && !beam.vehicle)
      for (float side : {-1.F, 1.F}) {
        ray(frame, {p.x + side * .92F, p.y, p.z - .35F}, {p.x + side * .92F, p.y, p.z + .35F}, .025F, {1, .8F, .32F, .85F});
        ray(frame, {p.x + side * .92F, p.y, p.z - .35F}, {p.x + side * .64F, p.y, p.z - .35F}, .025F, {1, .8F, .32F, .85F});
      }
    if (beam.vehicle && evolved(run, Evolution::hostile_acquisition))
      for (float side : {-1.F, 1.F}) {
        const Vec3 a{p.x + side * 1.1F, p.y, p.z}, b{pose.position.x + side * .38F, pose.position.y - .1F, pose.position.z};
        ray(frame, a, b, .045F, {.72F, 1, .86F, .7F});
      }
  }
  if (!effects)
    return;
  for (const auto& beat : effects->beats()) {
    const float u = std::clamp(beat.age / .55F, 0.F, 1.F), fade = (1 - u) * (1 - u);
    if (beat.kind == SceneBeatKind::emp_pulse) {
      const auto p = point(beat.position, distance, .18F);
      const float radius = std::max(.1F, beat.radius) * (.12F + .88F * std::min(1.F, u * 2.4F));
      add(frame, "ring", p, {radius, 1, radius}, {}, evolved(run, Evolution::storm_drain) ? std::array<float, 4>{.55F, .75F, 1, .9F * fade} : std::array<float, 4>{.3F, .95F, .78F, .9F * fade}, true);
      if (evolved(run, Evolution::storm_drain))
        add(frame, "ring", {p.x, p.y + .12F, p.z}, {radius * .86F, 1, radius * .86F}, {}, {.8F, .9F, 1, .55F * fade}, true);
    } else if (beat.kind == SceneBeatKind::shield_hit || beat.kind == SceneBeatKind::intercept || beat.kind == SceneBeatKind::reflect || beat.kind == SceneBeatKind::mirrorball) {
      rim(frame, pose, .95F * fade, beat.kind == SceneBeatKind::reflect || beat.kind == SceneBeatKind::mirrorball ? std::array<float, 4>{.86F, .75F, 1, 1} : std::array<float, 4>{.5F, .95F, 1, 1});
    } else if (beat.kind == SceneBeatKind::phase_dash) {
      const auto p = point(beat.position, distance, pose.position.y);
      if (!reduced_motion)
        for (unsigned i = 0; i < 3; ++i) {
          const float a = static_cast<float>(i) / 3;
          add(frame, "ring", {p.x + (pose.position.x - p.x) * a, p.y, p.z + (pose.position.z - p.z) * a}, {1.3F, 1, 1.3F}, {}, {.52F, .76F, 1, fade * (1 - a) * .6F}, true);
        }
    } else if (beat.kind == SceneBeatKind::repair) {
      // Capture cue is authoritative; repair rank controls its supporting glyph.
      const auto p = world(pose, {0, .65F, 0});
      ray(frame, {p.x - .2F, p.y, p.z}, {p.x + .2F, p.y, p.z}, .045F, {.6F, 1, .72F, fade});
      ray(frame, {p.x, p.y, p.z - .2F}, {p.x, p.y, p.z + .2F}, .045F, {.6F, 1, .72F, fade});
    }
  }
}
} // namespace human_resources::product
