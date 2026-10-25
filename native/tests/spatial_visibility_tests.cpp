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
std::array<float, 2> hud_vertical_limits(std::array<float, 2> size) {
  static std::map<unsigned, std::array<float, 2>> cache;
  const auto key = static_cast<unsigned>(size[0]);
  if (cache.contains(key))
    return cache.at(key);
  progression_tests::MemoryBackend backend;
  hr::progression::AccountStore store(backend);
  (void)store.load();
  hr::gameplay::Snapshot run;
  run.phase = hr::gameplay::Phase::active;
  const auto ui = hr::product::interface({.screen = hr::product::Screen::run, .run = &run, .store = &store, .viewport = {0, 0, size[0], size[1]}, .safe_area = {0, size[0] == 320 ? 20.F : 59.F, 0, size[0] == 320 ? 0.F : 34.F}});
  const auto pause = std::find_if(ui.buttons.begin(), ui.buttons.end(), [](const auto& b) { return b.command == hr::product::Command::pause; });
  const auto dash = std::find_if(ui.buttons.begin(), ui.buttons.end(), [](const auto& b) { return b.command == hr::product::Command::dash; });
  require(pause != ui.buttons.end() && dash != ui.buttons.end(), "actual HUD and control boundaries exist");
  return cache[key] = {pause->bounds.bottom() + 12, dash->bounds.y - 10};
}
void full_gear_has_real_phone_margins() {
  float worst_clearance = 10000, top_clearance = 10000, bottom_clearance = 10000;
  require_near(hr::gameplay::tuning::field_half_width, 7.5F, .0001F, "expanded player boundary matches the widened playable corridor");
  require_near(hr::gameplay::tuning::enemy_spawn_ahead, 26.F, .0001F, "authored horizon delivery starts at the portrait approach edge");
  require_near(hr::gameplay::tuning::encounter_warning_entry_ahead, 15.F, .0001F, "first attack commitment waits for the fully readable field");
  require_near(hr::gameplay::tuning::enemy_retire_behind, 23.F, .0001F, "retirement is distinct from the readable attack band");
  for (auto size : {std::array<float, 2>{320, 568}, {390, 844}})
    for (auto theme : {hr::presentation::SceneryTheme::farmland, hr::presentation::SceneryTheme::suburbia, hr::presentation::SceneryTheme::desert})
      for (float side : {-1.F, 1.F})
        for (float along : {-17.F, 16.5F})
          for (float bank : {-12.F, 12.F})
            for (float vertical : {-12.F, 12.F})
              for (unsigned phase = 0; phase < 32; ++phase) {
                hr::gameplay::Snapshot run;
                run.runner_distance = 100;
                run.player = {side * 7.5F, 100 + along};
                run.velocity = {bank, vertical};
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
                      const auto limits = hud_vertical_limits(size);
                      const float clearance = std::min(p[1] - limits[0], limits[1] - p[1]);
                      top_clearance = std::min(top_clearance, p[1] - limits[0]);
                      bottom_clearance = std::min(bottom_clearance, limits[1] - p[1]);
                      if (clearance < worst_clearance) {
                        worst_clearance = clearance;
                        std::cout << "PLAYER_MARGIN phone=" << size[0] << " along=" << along << " mesh=" << instance.mesh << " y=" << p[1] << " required=" << limits[0] << ".." << limits[1] << " clearance=" << clearance << '\n';
                      }
                    }
              }
  std::cout << "PLAYER_EXTREMES top_clearance=" << top_clearance << " bottom_clearance=" << bottom_clearance << "\n";
  require(worst_clearance >= 0, "complete maximum gear clears the real HUD by twelve pixels and control by ten");
}
void supported_stages_keep_real_flight_and_readable_controls() {
  struct Display { unsigned width, height; hr::ui::Insets insets; };
  constexpr std::array displays{
      Display{768,1024,{0,24,0,20}}, Display{1024,1366,{0,24,0,20}}, Display{1366,1024,{0,24,0,20}},
      Display{844,390,{59,0,59,21}}, Display{320,320,{}}, Display{320,500,{}},
      Display{390,568,{0,20,0,0}}, Display{844,390,{31,12,17,8}}};
  progression_tests::MemoryBackend backend;
  hr::progression::AccountStore store(backend);
  (void)store.load();
  hr::gameplay::Snapshot run;
  run.phase = hr::gameplay::Phase::active;
  run.runner_distance = 100;
  run.hull = run.maximum_hull = 112;
  run.shield = run.maximum_shield = 100;
  run.laser_count = 4;
  run.beam_capacity = 6;
  for (auto module : {hr::gameplay::Module::laser_array, hr::gameplay::Module::orbit_drones, hr::gameplay::Module::shield_generator, hr::gameplay::Module::beam_splitter, hr::gameplay::Module::tractor_lens, hr::gameplay::Module::emp_ring})
    run.modules[static_cast<unsigned>(module)] = 3;
  run.evolution_bits = (1U << static_cast<unsigned>(hr::gameplay::Evolution::great_vacuum)) | (1U << static_cast<unsigned>(hr::gameplay::Evolution::storm_drain));
  for (const auto display : displays) for (float text_scale : {1.F,2.F}) {
    const hr::ui::Rect viewport{0,0,static_cast<float>(display.width),static_cast<float>(display.height)};
    const auto framing = hr::product::gameplay_framing(viewport, display.insets, text_scale, {display.width*2,display.height*2});
    require(framing.stage.width > 100 && framing.stage.height > 180, "largest supported text leaves a real portrait combat stage");
    const auto actual = hr::presentation::fit_world_destination(framing.render_extent.width, framing.render_extent.height, display.width*2,display.height*2,framing.destination);
    require_near(framing.stage.x, static_cast<float>(actual.x)*.5F, .001F, "attachment stage matches exact renderer destination x");
    require_near(framing.stage.height, static_cast<float>(actual.height)*.5F, .001F, "attachment stage matches exact renderer destination height");
    hr::product::View view{hr::product::Screen::run,&run,&store,viewport,display.insets};
    view.text_scale=text_scale; view.framing=&framing;
    run.stats.active_seconds=30;
    const auto ui=hr::product::interface(view);
    require(!ui.text_overflow,"supported stage HUD measures without text loss at requested size");
    float top=framing.stage.y, bottom=framing.stage.bottom();
    for(const auto& button:ui.buttons) {
      require(button.bounds.width>=44 && button.bounds.height>=44,"complete Pause/Dash target retains forty-four points");
      require(button.bounds.x>=framing.safe.x && button.bounds.y>=framing.safe.y && button.bounds.right()<=framing.safe.right()+.01F && button.bounds.bottom()<=framing.safe.bottom()+.01F,"all targets lie within actual safe area");
      if(framing.compact) {
        const auto overlap=hr::ui::intersect(button.bounds,framing.stage);
        require(overlap.width<.01F || overlap.height<.01F,"entire compact control target stays outside world stage");
      } else {
        if(button.command==hr::product::Command::pause) top=button.bounds.bottom()+12;
        if(button.command==hr::product::Command::dash) bottom=button.bounds.y-10;
      }
    }
    for(const auto& quad:ui.quads)
      if(std::abs(quad.uv[2]-64.F/1024)<.00001F && std::abs(quad.uv[3]-80.F/2048)<.00001F)
        require(quad.rect[2]*viewport.width*48.F/64.F>=14*text_scale-.01F,"stage reflow preserves physical font floor and enlarged text");
    if(framing.compact) {
      auto damaged=view;
      damaged.damage_feedback={.age=.1F,.hull=false,.cause=hr::gameplay::DamageCause::contact};
      require(!hr::product::interface(damaged).text_overflow,"actual damage provenance wraps in short rails at enlarged text size");
      run.harvest_overdrive_seconds=8;
      require(!hr::product::interface(view).text_overflow,"short rails retain actual Overdrive confirmation without control overlap");
      run.harvest_overdrive_seconds=0;
    }
    float clearance=10000;
    for(float side:{-1.F,1.F}) for(float along:{-17.F,16.5F}) for(float bank:{-12.F,12.F}) for(float vertical:{-12.F,12.F}) for(unsigned phase=0;phase<32;++phase) {
      run.player={side*7.5F,100+along};run.velocity={bank,vertical};run.stats.active_seconds=30+static_cast<double>(phase)*7.3919827/32;
      run.shots.clear();
      for(unsigned i=0;i<4;++i)run.shots.push_back({run.player,{(i&1)?-5.65F:5.65F,100+((i&2)?-6.F:6.F)},.1F,false,hr::gameplay::ShotKind::laser,static_cast<std::uint8_t>(i)});
      for(unsigned i=0;i<3;++i)run.shots.push_back({run.player,{(i&1)?-5.65F:5.65F,100+((i&2)?-6.F:6.F)},.1F,true,hr::gameplay::ShotKind::drone,static_cast<std::uint8_t>(i)});
      const auto frame=hr::product::scene(run,framing,false);
      require(frame.camera==framing.camera && frame.world_destination==framing.destination,"scene consumes the actual shared projection and destination");
      for(const auto& instance:frame.instances)if(player_mesh(instance.mesh))for(auto vertex:mesh(instance.mesh).vertices) {
        auto point=vertex_screen(frame,instance,vertex,framing.stage.width,framing.stage.height);point[0]+=framing.stage.x;point[1]+=framing.stage.y;
        const float margin=std::min({point[0]-framing.stage.x,framing.stage.right()-point[0],point[1]-top,bottom-point[1]});
        clearance=std::min(clearance,margin);
        if(margin<0)std::cerr<<"STAGE_FAIL "<<display.width<<'x'<<display.height<<" text="<<text_scale<<" mesh="<<instance.mesh<<" point="<<point[0]<<','<<point[1]<<" band="<<top<<".."<<bottom<<'\n';
        require(margin>=0,"real banked maximum gear clears actual stage and all relevant controls across full movement");
      }
    }
    std::cout<<"SUPPORTED_STAGE "<<display.width<<'x'<<display.height<<" text="<<text_scale<<" rect="<<framing.stage.x<<','<<framing.stage.y<<','<<framing.stage.width<<','<<framing.stage.height<<" clearance="<<clearance<<'\n';
    for(float station:{hr::gameplay::tuning::encounter_warning_entry_ahead,-hr::gameplay::tuning::encounter_warning_retire_behind})for(float side:{-1.F,1.F})for(float player_along:{-17.F,16.5F})for(unsigned heading=0;heading<16;++heading) {
      run.player={-side*7.5F,100+player_along};
      const float angle=static_cast<float>(heading)*6.2831853F/16;
      run.entities={{.id=1,.kind=hr::gameplay::EntityKind::boss,.position={side*7.5F,100+station},.velocity={std::sin(angle),-std::cos(angle)},.warning_aim=run.player,.warning_seconds=1}};
      const auto muzzle=hr::gameplay::vehicle_cannon_shot(run.entities.front(),run.player);
      require(muzzle.valid,"real committed boss source still reaches full flight field");
      run.projectiles={{.id=2,.position=muzzle.origin,.velocity=muzzle.direction,.radius=.4F,.source_id=1,.launch_position=muzzle.origin,.launch_height=muzzle.height,.kind=hr::gameplay::EnemyProjectileKind::boss_missile,.launch_direction=muzzle.direction}};
      const auto frame=hr::product::scene(run,framing,false);
      for(const auto& instance:frame.instances)if(instance.mesh=="tank-hull" || instance.mesh=="tank-turret" || instance.mesh=="missile")for(auto vertex:mesh(instance.mesh).vertices) {
        auto point=vertex_screen(frame,instance,vertex,framing.stage.width,framing.stage.height);point[1]+=framing.stage.y;
        require(point[1]>=top && point[1]<=bottom,"real first-commit and retirement source/missile clear stage and HUD on supported surfaces");
      }
    }
    run.entities.clear();run.projectiles.clear();
  }
  for(const auto size:{std::array<unsigned,2>{320,568},{390,844}}) {
    const auto framing=hr::product::gameplay_framing({0,0,static_cast<float>(size[0]),static_cast<float>(size[1])},{},1,{size[0]*2,size[1]*2});
    const float factor=std::min(1.F,1280.F/static_cast<float>(size[1]*2));
    const auto old_w=static_cast<unsigned>(static_cast<int>(static_cast<float>(size[0]*2)*factor)/4*4),old_h=static_cast<unsigned>(static_cast<int>(static_cast<float>(size[1]*2)*factor)/4*4);
    StyleFrame legacy;hr::presentation::set_overhead_camera(legacy,static_cast<float>(old_w)/static_cast<float>(old_h),17.F/.75F,{0,0,-.15F},27);
    require(framing.camera==legacy.camera && framing.render_extent.width==old_w && framing.render_extent.height==old_h,"accepted physical phone camera and capped source remain bit exact to old formula");
    require(framing.destination==std::array<float,4>{0,0,1,1},"accepted phone renderer destination remains exact full output");
  }
}
void threat_attack_entry_and_retirement_clear_real_controls() {
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
      for (float station : {hr::gameplay::tuning::encounter_warning_entry_ahead, -hr::gameplay::tuning::encounter_warning_retire_behind})
        for (float side : {-1.F, 1.F})
          for (float player_z : {-17.F, 16.5F})
            for (unsigned heading = 0; heading < 16; ++heading) {
              const float angle = static_cast<float>(heading) * 6.2831853F / 16;
              run.player = {-side * 7.5F, 100 + player_z};
              run.entities = {{.id = 1, .kind = hr::gameplay::EntityKind::boss, .position = {side * 7.5F, 100 + station}, .velocity = {std::sin(angle), -std::cos(angle)}, .warning_aim = run.player, .warning_seconds = 1}};
              const auto muzzle=hr::gameplay::vehicle_cannon_shot(run.entities.front(),run.player);
              require(muzzle.valid, "actual source cannon reaches legal player band at first attack/retirement");
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
                    require(p[1] >= pause->bounds.bottom() + 12, "actual boss and missile first attack clears the top HUD by twelve phone pixels");
                    require(p[1] <= phase->bounds.y - 10, "actual boss and missile retirement clears the bottom control by ten phone pixels");
                  }
            }
  }
}
void horizon_approach_is_continuous_before_attacks() {
  progression_tests::MemoryBackend backend;
  hr::progression::AccountStore store(backend);
  (void)store.load();
  for (auto size : {std::array<float, 2>{320, 568}, {390, 844}}) {
    hr::gameplay::Snapshot run;
    run.phase = hr::gameplay::Phase::active;
    run.runner_distance = 100;
    run.stats.active_seconds = 30;
    const auto ui = hr::product::interface({.screen = hr::product::Screen::run, .run = &run, .store = &store, .viewport = {0, 0, size[0], size[1]}, .safe_area = {0, size[0] == 320 ? 20.F : 59.F, 0, size[0] == 320 ? 0.F : 34.F}});
    const auto pause = std::find_if(ui.buttons.begin(), ui.buttons.end(), [](const auto& b) { return b.command == hr::product::Command::pause; });
    require(pause != ui.buttons.end(), "actual run HUD defines the approach reference");
    float previous_center = -10000;
    for (float station : {hr::gameplay::tuning::enemy_spawn_ahead, 21.F, 17.F, hr::gameplay::tuning::encounter_warning_entry_ahead}) {
      run.entities = {{.id = 1, .kind = hr::gameplay::EntityKind::boss, .position = {0, 100 + station}, .velocity = {0, -1}}};
      const auto frame = hr::product::scene(run, size[0] / size[1], false, false);
      float top = 10000, bottom = -10000;
      for (const auto& instance : frame.instances)
        if (instance.mesh == "tank-hull" || instance.mesh == "tank-turret")
          for (auto vertex : mesh(instance.mesh).vertices) {
            const auto point = vertex_screen(frame, instance, vertex, size[0], size[1]);
            top = std::min(top, point[1]);
            bottom = std::max(bottom, point[1]);
          }
      require(top < bottom, "actual source boss geometry exists throughout the approach");
      const float center = (top + bottom) * .5F;
      require(center > previous_center + 1, "source geometry approaches monotonically through the portrait horizon");
      previous_center = center;
      const StyleInstance ground{.position = {0, 0, 2.3F - station}};
      const auto root = vertex_screen(frame, ground, {}, size[0], size[1]);
      if (station == hr::gameplay::tuning::enemy_spawn_ahead) {
        require(top < pause->bounds.bottom(), "approach begins above the playable HUD boundary instead of popping into midfield");
        require(root[1] < size[1] * .15F, "the real ground projection places horizon delivery in the upper fifteen percent");
      } else if (station == hr::gameplay::tuning::encounter_warning_entry_ahead) {
        require(top >= pause->bounds.bottom() + 12, "entire source boss becomes readable before any attack may commit");
        require(root[1] > pause->bounds.bottom() + 12, "actual first-commit ground projection is inside the readable field");
      }
      std::cout << "APPROACH phone=" << size[0] << 'x' << size[1] << " ahead=" << station
                << " bodyY=" << top << ".." << bottom << " groundY=" << root[1] << '\n';
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
void boss_outer_lanes_protect_consumed_canopy() {
  // Actual consumed Farmland tree pose from station33.6's opaque left fan
  // intersection; mirror it to exercise both committed outer directions. The
  // boss is inside the15m readable band and18m commit range. The player has
  // dodged away from the locked aim, so its hull cannot mask a missing lane.
  for (float aspect : {320.F / 568.F, 390.F / 844.F})
    for (float side : {-1.F, 1.F}) {
      hr::gameplay::Snapshot run;
      run.player = {0, 0};
      run.entities.push_back({.id = 1001, .kind = hr::gameplay::EntityKind::boss,
          .position = {side * 4.5F, 14}, .warning_aim = {side * 7.5F, 0}, .warning_seconds = 1, .body_heading = 0});
      const auto pattern = hr::gameplay::cannon_geometry();
      const auto muzzle = hr::gameplay::vehicle_cannon_shot(run.entities[0], run.entities[0].warning_aim);
      require(pattern.boss_projectiles == 3 && muzzle.valid, "actual boss geometry supplies three valid committed lanes");
      StyleFrame frame;
      hr::presentation::set_overhead_camera(frame, aspect, 17.F / .75F, {0, 0, -.15F}, 27);
      frame.instances.push_back({.mesh = "tree-round-large", .position = {side * 12.1884174F, 0, 2.6F},
          .scale = {1.665F, 1.665F, 1.665F}, .rotation = {0, side < 0 ? .3F : -.3F, 0}});
      auto centre_only = frame;
      auto tank = run;
      tank.entities[0].kind = hr::gameplay::EntityKind::tank;
      hr::product::preserve_play_visibility(centre_only, 1, &tank);
      hr::product::preserve_play_visibility(frame, 1, &run);
      require(centre_only.instances[0].tint[3] > .8F, "real canopy lies outside a single cannon lane's protected region");
      require(frame.instances[0].tint[3] < .3F, "both actual boss outer lanes protect the consumed canopy independently");
    }
}
} // namespace
int main(int argc, char** argv) {
  require(argc == 2, "cooked fixture root is explicit");
  content = argv[1];
  full_gear_has_real_phone_margins();
  supported_stages_keep_real_flight_and_readable_controls();
  threat_attack_entry_and_retirement_clear_real_controls();
  horizon_approach_is_continuous_before_attacks();
  critical_geometry_controls_every_landmark();
  fading_is_continuous_and_requires_real_geometry();
  shotgun_outer_lane_has_independent_protection();
  boss_outer_lanes_protect_consumed_canopy();
}
