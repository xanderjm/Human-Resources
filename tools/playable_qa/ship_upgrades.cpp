// Labelled presentation-only upgrade inspection. No Game advance or save IO.
#include <SDL3/SDL.h>
#include <array>
#include <algorithm>
#include <memory>
#include <cmath>
#include <numbers>
#include <fstream>
#include <human_resources/gameplay/tuning.h>
#include <human_resources/product/presentation.h>
#include <human_resources/product/scene_effects.h>
#include <human_resources/product/ship_appearance.h>
#include <human_resources/renderer/style_renderer.h>
#include <iostream>
#include <stdexcept>
#include <string>
namespace hr = human_resources;
void check(bool value) {
  if (!value)
    throw std::runtime_error(SDL_GetError());
}
int main(int argc, char** argv) {
  if (argc == 2 && std::string_view(argv[1]) == "--validate-only") {
    std::cout << "Synthetic presentation fixture; no gameplay/storage.\n";
    return 0;
  }
  if (argc != 3 && argc != 4)
    return 2;
  check(SDL_Init(SDL_INIT_VIDEO));
  auto* window = SDL_CreateWindow("Human Resources / Ship Upgrade Inspection", 390, 844, SDL_WINDOW_HIGH_PIXEL_DENSITY);
  check(window != nullptr);
  {
    hr::renderer::StyleRenderer renderer;
    const auto ids = hr::product::content_ids();
    check(renderer.initialize(window, argv[1], 588, 1280, ids));
    std::ofstream manifest(std::string(argv[2]) + "/frames.csv");
    manifest << "name,instances,world_draws,shadow_draws,triangles\n";
    const auto capture = [&](const hr::gameplay::Snapshot& run, std::string name, bool reduced = false, bool overhead = false, hr::product::SceneEffects* effects = nullptr) {
      auto frame = hr::product::scene(run, 390.F / 844.F, reduced, false, effects);
      // The diagnostic top camera is separate from actual phone framing.
      if (overhead)
        hr::presentation::set_overhead_camera(frame, 390.F / 844.F, 17.F, {0, 0, 2.3F}, .001F);
      auto* stream = SDL_IOFromFile((std::string(argv[2]) + "/" + name + ".rgba").c_str(), "wb");
      check(stream != nullptr);
      const bool ok = renderer.render(frame, stream, false, {390, 844});
      SDL_CloseIO(stream);
      check(ok);
      const auto stats = renderer.stats();
      manifest << name << ',' << frame.instances.size() << ',' << stats.world_draws << ',' << stats.shadow_draws << ',' << stats.triangles << '\n';
    };
    // Optional separately cooked neutral-material tree, with production mesh
    // positions/normals/indices untouched. Native neutral geometry review.
    std::unique_ptr<SDL_Window,decltype(&SDL_DestroyWindow)> clay_window{
        argc==4?SDL_CreateWindow("Human Resources / Neutral Ship Geometry",390,844,SDL_WINDOW_HIGH_PIXEL_DENSITY):nullptr,SDL_DestroyWindow};
    hr::renderer::StyleRenderer clay;
    if(argc==4) {check(clay_window!=nullptr);check(clay.initialize(clay_window.get(),argv[3],588,1280,ids));}
    const auto capture_clay = [&](const hr::gameplay::Snapshot& run, const std::string& name) {
      if(argc!=4) return;
      auto frame=hr::product::scene(run,390.F/844.F,true);
      std::erase_if(frame.instances,[](const auto& p){ return !p.damage_reactive&&p.mesh!="ship-drone"&&p.mesh!="ship-drone-elite"; });
      for(auto& p:frame.instances) {
        p.tint={1,1,1,1};p.rotor_style[1]=p.rotor_style[2]=p.rotor_style[3]=1;
        p.polished_hull=false;p.hit_flash={};
      }
      for(bool top:{false,true}) {
        hr::presentation::set_overhead_camera(frame,390.F/844.F,17.F,{0,0,2.3F},top?.001F:27.F);
        auto* stream=SDL_IOFromFile((std::string(argv[2])+"/clay-"+name+(top?"-top":"")+".rgba").c_str(),"wb");
        check(stream!=nullptr);const bool ok=clay.render(frame,stream,false,{390,844});SDL_CloseIO(stream);check(ok);
      }
    };
    hr::gameplay::Snapshot starter;
    starter.phase = hr::gameplay::Phase::active;
    starter.hull = starter.maximum_hull = 100;
    starter.shield = starter.maximum_shield = 40;
    starter.laser_count = 1;
    starter.beam_capacity = 1;
    capture(starter, "starter");
    capture_clay(starter,"starter");
    capture(starter, "starter-top", false, true);
    for (unsigned m = 0; m < static_cast<unsigned>(hr::gameplay::Module::count); ++m)
      if(hr::gameplay::tuning::available_blueprints & (1U<<m))
      for (unsigned rank = 1; rank <= 3; ++rank) {
        auto run = starter;
        run.modules[m] = static_cast<std::uint8_t>(rank);
        if (m == static_cast<unsigned>(hr::gameplay::Module::laser_array))
          run.laser_count += rank;
        if (m == static_cast<unsigned>(hr::gameplay::Module::beam_splitter))
          run.beam_capacity += rank;
        const auto name = std::string(hr::gameplay::module_id(static_cast<hr::gameplay::Module>(m))) + "-" + std::to_string(rank);
        capture(run, name);
        capture_clay(run,name);
        if (rank == 3)
          capture(run, name + "-top", false, true);
      }
    for (unsigned r = 0; r < 6; ++r) {
      auto run = starter;
      run.research[r] = 20;
      capture(run, "research-" + std::to_string(r));
    }
    for (unsigned e : hr::gameplay::tuning::active_evolutions) {
      auto run = starter;
      run.evolution_bits = 1U << e;
      for(auto ingredient:hr::gameplay::tuning::evolution_ingredients[e]) run.modules[ingredient]=3;
      if(run.modules[static_cast<unsigned>(hr::gameplay::Module::laser_array)]) run.laser_count=4;
      if(run.modules[static_cast<unsigned>(hr::gameplay::Module::beam_splitter)]) run.beam_capacity=4;
      if(e==static_cast<unsigned>(hr::gameplay::Evolution::great_vacuum)) run.beam_capacity=6;
      capture(run, "evolution-" + std::to_string(e));
    }
    auto run = starter;
    for (auto m : {hr::gameplay::Module::beam_splitter, hr::gameplay::Module::emp_ring, hr::gameplay::Module::shield_generator, hr::gameplay::Module::repair_swarm, hr::gameplay::Module::tractor_lens, hr::gameplay::Module::orbit_drones})
      run.modules[static_cast<unsigned>(m)] = 3;
    run.evolution_bits = (1U << static_cast<unsigned>(hr::gameplay::Evolution::storm_drain)) |
                         (1U << static_cast<unsigned>(hr::gameplay::Evolution::great_vacuum));
    for (auto& r : run.research)
      r = 20;
    run.beam_capacity=6;
    capture(run, "combined");
    capture_clay(run,"combined");
    capture(run, "combined-reduced", true);
    capture(run, "combined-top", false, true);
    using M=hr::gameplay::Module;
    const std::array<std::array<M,6>,3> loadouts{{
      {M::laser_array,M::rail_lens,M::arc_coil,M::shield_generator,M::phase_drive,M::vector_thrusters},
      {M::beam_splitter,M::tractor_lens,M::heavy_winch,M::emp_ring,M::vector_thrusters,M::repair_swarm},
      {M::shield_generator,M::tractor_lens,M::emp_ring,M::repair_swarm,M::phase_drive,M::orbit_drones}}};
    for(unsigned build=0;build<loadouts.size();++build) {
      auto combination=starter;
      for(auto m:loadouts[build]) combination.modules[static_cast<unsigned>(m)]=3;
      combination.laser_count=build==0?4:1;
      combination.beam_capacity=build==1?6:1;
      if(build==0) combination.evolution_bits=1U<<static_cast<unsigned>(hr::gameplay::Evolution::office_shredder);
      if(build==1) combination.evolution_bits=(1U<<static_cast<unsigned>(hr::gameplay::Evolution::great_vacuum))|(1U<<static_cast<unsigned>(hr::gameplay::Evolution::hostile_acquisition));
      if(build==2) combination.evolution_bits=(1U<<static_cast<unsigned>(hr::gameplay::Evolution::storm_drain))|(1U<<static_cast<unsigned>(hr::gameplay::Evolution::employee_benefits));
      const auto name="loadout-"+std::to_string(build);
      capture(combination,name);capture(combination,name+"-top",false,true);capture_clay(combination,name);
      if(build==0) for(unsigned direction=0;direction<12;++direction) {
        const float angle=static_cast<float>(direction)*2*std::numbers::pi_v<float>/12;
        combination.shots.clear();
        for(unsigned gun=0;gun<4;++gun)
          combination.shots.push_back({{}, {std::cos(angle)*5,std::sin(angle)*5},.1F,false,hr::gameplay::ShotKind::laser,static_cast<std::uint8_t>(gun)});
        capture(combination,"aim-"+std::to_string(direction));
        capture_clay(combination,"aim-"+std::to_string(direction));
      }
    }
    for (float x : {-5.65F, 5.65F}) {
      run.player.x = x;
      capture(run, x < 0 ? "combined-left-edge" : "combined-right-edge");
    }
    run.player = {};
    run.evolution_bits = 0;
    hr::product::SceneEffects effects;
    std::ofstream phases(std::string(argv[2]) + "/motion.csv");
    phases << "frame,active_seconds,angle\n";
    for (unsigned frame = 0; frame < 180; ++frame) {
      // 1s base, choose Fire Rate research without advancing time, 1s faster, 1s pause.
      if (frame == 0)
        run.research[static_cast<unsigned>(hr::gameplay::Research::fire_rate)] = 0;
      if (frame == 60)
        run.research[static_cast<unsigned>(hr::gameplay::Research::fire_rate)] = 50;
      if (frame < 120)
        run.stats.active_seconds += 1. / 60;
      effects.update(run);
      phases << frame << ',' << run.stats.active_seconds << ',' << effects.rotor_angle() << '\n';
      if (frame % 10 == 0)
        capture(run, "motion-" + std::to_string(frame), false, false, &effects);
    }
    std::cout << "SYNTHETIC ship upgrade inspection backend=" << renderer.backend() << '\n';
    check(renderer.wait_idle());
  }
  SDL_DestroyWindow(window);
  SDL_Quit();
}
