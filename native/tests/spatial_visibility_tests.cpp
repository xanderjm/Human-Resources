#include "progression_test_backend.h"
#include "test_support.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <human_resources/assets/preview_mesh.h>
#include <human_resources/gameplay/tuning.h>
#include <human_resources/product/presentation.h>
#include <human_resources/product/visibility.h>
#include <map>
namespace {
namespace hr = human_resources;
using hr::presentation::StyleFrame;
using hr::presentation::StyleInstance;
using hr::presentation::Vec3;
using test_support::require;
using test_support::require_near;
std::map<std::string, hr::assets::PreviewMesh> meshes;
std::filesystem::path content;
const hr::assets::PreviewMesh& mesh(std::string_view id) {
  const auto name = std::string(id);
  if (meshes.contains(name))
    return meshes.at(name);
  std::ifstream file(content / (name + ".hrm"), std::ios::binary | std::ios::ate);
  require(file.good(), "real cooked mesh opens for projection regression");
  const auto size = file.tellg();
  require(size > 0 && size < 16 * 1024 * 1024, "bounded real mesh fixture");
  std::vector<std::byte> bytes(static_cast<std::size_t>(size));
  file.seekg(0);
  file.read(reinterpret_cast<char*>(bytes.data()), size);
  auto& result = meshes[name];
  require(hr::assets::decode_preview_mesh(bytes, result), "existing production decoder validates cooked projection fixture");
  return result;
}
std::array<float, 2> vertex_screen(const StyleFrame& f, const StyleInstance& i, hr::assets::PreviewVertex vertex, float width, float height) {
  auto p = vertex.position;
  if (vertex.rotor > .5F) {
    const float c = std::cos(f.ring_angle), s = std::sin(f.ring_angle), x = p[0], z = p[2];
    p[0] = c * x + s * z;
    p[2] = -s * x + c * z;
  }
  const auto m = hr::presentation::multiply(f.camera, hr::presentation::model_matrix(i));
  const float w = m[12] * p[0] + m[13] * p[1] + m[14] * p[2] + m[15];
  return {(1 + (m[0] * p[0] + m[1] * p[1] + m[2] * p[2] + m[3]) / w) * width * .5F, (1 - (m[4] * p[0] + m[5] * p[1] + m[6] * p[2] + m[7]) / w) * height * .5F};
}
bool player_mesh(std::string_view id) {
  return id == "ufo-modular" || id == "ufo-laser" || id == "ufo-beam" || id == "ufo-shield" || id == "ufo-rim-lens" || id == "shield-shell";
}
void full_gear_has_real_phone_margins() {
  require_near(hr::gameplay::tuning::field_half_width, 5.65F, .0001F, "approved player core boundary matches thirteen-metre band");
  require_near(hr::gameplay::tuning::enemy_spawn_ahead, 12.6F, .0001F, "approved corrected-source readable ingress bound");
  require_near(hr::gameplay::tuning::enemy_retire_behind, 11.F, .0001F, "approved readable retirement bound");
  for (auto size : {std::array<float, 2>{320, 568}, {390, 844}})
    for (auto theme : {hr::presentation::SceneryTheme::farmland, hr::presentation::SceneryTheme::suburbia, hr::presentation::SceneryTheme::desert})
      for (float side : {-1.F, 1.F})
        for (float along : {-6.F, 6.F})
          for (float bank : {-7.2F, 7.2F})
            for (unsigned phase = 0; phase < 32; ++phase) {
              hr::gameplay::Snapshot run;
              run.runner_distance = 100;
              run.player = {side * 5.65F, 100 + along};
              run.velocity = {bank, 2.2F};
              run.stats.active_seconds = static_cast<double>(phase) * 7.3919827 / 32;
              run.shield = run.maximum_shield = 100;
              run.laser_count = 4;
              run.beam_capacity = 6;
              for (auto module : {hr::gameplay::Module::laser_array, hr::gameplay::Module::orbit_drones, hr::gameplay::Module::shield_generator, hr::gameplay::Module::beam_splitter, hr::gameplay::Module::tractor_lens, hr::gameplay::Module::emp_ring})
                run.modules[static_cast<unsigned>(module)] = 3;
              run.evolution_bits = (1U << static_cast<unsigned>(hr::gameplay::Evolution::great_vacuum)) | (1U << static_cast<unsigned>(hr::gameplay::Evolution::storm_drain));
              for (unsigned i = 0; i < 4; ++i)
                run.shots.push_back({run.player, {(i & 1) ? -5.65F : 5.65F, 100 + ((i & 2) ? -6.F : 6.F)}, .1F, false, hr::gameplay::ShotKind::laser, static_cast<std::uint8_t>(i)});
              for (unsigned i = 0; i < 3; ++i)
                run.shots.push_back({run.player, {(i & 1) ? -5.65F : 5.65F, 100 + ((i & 2) ? -6.F : 6.F)}, .1F, true, hr::gameplay::ShotKind::drone, static_cast<std::uint8_t>(i)});
              const auto frame = hr::product::scene(run, size[0] / size[1], false, false, nullptr, theme);
              for (const auto& instance : frame.instances)
                if (player_mesh(instance.mesh))
                  for (auto vertex : mesh(instance.mesh).vertices) {
                    const auto p = vertex_screen(frame, instance, vertex, size[0], size[1]);
                    require(p[0] >= 6 && p[0] <= size[0] - 6, "actual banked/aimed maximum gear retains six real phone pixels at both sides");
                    require(p[1] >= 150 && p[1] < size[1] - 160, "maximum gear avoids top HUD and bottom controls at every dodge extreme");
                  }
            }
}
void threat_ingress_and_retirement_clear_real_controls() {
  progression_tests::MemoryBackend backend;
  hr::progression::AccountStore store(backend);
  (void)store.load();
  for (auto size : {std::array<float, 2>{320, 568}, {390, 844}}) {
    hr::gameplay::Snapshot run;
    run.phase = hr::gameplay::Phase::active;
    run.runner_distance = 100;
    run.stats.active_seconds = 30;
    run.modules[static_cast<unsigned>(hr::gameplay::Module::phase_drive)] = 1;
    const auto ui = hr::product::interface({.screen = hr::product::Screen::run, .run = &run, .store = &store, .viewport = {0, 0, size[0], size[1]}, .safe_area = {0, size[0] == 320 ? 20.F : 59.F, 0, size[0] == 320 ? 0.F : 34.F}});
    const auto pause = std::find_if(ui.buttons.begin(), ui.buttons.end(), [](const auto& b) { return b.command == hr::product::Command::pause; });
    const auto phase = std::find_if(ui.buttons.begin(), ui.buttons.end(), [](const auto& b) { return b.command == hr::product::Command::dash; });
    require(pause != ui.buttons.end() && phase != ui.buttons.end(), "real run controls define protected HUD boundaries");
    for (auto theme : {hr::presentation::SceneryTheme::farmland, hr::presentation::SceneryTheme::suburbia, hr::presentation::SceneryTheme::desert})
      for (float station : {hr::gameplay::tuning::enemy_spawn_ahead, -hr::gameplay::tuning::enemy_retire_behind})
        for (float side : {-1.F, 1.F})
          for (float player_z : {-6.F, 6.F})
            for (unsigned heading = 0; heading < 16; ++heading) {
              const float angle = static_cast<float>(heading) * 6.2831853F / 16;
              run.player = {-side * 5.65F, 100 + player_z};
              run.entities = {{.id = 1, .kind = hr::gameplay::EntityKind::boss, .position = {side * 5.65F, 100 + station}, .velocity = {std::sin(angle), -std::cos(angle)}, .warning_aim = run.player, .warning_seconds = 1}};
              const auto muzzle=hr::gameplay::vehicle_cannon_shot(run.entities.front(),run.player);
              require(muzzle.valid,"actual source cannon reaches legal player band at ingress/retirement");
              run.projectiles = {{.id = 2, .position = muzzle.origin, .velocity = muzzle.direction, .radius = .4F,
                  .source_id=1,.launch_position=muzzle.origin,.launch_height=muzzle.height,
                  .kind=hr::gameplay::EnemyProjectileKind::boss_missile,.launch_direction=muzzle.direction}};
              const auto frame = hr::product::scene(run, size[0] / size[1], false, false, nullptr, theme);
              for (const auto& instance : frame.instances)
                if (instance.mesh == "tank-hull" || instance.mesh == "tank-turret" || instance.mesh == "missile")
                  for (auto vertex : mesh(instance.mesh).vertices) {
                    const auto p = vertex_screen(frame, instance, vertex, size[0], size[1]);
                    if(p[1]<pause->bounds.bottom()+12 || p[1]>phase->bounds.y-10)
                      std::cerr<<"threat-phone-clearance mesh="<<instance.mesh<<" phone="<<size[0]<<'x'<<size[1]
                          <<" station="<<station<<" side="<<side<<" heading="<<heading<<" vertexY="<<p[1]
                          <<" required="<<pause->bounds.bottom()+12<<".."<<phase->bounds.y-10<<'\n';
                    require(p[1] >= pause->bounds.bottom() + 12, "actual boss and missile ingress clears the top HUD by twelve phone pixels");
                    require(p[1] <= phase->bounds.y - 10, "actual boss and missile retirement clears the bottom control by ten phone pixels");
                  }
            }
  }
}
void critical_geometry_controls_every_landmark() {
  for (auto id : {"barn", "house-coral", "house-teal", "cliff-module-a", "cliff-module-b", "cactus-tall", "windmill-mast", "windmill-rotor", "tree-round-large", "tree-round-small", "service-canopy"})
    require(hr::product::visibility_mesh_bounds(id).has_value(), "all tall source classes have cooked visibility bounds");
  for (auto id : {"barn", "cliff-module-a"}) {
    StyleFrame frame;
    hr::presentation::set_overhead_camera(frame, 390.F / 844, 17, {}, 27);
    frame.instances.push_back({.mesh = id, .position = {5.3F, 0, 3.5F}, .scale = {1.2F, 1.2F, 1.2F}});
    frame.instances.push_back({.mesh = "ufo-modular", .position = {-5.65F, 2.5F, 2.3F}});
    frame.instances.push_back({.mesh = "missile", .position = {5.3F, 2.2F, 2.3F}});
    hr::product::preserve_play_visibility(frame, 1);
    require(frame.instances[0].tint[3] < .3F, "foreground barn/cliff fades for real projectile despite being far from ship");
    require(frame.instances[1].tint[3] == 1 && frame.instances[2].tint[3] == 1, "critical source materials remain untouched");
  }
  StyleFrame warning;
  hr::presentation::set_overhead_camera(warning, 390.F / 844, 17, {}, 27);
  warning.instances.push_back({.mesh = "barn", .position = {0, 0, 2}, .scale = {1.2F, 1.2F, 1.2F}});
  hr::gameplay::Snapshot run;
  run.entities.push_back({.id = 1, .kind = hr::gameplay::EntityKind::soldier, .position = {-5, 5}, .warning_aim = {5, -1}, .warning_seconds = 1});
  hr::product::preserve_play_visibility(warning, 1, &run);
  require(warning.instances[0].tint[3] < .3F, "actual committed warning path stays readable even without a nearby ship");
}
void fading_is_continuous_and_requires_real_geometry() {
  float previous = 1;
  bool intermediate = false;
  for (unsigned step = 0; step <= 1000; ++step) {
    StyleFrame f;
    hr::presentation::set_overhead_camera(f, 390.F / 844, 17, {}, 27);
    f.instances.push_back({.mesh = "barn", .position = {10 - static_cast<float>(step) * .01F, 0, 3.5F}});
    f.instances.push_back({.mesh = "missile", .position = {0, 2.2F, 2.3F}});
    hr::product::preserve_play_visibility(f, 1);
    const auto alpha = f.instances[0].tint[3];
    require(alpha >= .119F && alpha <= 1, "bounded transparent coverage");
    require(std::abs(previous - alpha) < .04F, "spatial feather avoids abrupt alpha jumps during movement");
    intermediate = intermediate || (alpha > .2F && alpha < .9F);
    previous = alpha;
  }
  require(intermediate && previous < .3F, "continuous feather reaches actual protected overlap");
  StyleFrame empty;
  hr::presentation::set_overhead_camera(empty, 390.F / 844, 17, {}, 27);
  empty.instances.push_back({.mesh = "barn", .position = {0, 0, 3.5F}});
  hr::product::preserve_play_visibility(empty, 1);
  require(empty.instances[0].tint[3] == 1, "empty scene never invents a protected actor or outcome");
}
void shotgun_outer_lane_has_independent_protection() {
  hr::gameplay::Snapshot run;
  run.entities.push_back({.id = 1, .kind = hr::gameplay::EntityKind::soldier, .position = {0, 12}, .warning_aim = {0, -12}, .warning_seconds = 1, .infantry_role = hr::gameplay::InfantryRole::shotgunner});
  const auto& actor = run.entities.front();
  const auto muzzle = hr::gameplay::infantry_shot(actor.infantry_role, actor.position, actor.warning_aim);
  const auto attack = hr::gameplay::infantry_attack(actor.infantry_role);
  require(muzzle.valid && attack.pellets == 3, "legal committed shotgun fixture has three authoritative lanes");
  const float distance = std::hypot(actor.warning_aim.x - muzzle.origin.x, actor.warning_aim.z - muzzle.origin.z);
  const float angle = attack.spread_radians;
  const hr::gameplay::Vec2 edge{muzzle.origin.x + distance * (muzzle.direction.x * std::cos(angle) - muzzle.direction.z * std::sin(angle)),
      muzzle.origin.z + distance * (muzzle.direction.x * std::sin(angle) + muzzle.direction.z * std::cos(angle))};
  StyleFrame frame;
  hr::presentation::set_overhead_camera(frame, 390.F / 844, 17, {}, 27);
  frame.instances.push_back({.mesh = "cactus-tall", .position = {edge.x, 0, -edge.z + 3.5F}, .scale = {.4F, .4F, .4F}});
  auto centre_only = frame;
  auto untyped = run;
  untyped.entities.front().infantry_role = hr::gameplay::InfantryRole::none;
  hr::product::preserve_play_visibility(centre_only, 1, &untyped);
  hr::product::preserve_play_visibility(frame, 1, &run);
  require(centre_only.instances[0].tint[3] > .8F, "outer lane fixture lies beyond the centre path's protected footprint");
  require(frame.instances[0].tint[3] < .3F, "committed outer shotgun lane protects its actual foreground cactus");
}
} // namespace
int main(int argc, char** argv) {
  require(argc == 2, "cooked fixture root is explicit");
  content = argv[1];
  full_gear_has_real_phone_margins();
  threat_ingress_and_retirement_clear_real_controls();
  critical_geometry_controls_every_landmark();
  fading_is_continuous_and_requires_real_geometry();
  shotgun_outer_lane_has_independent_protection();
}
