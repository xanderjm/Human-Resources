#pragma once
// QA-only initial fixture access. Subsequent simulation uses public Game.
#include "run_state.h"
#include <SDL3/SDL.h>
#include <human_resources/product/presentation.h>
#include <human_resources/product/scene_effects.h>
#include <human_resources/renderer/style_renderer.h>
#include <iostream>

namespace hrqa {
inline void vehicle_shot_readability_proof(human_resources::renderer::StyleRenderer& renderer,
    human_resources::progression::AccountStore& store,const char* path) {
  namespace hr=human_resources;
  auto* stream=SDL_IOFromFile(path,"wb");
  if(!stream) throw std::runtime_error(SDL_GetError());
  // Exact first132 Game frames of the accepted17s initial fixture. Render only
  // frames60..131 of each source, at actual320px width, without capture reruns.
  for(auto kind:{hr::gameplay::EntityKind::jeep,hr::gameplay::EntityKind::tank,hr::gameplay::EntityKind::boss}) {
    hr::gameplay::Game game;game.start({.seed=514,.unlocked_blueprints=0xffff});auto& state=*game.state_;
    state.actors.clear();
    state.view.modules[static_cast<unsigned>(hr::gameplay::Module::heavy_winch)]=3;
    state.view.modules[static_cast<unsigned>(hr::gameplay::Module::beam_splitter)]=2;
    hr::gameplay::detail::Actor actor;
    actor.view.id=state.next_id++;actor.view.kind=kind;actor.view.position={0,kind==hr::gameplay::EntityKind::jeep ? 13.F : 12.6F};
    actor.view.hull=400;actor.view.maximum_hull=4000;actor.attack_timer=.1F;
    state.actors.push_back(actor);state.sync();hr::product::SceneEffects effects;bool fired=false;
    for(unsigned frame=0;frame<132;++frame) {
      game.advance(1.F/60);
      auto run=game.snapshot();
      std::erase_if(run.entities,[&](const auto& value){return value.id!=actor.view.id;});
      std::erase_if(run.projectiles,[&](const auto& value){return value.source_id!=actor.view.id;});
      std::erase_if(run.beams,[&](const auto& value){return value.target_id!=actor.view.id;});run.shots.clear();
      effects.update(run);
      if(!run.projectiles.empty() && !fired) {
        fired=true;std::cout<<"readability-shot kind="<<static_cast<unsigned>(kind)<<" source_frame="<<frame<<" shots="<<run.projectiles.size()<<'\n';
      }
      if(frame<60) continue;
      auto scene=hr::product::scene(run,320.F/696.F,false,false,&effects);scene.active_delta_seconds=1.F/60;
      hr::product::View view;view.screen=hr::product::Screen::run;view.run=&run;view.store=&store;view.viewport={0,0,390,844};
      scene.screen_quads=hr::product::interface(view).quads;
      if(!renderer.render(scene,stream,false)) throw std::runtime_error(SDL_GetError());
    }
    if(!fired) throw std::runtime_error("Readability fixture did not emit its actual source shot");
  }
  SDL_CloseIO(stream);
  if(!renderer.wait_idle()) throw std::runtime_error(SDL_GetError());
}
inline void vehicle_transition_proof(human_resources::renderer::StyleRenderer& renderer,
    human_resources::progression::AccountStore& store,const char* path) {
  namespace hr=human_resources;
  auto* stream=SDL_IOFromFile(path,"wb");
  if(!stream) throw std::runtime_error(SDL_GetError());
  std::cout<<"SCRIPTED INITIAL GAME FIXTURE: seed514, synthetic damaged400/4000 vehicle and Winch3/Splitter2; actual public Game advance, isolated source composition, no natural-earned claim\n";
  unsigned output_frame=0;
  for(auto kind:{hr::gameplay::EntityKind::jeep,hr::gameplay::EntityKind::tank,hr::gameplay::EntityKind::boss}) {
    hr::gameplay::Game game;game.start({.seed=514,.unlocked_blueprints=0xffff});
    auto& state=*game.state_;
    state.actors.clear();
    state.view.modules[static_cast<unsigned>(hr::gameplay::Module::heavy_winch)]=3;
    state.view.modules[static_cast<unsigned>(hr::gameplay::Module::beam_splitter)]=2;
    hr::gameplay::detail::Actor actor;
    actor.view.id=state.next_id++;actor.view.kind=kind;
    // This isolated jeep starts40cm beyond ordinary ingress so its moving
    // muzzle completes one shot before the deliberate4.5m safe-range stop.
    actor.view.position={0,kind==hr::gameplay::EntityKind::jeep ? 13.F : 12.6F};
    actor.view.hull=400;actor.view.maximum_hull=4000;actor.attack_timer=.1F;
    state.actors.push_back(actor);state.sync();
    const unsigned count=kind==hr::gameplay::EntityKind::jeep ? 360U : kind==hr::gameplay::EntityKind::tank ? 390U : 270U;
    hr::product::SceneEffects effects;float previous_progress=0;bool fired=false,cancelled=false,landed=false;
    unsigned previous_shots=0;
    for(unsigned frame=0;frame<count;++frame,++output_frame) {
      if(game.snapshot().phase==hr::gameplay::Phase::choosing_upgrade && !game.choose_upgrade(0))
        throw std::runtime_error("Seeded vehicle fixture cannot resolve actual draft");
      const float forward=kind!=hr::gameplay::EntityKind::boss && frame>=150 && frame<270 ? 1.F : 0.F;
      game.advance(1.F/60,{{0,forward},false});
      auto run=game.snapshot();
      std::erase_if(run.entities,[&](const auto& value){return value.id!=actor.view.id;});
      std::erase_if(run.projectiles,[&](const auto& value){return value.source_id!=actor.view.id;});
      std::erase_if(run.beams,[&](const auto& value){return value.target_id!=actor.view.id;});
      run.shots.clear();
      if(run.entities.empty()) throw std::runtime_error("Seeded vehicle unexpectedly disappeared before full return proof");
      const auto& body=run.entities.front();
      if(!run.projectiles.empty()) fired=true;
      if(previous_progress>0 && body.capture_progress==0) cancelled=true;
      if(cancelled && body.fire_recovery_seconds==0) landed=true;
      if(frame==0 || frame%60==0 || (previous_progress>0)!=(body.capture_progress>0) || previous_shots!=run.projectiles.size())
        std::cout<<"vehicle-trace output_frame="<<output_frame<<" kind="<<static_cast<unsigned>(kind)<<" actor="<<body.id
            <<" seconds="<<run.stats.active_seconds<<" player="<<run.player.x<<','<<run.player.z<<" ground="<<body.position.x<<','<<body.position.z
            <<" progress="<<body.capture_progress<<" recovery="<<body.fire_recovery_seconds<<" warning="<<body.warning_seconds
            <<" shots="<<run.projectiles.size()<<'\n';
      previous_progress=body.capture_progress;previous_shots=static_cast<unsigned>(run.projectiles.size());
      effects.update(run);
      auto scene=hr::product::scene(run,588.F/1280.F,false,false,&effects);
      scene.active_delta_seconds=1.F/60;
      hr::product::View view;view.screen=hr::product::Screen::run;view.run=&run;view.store=&store;view.viewport={0,0,390,844};
      scene.screen_quads=hr::product::interface(view).quads;
      if(!renderer.render(scene,stream,false)) throw std::runtime_error(SDL_GetError());
    }
    if(!fired || (kind!=hr::gameplay::EntityKind::boss && (!cancelled || !landed)))
      throw std::runtime_error("Seeded native fixture missed its required actual fire/cancel/fullreturn outcome");
    std::cout<<"vehicle-block kind="<<static_cast<unsigned>(kind)<<" frames="<<count<<" fired="<<fired<<" cancelled="<<cancelled<<" fullreturn="<<landed<<'\n';
  }
  SDL_CloseIO(stream);
  if(!renderer.wait_idle()) throw std::runtime_error(SDL_GetError());
}
}
