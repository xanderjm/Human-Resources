#include "test_support.h"
#include <human_resources/gameplay/tuning.h>
#include <algorithm>
#include <bit>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <human_resources/assets/preview_mesh.h>
#include <human_resources/presentation/rotor_geometry.h>
#include <human_resources/product/presentation.h>
#include <human_resources/product/scene_effects.h>
#include <human_resources/product/ship_appearance.h>
#include <map>
#include <numbers>
#include <set>
#include <sstream>
namespace {
namespace hr = human_resources;
using hr::gameplay::Module;
using hr::presentation::StyleFrame;
using hr::presentation::StyleInstance;
using hr::presentation::Vec3;
using test_support::require;
using test_support::require_near;
constexpr hr::product::ShipPose pose{{0, 2.5F, 2.3F}, 1.12F / .75F, 0, 0};
std::filesystem::path content;
std::map<std::string, hr::assets::PreviewMesh, std::less<>> meshes;
const hr::assets::PreviewMesh& mesh(std::string_view id) {
  const std::string name(id);
  if (meshes.contains(name))
    return meshes.at(name);
  std::ifstream in(content / (name + ".hrm"), std::ios::binary | std::ios::ate);
  require(in.good(), "consumed physical component opens");
  const auto length = in.tellg();
  require(length > 0 && length < 16 * 1024 * 1024, "bounded physical component");
  std::vector<std::byte> bytes(static_cast<std::size_t>(length));
  in.seekg(0);
  in.read(reinterpret_cast<char*>(bytes.data()), length);
  auto& result = meshes[name];
  require(hr::assets::decode_preview_mesh(bytes, result), "physical mesh decodes using production contract");
  return result;
}
Vec3 transformed(const StyleInstance& instance, Vec3 p) {
  const auto m = hr::presentation::model_matrix(instance);
  return {m[0] * p.x + m[1] * p.y + m[2] * p.z + m[3], m[4] * p.x + m[5] * p.y + m[6] * p.z + m[7], m[8] * p.x + m[9] * p.y + m[10] * p.z + m[11]};
}
std::vector<Vec3> vertices(const StyleInstance& instance) {
  std::vector<Vec3> out;
  for (const auto& v : mesh(instance.mesh).vertices) {
    Vec3 p{v.position[0], v.position[1], v.position[2]};
    if (v.rotor > .5F) {
      const float radius = std::hypot(p.x, p.z);
      if (radius > .00001F) {
        const float scale = hr::presentation::rotor_radius(radius, instance.rotor_style[0]) / radius;
        p.x *= scale;
        p.z *= scale;
      }
    }
    out.push_back(transformed(instance, p));
  }
  return out;
}
hr::gameplay::Snapshot starter() {
  hr::gameplay::Snapshot r;
  r.laser_count = r.beam_capacity = 1;
  r.hull = r.maximum_hull = 100;
  r.shield = r.maximum_shield = 40;
  return r;
}
void equip(hr::gameplay::Snapshot& r, Module type, unsigned rank) {
  r.modules[static_cast<unsigned>(type)] = static_cast<std::uint8_t>(rank);
  if (type == Module::laser_array)
    r.laser_count = rank + 1;
  if (type == Module::beam_splitter)
    r.beam_capacity = rank + 1;
}
StyleFrame present(const hr::gameplay::Snapshot& run, hr::product::ShipPose p = pose, bool reduced = true) {
  StyleFrame frame;
  hr::product::append_ship_gear(frame, run, p, reduced);
  return frame;
}
unsigned count(const StyleFrame& f, std::string_view name) {
  return static_cast<unsigned>(std::count_if(f.instances.begin(), f.instances.end(), [&](const auto& p) { return p.mesh == name; }));
}
const StyleInstance& instance(const StyleFrame& f, std::string_view name) {
  const auto found = std::find_if(f.instances.begin(), f.instances.end(), [&](const auto& p) { return p.mesh == name; });
  require(found != f.instances.end(), "required working ship component exists");
  return *found;
}
std::string geometry(const StyleFrame& f) {
  std::ostringstream result;
  for (const auto& p : f.instances) {
    if (p.effect)
      continue;
    for (const auto v : vertices(p))
      result << std::lround(v.x * 10000) << ',' << std::lround(v.y * 10000) << ',' << std::lround(v.z * 10000) << ';';
  }
  return result.str();
}
void all_modules_change_physical_geometry_in_each_rank() {
  const auto base = geometry(present(starter()));
  std::set<std::string> shapes;
  for (unsigned i = 0; i < static_cast<unsigned>(Module::count); ++i)
    if (hr::gameplay::tuning::available_blueprints & (1U << i))
    for (unsigned rank = 1; rank <= 3; ++rank) {
      auto r = starter();
      equip(r, static_cast<Module>(i), rank);
      const auto frame = present(r);
      const auto shape = geometry(frame);
      require(shape != base && shapes.insert(shape).second, "every module/rank changes actual model vertices, without depending on colour or glyph identity");
      require(count(frame, "cube") == 0, "ship hardware contains no line/cube glyph decorations");
      for (const auto& p : frame.instances) {
        require(p.mesh.starts_with("ship-") || p.mesh.starts_with("ufo-") || p.mesh == "shield-shell", "only actual ship component meshes form installed hardware");
        if (!p.effect && !p.mesh.starts_with("ship-drone"))
          require(p.damage_reactive && p.casts_shadow, "physical assemblies share local hit/depth/shadow ownership");
      }
    }
}
void ring_geometry_and_cells_follow_their_actual_components() {
  auto run = starter();
  float previous = 0;
  for (unsigned rank = 0; rank <= 3; ++rank) {
    equip(run, Module::tractor_lens, rank);
    equip(run, Module::emp_ring, rank);
    const auto frame = present(run);
    const auto look = hr::product::ship_appearance(run);
    require(count(frame, "ufo-rim-lens") == 6 + 6 * rank, "EMP rebuilds the ring with twelve/eighteen/twenty-four real capacitor cells");
    float outer = 0;
    for (const auto p : vertices(instance(frame, look.rotor_mesh)))
      outer = std::max(outer, std::hypot(p.x - pose.position.x, p.z - pose.position.z));
    require(outer > previous, "Tractor Lens increases the physical rotor diameter at each rank");
    previous = outer;
    require_near(hr::presentation::rotor_radius(.604F, look.ring_growth), .604F, .000001F, "fixed bearing mating surface is preserved");
    require_near(instance(frame, "ship-dome").scale.x, pose.scale, .000001F, "ring growth never inflates the canopy");
    for (const auto& a : frame.instances)
      if (a.mesh == "ufo-rim-lens") {
        const float radius = std::hypot(a.position.x - pose.position.x, a.position.z - pose.position.z);
        for (const auto& b : frame.instances)
          if (b.mesh == a.mesh && &a != &b)
            require(std::hypot(a.position.x - b.position.x, a.position.z - b.position.z) > radius * .13F, "rim cells retain separate housings at maximum count");
      }
  }
}
void component_combinations_keep_their_responsibilities() {
  auto run = starter();
  equip(run, Module::laser_array, 3);
  equip(run, Module::rail_lens, 3);
  equip(run, Module::arc_coil, 3);
  equip(run, Module::phase_drive, 3);
  auto frame = present(run);
  require(count(frame, "ship-laser-barrel-3") == 4 && count(frame, "ship-electrode-3") == 4, "count,barrel and muzzle upgrades compose on four real weapons");
  const auto& phased=mesh("ship-dome-phase-3");
  const auto& original=mesh("ship-dome");
  float original_z=0, phase_z=0, original_y=0, phase_y=0;
  for(const auto& v:original.vertices) {original_z=std::max(original_z,std::abs(v.position[2]));original_y=std::max(original_y,v.position[1]);}
  for(const auto& v:phased.vertices) {
    phase_z=std::max(phase_z,std::abs(v.position[2]));phase_y=std::max(phase_y,v.position[1]);
    if(std::abs(v.position[1])<.00001F) require(std::hypot(v.position[0],v.position[2])<.506F,"Phase Drive retains the fitted circular mounting flange");
  }
  require(count(frame,"ship-dome-phase-3")==1 && phase_z>original_z && phase_y<original_y,"Phase Drive reshapes actual canopy vertices above its unchanged mounting flange");
  require(count(frame, "ship-armour-3") == 0, "retired armour is absent from the active ship assembly");
  require(count(frame, "ship-scanner-dish") == 0, "retired Scanner does not replace the ordinary antenna");
  run = starter();
  equip(run, Module::shield_generator, 3);
  frame = present(run);
  require(count(frame, "ufo-antenna") == 0 && count(frame, "ship-shield-crystal") == 3 && count(frame, "ship-scanner-dish") == 0, "the surviving shield keeps its mast without the retired radar");
  run.shield = 0;
  frame = present(run);
  require(count(frame, "shield-shell") == 0 && count(frame, "ship-shield-crystal") == 3, "depleted shield loses its field but keeps physical equipment");
}
void muzzles_and_intakes_belong_to_the_remodelled_parts() {
  auto run = starter();
  equip(run, Module::laser_array, 3);
  equip(run, Module::rail_lens, 3);
  equip(run, Module::arc_coil, 3);
  for (unsigned i = 0; i < 4; ++i)
    run.shots.push_back({{}, {2 + static_cast<float>(i), 8}, .1F, true, hr::gameplay::ShotKind::laser, static_cast<std::uint8_t>(i)});
  auto frame = present(run);
  StyleFrame rays;
  hr::product::append_gear_shots(rays, run, pose, 0, true);
  unsigned mount = 0;
  for (const auto& part : frame.instances)
    if (part.mesh == "ship-electrode-3") {
      const auto expected = transformed(part, {0, .07F, .22F + .18F * 1.9F});
      const auto actual = transformed(rays.instances[mount * 2], {0, 0, -1});
      require_near(actual.x, expected.x, .00001F, "laser X leaves actual remodeled focusing head");
      require_near(actual.y, expected.y, .00001F, "laser height belongs to actual focusing head");
      require_near(actual.z, expected.z, .00001F, "laser Z leaves actual focusing head");
      ++mount;
    }
  run = starter();
  equip(run, Module::beam_splitter, 3);
  run.beams = {{.target_id = 1, .target = {2, 8}, .progress = .2F}, {.target_id = 2, .target = {-2, 9}, .progress = .3F}};
  const auto rig = present(run);
  const auto scene = hr::product::scene(run, 390.F / 844.F, true);
  std::vector<Vec3> mouths;
  for (const auto& part : rig.instances)
    if (part.mesh == "ship-capture-port")
      mouths.push_back(transformed(part, {0, -.09F / (1 + .06F * 3), 0}));
  unsigned beam = 0;
  for (const auto& p : scene.instances)
    if (p.mesh == "beam") {
      const auto top = transformed(p, {0, 1, 0});
      require_near(top.x, mouths[beam].x, .0001F, "capture beam X terminates at its actual manifold aperture");
      require_near(top.z, mouths[beam].z, .0001F, "capture beam Z terminates at its actual manifold aperture");
      ++beam;
    }
  require(beam == 2 && mouths.size() == 4, "only actual occupied beams appear under the four installed apertures");
}
void drone_clearance_is_measured_from_the_full_physical_ship() {
  for (unsigned count_drones = 1; count_drones <= 3; ++count_drones)
    for (unsigned tick = 0; tick < 48; ++tick) {
      auto r = starter();
      equip(r, Module::orbit_drones, count_drones);
      equip(r, Module::tractor_lens, 3);
      equip(r, Module::laser_array, 3);
      equip(r, Module::rail_lens, 3);
      equip(r, Module::vector_thrusters, 3);
      equip(r, Module::beam_splitter, 3);
      r.stats.active_seconds = static_cast<double>(tick) / 8;
      const hr::product::ShipPose tilted{pose.position, pose.scale, tick % 2 ? .38F : -.38F, tick % 3 ? .26F : -.26F};
      const auto frame = present(r, tilted, false);
      float ship_radius = 0;
      std::vector<const StyleInstance*> drones;
      for (const auto& p : frame.instances) {
        if (p.mesh == "ship-drone") {
          drones.push_back(&p);
          continue;
        }
        if (p.effect)
          continue;
        for (auto v : vertices(p))
          ship_radius = std::max(ship_radius, std::hypot(v.x - pose.position.x, v.z - pose.position.z));
      }
      require(drones.size() == count_drones, "formation has actual installed count");
      float drone_extent = 0;
      for (const auto* p : drones)
        for (auto v : vertices(*p))
          drone_extent = std::max(drone_extent, std::hypot(v.x - p->position.x, v.z - p->position.z));
      for (unsigned i = 0; i < drones.size(); ++i) {
        const float distance = std::hypot(drones[i]->position.x - pose.position.x, drones[i]->position.z - pose.position.z);
        require(distance - drone_extent - ship_radius >= drone_extent - .0001F, "half a drone-width of air clears the actual banked ring,weapons,pods and capture manifold");
        for (unsigned j = i + 1; j < drones.size(); ++j)
          require(std::hypot(drones[i]->position.x - drones[j]->position.x, drones[i]->position.z - drones[j]->position.z) >= 4 * drone_extent - .0001F, "one drone-width of air remains between neighboring silhouettes");
      }
    }
}
void maintenance_docks_clear_capture_and_engine_machinery() {
  for(unsigned rank=1;rank<=3;++rank) {
    auto run=starter();
    for(auto m:{Module::beam_splitter,Module::repair_swarm,Module::vector_thrusters,Module::shield_generator,Module::tractor_lens,Module::phase_drive})
      equip(run,m,rank);
    const auto frame=present(run);
    float machinery_top=0,dock_floor=1000;
    for(const auto& part:frame.instances) {
      if(part.mesh=="ship-capture-port"||part.mesh=="ship-engine-pod")
        for(const auto v:vertices(part)) machinery_top=std::max(machinery_top,v.y);
      if(part.mesh=="ship-maintenance-bay")
        for(const auto v:vertices(part)) dock_floor=std::min(dock_floor,v.y);
    }
    require(dock_floor>machinery_top+.04F,"real maintenance bay floors clear the crossing capture/engine machinery at every rank");
  }
}
void underbody_capture_casings_clear_the_rotating_ring() {
  for(unsigned rank=1;rank<=3;++rank) for(bool winch:{false,true}) {
    auto run=starter();
    equip(run,Module::beam_splitter,rank);equip(run,Module::tractor_lens,rank);equip(run,Module::emp_ring,rank);
    if(winch) equip(run,Module::heavy_winch,rank);
    const auto frame=present(run);
    require(count(frame,"ship-capture-manifold")==static_cast<unsigned>(!winch),"Winch replaces the central intake casing rather than overlapping it");
    const auto& casing=instance(frame,winch?"ship-winch-housing":"ship-capture-manifold");
    float top=-1000;
    for(const auto v:vertices(casing)) top=std::max(top,v.y);
    require(top<pose.position.y-.03F*pose.scale,"expanded intake casing stays below the spinning annulus and fixed hull with real air clearance");
  }
}
void intake_bindings_survive_beam_compaction_and_finishing() {
  auto run = starter();
  equip(run, Module::beam_splitter, 3);
  run.beams = {{.target_id = 21}, {.target_id = 22}, {.target_id = 23}};
  hr::product::SceneEffects effects;
  effects.begin_capture_frame(run);
  require(effects.capture_port(21) == 0 && effects.capture_port(22) == 1 && effects.capture_port(23) == 2,
      "new beams receive different physical apertures");
  run.stats.active_seconds = .1;
  run.beams.erase(run.beams.begin());
  run.beams.push_back({.target_id = 24});
  effects.begin_capture_frame(run);
  require(effects.capture_port(22) == 1 && effects.capture_port(23) == 2 && effects.capture_port(24) == 0,
      "compacted live beam vector does not move captures between working apertures");
  require(effects.capture_port(21) == 0, "just-completed target retains its actual intake for the visual finish");
  std::reverse(run.beams.begin(), run.beams.end());
  effects.begin_capture_frame(run);
  require(effects.capture_port(22) == 1 && effects.capture_port(23) == 2, "held active time and snapshot reordering preserve intake binding");
  run.stats.active_seconds = .2;
  std::erase_if(run.beams, [](const auto& beam) { return beam.target_id == 23; });
  effects.begin_capture_frame(run);
  require(effects.capture_port(23) == 2, "removed target keeps its port during its complete .34second finish");
  run.stats.active_seconds = 1;
  effects.begin_capture_frame(run);
  require(effects.capture_port(23) == 0, "old finisher bindings retire boundedly");
  effects.reset();
  require(effects.capture_port(22) == 0, "new flight clears transient physical port assignments");
}
void motion_and_real_repair_deployment() {
  auto run = starter();
  run.research[static_cast<unsigned>(hr::gameplay::Research::fire_rate)] = 30;
  hr::product::SceneEffects effects;
  run.stats.active_seconds = 10;
  effects.update(run);
  const float angle = effects.rotor_angle();
  run.research[static_cast<unsigned>(hr::gameplay::Research::fire_rate)] = 0;
  effects.update(run);
  require_near(effects.rotor_angle(), angle, .000001F, "choosing another rank at held active time never jumps rotor phase");
  run.stats.active_seconds += .01;
  effects.update(run);
  require(std::abs(std::remainder(effects.rotor_angle() - angle, 2 * std::numbers::pi_v<float>)) < .06F, "ring acceleration stays continuous");
  equip(run, Module::repair_swarm, 3);
  StyleFrame docked, working;
  hr::product::append_ship_gear(docked, run, pose, false, 0);
  hr::product::append_ship_gear(working, run, pose, false, .5F);
  require(count(docked, "ship-maintenance-bay") == 3 && count(docked, "ship-repair-unit") == 3, "repair rank uses real recessed docks and units");
  require(instance(working, "ship-repair-unit").position.y > instance(docked, "ship-repair-unit").position.y, "actual heal response deploys the docked machinery");
  const auto first = present(run, pose, true);
  run.stats.active_seconds += 100;
  const auto later = present(run, pose, true);
  require(geometry(first) == geometry(later), "reduced motion keeps all physical upgrades and freezes cosmetic motion");
}
} // namespace
int main(int argc, char** argv) {
  require(argc == 2, "cooked physical geometry root is explicit");
  content = argv[1];
  all_modules_change_physical_geometry_in_each_rank();
  ring_geometry_and_cells_follow_their_actual_components();
  component_combinations_keep_their_responsibilities();
  muzzles_and_intakes_belong_to_the_remodelled_parts();
  drone_clearance_is_measured_from_the_full_physical_ship();
  maintenance_docks_clear_capture_and_engine_machinery();
  underbody_capture_casings_clear_the_rotating_ring();
  intake_bindings_survive_beam_compaction_and_finishing();
  motion_and_real_repair_deployment();
}
