#include "ship_assembly.h"
#include <algorithm>
#include <cmath>
#include <human_resources/product/gear_presentation.h>
#include <human_resources/product/scene_effects.h>
#include <human_resources/product/ship_appearance.h>
#include <human_resources/presentation/rotor_geometry.h>
#include <numbers>
namespace human_resources::product {
presentation::Vec3 ship_socket(ShipPose pose, presentation::Vec3 local) {
  const float cb = std::cos(pose.bank), sb = std::sin(pose.bank);
  const float cp = std::cos(pose.pitch), sp = std::sin(pose.pitch);
  const float y = sb * local.x + cb * local.y;
  return {pose.position.x + pose.scale * (cb * local.x - sb * local.y),
      pose.position.y + pose.scale * (cp * y - sp * local.z),
      pose.position.z + pose.scale * (sp * y + cp * local.z)};
}
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
  return ship_socket(pose, local);
}
void add(StyleFrame& frame, std::string_view mesh, Vec3 p, Vec3 scale = {1, 1, 1}, Vec3 rotation = {}, std::array<float, 4> tint = {1, 1, 1, 1}, bool effect = false) {
  frame.instances.push_back({.mesh = mesh, .position = p, .scale = scale, .rotation = rotation, .tint = tint, .effect = effect, .casts_shadow = !effect});
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
void rim(StyleFrame& frame, ShipPose pose, float opacity, std::array<float,4> color) {
  color[3]=opacity;
  add(frame,"shield-shell",{pose.position.x,pose.position.y+.22F,pose.position.z},
      {1.7F*pose.scale,1.7F*pose.scale,1.7F*pose.scale},{},color,true);
  frame.instances.back().shield_rim=true;
}
void dash_wake(StyleFrame& frame, const gameplay::Snapshot& run, const SceneBeat& beat, ShipPose pose, float distance, bool reduced) {
  if (beat.age >= .22F)
    return;
  const auto start = point(beat.position, distance, pose.position.y);
  const auto end = point(beat.destination, distance, pose.position.y);
  const float length = std::hypot(end.x - start.x, end.z - start.z);
  if (length < .08F)
    return; // A blocked dash never claims displacement.
  const float remaining = 1 - beat.age / .22F;
  const float fade = remaining * remaining;
  const Vec3 side{(end.z - start.z) / length, 0, -(end.x - start.x) / length};
  for (float sign : {-1.F, 1.F}) {
    const Vec3 a{start.x + side.x * sign * .62F, start.y, start.z + side.z * sign * .62F};
    const Vec3 b{end.x + side.x * sign * .24F, end.y, end.z + side.z * sign * .24F};
    ray(frame, a, b, reduced ? .035F : .055F, {.53F, .91F, 1, .78F * fade});
  }
  if (reduced)
    return;
  for (unsigned i = 0; i < 2; ++i) {
    const float u = .18F + static_cast<float>(i) * .36F;
    const Vec3 p{start.x + (end.x - start.x) * u, start.y, start.z + (end.z - start.z) * u};
    const auto rig=detail::ship_assembly(run,ShipPose{p,pose.scale,pose.bank,pose.pitch},false,frame.ring_angle);
    for(auto part:rig.instances()) {
      if(!part.damage_reactive||part.effect) continue;
      part.tint={.58F,.87F,1,fade*(.12F+.06F*static_cast<float>(i))};
      part.damage_reactive=false;part.casts_shadow=false;
      frame.instances.push_back(part);
    }
  }
}
void overdrive_engine(StyleFrame& frame, const gameplay::Snapshot& run, ShipPose pose, bool reduced) {
  if (run.harvest_overdrive_seconds <= 0)
    return;
  const float fade = std::min(1.F, run.harvest_overdrive_seconds / .35F);
  const float phase = reduced ? 0 : static_cast<float>(run.stats.active_seconds) * 5;
  // Three moving segments frame the actual rotating annulus. They are energy
  // accents, never invented permanent hardware or a larger damage/shield area.
  for (unsigned i = 0; i < 3; ++i)
    for (unsigned j = 0; j < 2; ++j) {
      const float a = phase + static_cast<float>(i) * tau / 3 + static_cast<float>(j) * .20F;
      const auto from = world(pose, {std::cos(a) * presentation::rotor_radius(1.53F, ship_appearance(run).ring_growth), .40F, std::sin(a) * presentation::rotor_radius(1.53F, ship_appearance(run).ring_growth)});
      const auto to = world(pose, {std::cos(a + .18F) * presentation::rotor_radius(1.53F, ship_appearance(run).ring_growth), .40F, std::sin(a + .18F) * presentation::rotor_radius(1.53F, ship_appearance(run).ring_growth)});
      ray(frame, from, to, .055F, {.92F, 1, .52F, .92F * fade});
    }
  const float pulse = reduced ? 1.F : .90F + .10F * std::sin(phase * 2);
  add(frame, "ring", world(pose, {0, -.15F, 0}), {.63F, 1, .63F},
      {pose.pitch, 0, pose.bank}, {.55F, 1, .77F, .85F * pulse * fade}, true);
}
void sweep_charge(StyleFrame& frame, const gameplay::Snapshot& run, ShipPose pose, float distance, bool reduced) {
  if (run.harvest_overdrive_seconds > 0)
    return;
  const gameplay::BeamView* leading = nullptr;
  for (const auto& beam : run.beams)
    if (!beam.vehicle && beam.sweep_fraction > 0 && (!leading || beam.sweep_fraction > leading->sweep_fraction))
      leading = &beam;
  if (!leading)
    return;
  // One charge arc stays on the visible annulus, including when the specimen
  // is hidden beneath the hull. Six beams cannot become six competing gauges.
  const float fraction = std::clamp(leading->sweep_fraction, 0.F, 1.F);
  const bool qualified = fraction >= 1;
  const auto target = point(leading->target, distance, pose.position.y);
  const float facing = std::atan2(target.z - pose.position.z, target.x - pose.position.x);
  const float span = std::numbers::pi_v<float> * fraction;
  const float pulse = !reduced && leading->sweep_active && !qualified ? .025F * std::sin(static_cast<float>(run.stats.active_seconds) * 10) : 0;
  const float radius = presentation::rotor_radius(1.47F, ship_appearance(run).ring_growth) + pulse;
  const std::array<float, 4> color = qualified ? std::array<float, 4>{.95F, 1, .57F, .98F}
                                            : std::array<float, 4>{.70F, 1, .86F, .96F};
  constexpr unsigned segments = 8;
  for (unsigned i = 0; i < segments; ++i) {
    const float a = facing - span * .5F + span * static_cast<float>(i) / static_cast<float>(segments);
    const float b = facing - span * .5F + span * static_cast<float>(i + 1) / static_cast<float>(segments);
    ray(frame, world(pose, {std::cos(a) * radius, .52F, std::sin(a) * radius}),
        world(pose, {std::cos(b) * radius, .52F, std::sin(b) * radius}), .075F, color);
  }
}
void sweep_finish(StyleFrame& frame, float age, ShipPose pose, bool reduced) {
  if (age >= .34F)
    return;
  const float u = age / .34F, fade = (1 - u) * (1 - u);
  if (reduced)
    return; // The ordinary success collar retains the same outcome.
  for (unsigned i = 0; i < 3; ++i) {
    const float a = static_cast<float>(i) * tau / 3 + u * 2.2F;
    const float radius = 1.85F * (1 - u) + .24F;
    const auto from = world(pose, {std::cos(a) * radius, .16F + u * .45F, std::sin(a) * radius});
    const auto to = world(pose, {std::cos(a + .38F) * radius * .78F, .25F + u * .45F, std::sin(a + .38F) * radius * .78F});
    ray(frame, from, to, .065F, {.66F, 1, .80F, .95F * fade});
  }
}
void damage_fracture(StyleFrame& frame, const SceneBeat& beat, ShipPose pose, bool reduced) {
  const bool hull = beat.kind == SceneBeatKind::hull_hit;
  const float fade = std::max(0.F, 1 - beat.age / .55F);
  const auto color = hull ? std::array<float, 4>{1, .40F, .24F, fade} : std::array<float, 4>{.52F, .95F, 1, fade};
  if (reduced) {
    add(frame, "ring", world(pose, {0, .32F, 0}), {1.62F, 1, 1.62F}, {pose.pitch, 0, pose.bank}, color, true);
    return;
  }
  for (unsigned i = 0; i < 4; ++i) {
    const float angle = static_cast<float>(i) * tau / 4 + .25F;
    const float radius = 1.5F + beat.age * (hull ? 1.1F : .65F);
    const auto a = world(pose, {std::cos(angle) * radius, .30F, std::sin(angle) * radius});
    const auto b = world(pose, {std::cos(angle + .12F) * (radius + .17F), .38F, std::sin(angle + .12F) * (radius + .17F)});
    const auto c = world(pose, {std::cos(angle) * (radius + .30F), .28F, std::sin(angle) * (radius + .30F)});
    ray(frame, a, b, hull ? .06F : .035F, color);
    ray(frame, b, c, hull ? .06F : .035F, color);
  }
}
} // namespace
void append_ship_gear(StyleFrame& frame, const gameplay::Snapshot& run, ShipPose pose, bool reduced_motion, float repair_response) {
  const auto rig=detail::ship_assembly(run,pose,reduced_motion,frame.ring_angle,repair_response);
  frame.instances.insert(frame.instances.end(),rig.instances().begin(),rig.instances().end());
}
void append_gear_shots(StyleFrame& frame, const gameplay::Snapshot& run, ShipPose pose, float distance, bool reduced_motion) {
  const auto rig=detail::ship_assembly(run,pose,reduced_motion,frame.ring_angle);
  for (const auto& shot : run.shots) {
    if (shot.kind == gameplay::ShotKind::emp)
      continue; // Actual area event owns the field, never radial laser spokes.
    Vec3 a = point(shot.from, distance, .65F), b = point(shot.to, distance, .65F);
    auto color = std::array<float, 4>{.55F, .93F, 1, .96F};
    float width = .024F;
    if (shot.kind == gameplay::ShotKind::laser) {
      a=rig.laser_muzzles[std::min(rig.laser_count-1,static_cast<unsigned>(shot.emitter_index))];
      if (evolved(run, Evolution::office_shredder))
        width = .045F;
    } else if (shot.kind == gameplay::ShotKind::drone) {
      if(!rig.drone_count) continue;
      a=rig.drone_muzzles[std::min(rig.drone_count-1,static_cast<unsigned>(shot.emitter_index))];
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
  overdrive_engine(frame, run, pose, reduced_motion);
  sweep_charge(frame, run, pose, distance, reduced_motion);
  if (run.drone_burst_seconds > 0 && evolved(run, Evolution::employee_benefits)) {
    const auto rig=detail::ship_assembly(run,pose,reduced_motion,frame.ring_angle);
    for(unsigned i=0;i<rig.drone_count;++i)
      add(frame,"ring",rig.drones[i],{.48F,1,.48F},{},{.9F,1,.45F,.8F},true);
  }
  if (run.capture_burst_seconds > 0)
    add(frame, "ring", world(pose, {0, -.2F, 0}), {.4F, 1, .4F}, {pose.pitch, 0, pose.bank}, {.8F, 1, .45F, .8F}, true);
  if (rank(run, Module::vector_thrusters) > 0 && std::hypot(run.velocity.x,run.velocity.z)>.5F) {
    const auto rig=detail::ship_assembly(run,pose,reduced_motion,frame.ring_angle);
    for(unsigned i=0;i<rig.exhaust_count;++i)
      ray(frame,rig.exhausts[i],rig.exhaust_ends[i],.045F,{.4F,.8F,1,.65F});
  }
  for (const auto& beam : run.beams) {
    const auto p = point(beam.target, distance, .10F);
    if (beam.vehicle && evolved(run, Evolution::hostile_acquisition))
      for (float side : {-1.F, 1.F}) {
        const Vec3 a{p.x + side * 1.1F, p.y, p.z}, b{pose.position.x + side * .38F, pose.position.y - .1F, pose.position.z};
        ray(frame, a, b, .045F, {.72F, 1, .86F, .7F});
      }
  }
  if (!effects)
    return;
  // Successful intake is the game's signature payoff. Keep its brief collar
  // outside the hull silhouette, rather than hiding a small ring inside it.
  // Coalesce concurrent captures; six beams never become six opaque flashes.
  float capture_age = 1, sweep_age = 1;
  for (const auto& beat : effects->beats())
    if (beat.kind == SceneBeatKind::capture)
      capture_age = std::min(capture_age, beat.age);
    else if (beat.kind == SceneBeatKind::sweep_capture)
      sweep_age = std::min(sweep_age, beat.age);
  sweep_finish(frame, sweep_age, pose, reduced_motion);
  if (capture_age <= .4F) {
    const float u = std::clamp(capture_age / .4F, 0.F, 1.F);
    const float radius = reduced_motion ? 1.64F : 1.58F + .54F * (1 - (1 - u) * (1 - u));
    const float fade = (1 - u) * (1 - u);
    add(frame, "ring", world(pose, {0, .36F, 0}), {radius, 1, radius},
        {pose.pitch, 0, pose.bank}, {.48F, 1, .79F, .84F * fade}, true);
  }
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
      if (beat.kind == SceneBeatKind::shield_hit)
        damage_fracture(frame, beat, pose, reduced_motion);
    } else if (beat.kind == SceneBeatKind::hull_hit) {
      damage_fracture(frame, beat, pose, reduced_motion);
    } else if (beat.kind == SceneBeatKind::phase_dash) {
      dash_wake(frame, run, beat, pose, distance, reduced_motion);
    } else if (beat.kind == SceneBeatKind::harvest_overdrive && beat.age < .4F) {
      for (float side : {-1.F, 1.F}) {
        const auto a = world(pose, {side * .64F, .10F, .50F});
        const auto b = world(pose, {side * .90F, .10F, reduced_motion ? .95F : 1.65F - beat.age * 2});
        ray(frame, a, b, reduced_motion ? .04F : .08F, {.86F, 1, .61F, fade * .9F});
      }
    }
  }
}
} // namespace human_resources::product
