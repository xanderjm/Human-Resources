// Independent audit: production gameplay, public controls and draft application.
#include "run_state.h"
#include <human_resources/progression/account_session.h>
#include "progression_test_backend.h"
#include <bit>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <set>
#include <string>
using namespace human_resources::gameplay;
unsigned failures{};
void require(bool value,const char* name){std::cout<<(value?"PASS ":"FAIL ")<<name<<'\n';failures+=!value;}
float square(Vec2 a,Vec2 b){return (a.x-b.x)*(a.x-b.x)+(a.z-b.z)*(a.z-b.z);}
std::size_t draft(const Snapshot& s,unsigned policy){
  float best=-1e9F;std::size_t selected=0;
  for(std::size_t i=0;i<3;++i){const auto&c=s.choices[i];float utility=1;
    if(c.hardware){
      switch(c.module){
        case Module::beam_splitter:utility=policy==1?100:20;break;
        case Module::tractor_lens:utility=policy==1?90:15;break;
        case Module::laser_array:utility=policy==2?100:70;break;
        case Module::shield_generator:utility=policy==3?100:35;break;
        case Module::repair_swarm:utility=policy==3?95:40;break;
        case Module::orbit_drones:utility=policy==2?90:60;break;
        case Module::emp_ring:utility=policy==2?80:50;break;
        case Module::heavy_winch:utility=policy==1?80:25;break;
        case Module::reserve_battery:utility=policy==2?70:25;break;
        default:utility=30;break;
      } utility+=static_cast<float>(s.modules[static_cast<unsigned>(c.module)])*8;
    }else switch(c.research){
      case Research::weapon_damage:utility=policy==2?60:20;break;
      case Research::capture_speed:utility=policy==1?70:20;break;
      case Research::emergency_repairs:utility=(s.maximum_hull-s.hull)*.8F;break;
      case Research::hull_reinforcement:utility=policy==3?70:25;break;
      case Research::shield_capacity:utility=policy==3?65:20;break;
      case Research::fire_rate:utility=policy==2?55:20;break;
      default:utility=5;break;
    }
    if(!c.evolution_preview.empty())utility+=100;
    if(utility>best){best=utility;selected=i;}
  }return selected;
}
Control steer(const Snapshot&s,unsigned policy){
  if(policy==0)return {};
  Vec2 goal={0,s.runner_distance+1};float best=1e9F;
  for(const auto&e:s.entities)if(e.kind<=EntityKind::soldier){const auto d=square(s.player,e.position);if(d<best){goal=e.position;best=d;}}
  Vec2 result={(goal.x-s.player.x)*1.3F,(goal.z-s.player.z)*1.3F};
  // Bounded near-future steering away from bullets and committed warning aims.
  for(const auto&p:s.projectiles){Vec2 q={p.position.x+p.velocity.x*.18F,p.position.z+p.velocity.z*.18F};float d=square(s.player,q);if(d<16){result.x+=(s.player.x-q.x)*8/std::max(.3F,d);result.z+=(s.player.z-q.z)*8/std::max(.3F,d);}}
  for(const auto&e:s.entities)if(e.warning_seconds>0){float d=square(s.player,e.warning_aim);if(d<5){result.x+=(s.player.x-e.warning_aim.x)*2/std::max(.3F,d);result.z+=(s.player.z-e.warning_aim.z)*2/std::max(.3F,d);}}
  return {result,false};
}
void paced_runs(){
  for(unsigned account=0;account<2;++account)for(unsigned policy=0;policy<4;++policy){std::vector<double>times;unsigned alive=0,evolutions=0;std::uint64_t captures=0;std::uint32_t levels=0;std::size_t peak_entities=0,peak_projectiles=0;
    for(unsigned seed=21;seed<=32;++seed){Game g;RunSetup setup{.seed=seed,.unlocked_blueprints=account?65535U:0U};if(account)setup.permanent_ranks={5,5,5,5};g.start(setup);
      for(unsigned tick=0;tick<60*1200&&g.snapshot().phase!=Phase::dead;++tick){while(g.snapshot().phase==Phase::choosing_upgrade)(void)g.choose_upgrade(draft(g.snapshot(),policy));g.advance(1.F/60,steer(g.snapshot(),policy));peak_entities=std::max(peak_entities,g.snapshot().entities.size());peak_projectiles=std::max(peak_projectiles,g.snapshot().projectiles.size());}
      const auto&s=g.snapshot();times.push_back(s.stats.active_seconds);alive+=s.phase!=Phase::dead;evolutions+=s.stats.evolutions;captures+=s.stats.captures;levels+=s.stats.level;
    }std::sort(times.begin(),times.end());std::cout<<"RUN account="<<account<<" policy="<<policy<<" n=12 min="<<times.front()<<" median="<<(times[5]+times[6])/2<<" max="<<times.back()<<" alive20m="<<alive<<" mean_level="<<static_cast<float>(levels)/12<<" mean_captures="<<captures/12<<" evolutions="<<evolutions<<" peak_entities="<<peak_entities<<" peak_projectiles="<<peak_projectiles<<'\n';
  }
}
void saturated_and_boundary(){
 Game g;g.start({.seed=42,.permanent_ranks={255,255,255,255},.unlocked_blueprints=0xffffffffU});auto&s=*g.state_;
 require(g.snapshot().maximum_hull==130&&g.snapshot().maximum_shield==40,"out-of-contract permanent ranks clamp at public setup boundary");
 const auto before=g.snapshot().stats.active_seconds;
 g.advance(std::numeric_limits<float>::quiet_NaN());g.advance(std::numeric_limits<float>::infinity());g.advance(-1);
 require(g.snapshot().stats.active_seconds==before,"nonfinite and negative time never mutate active clocks");
 g.advance(1.F/60,{{std::numeric_limits<float>::max(),std::numeric_limits<float>::max()},true});
 require(std::isfinite(g.snapshot().player.x)&&std::isfinite(g.snapshot().player.z),"finite maximum control vector preserves finite player state");
 s.actors.clear();s.bullets.clear();s.view.beams.clear();s.view.invulnerability_seconds=100000;s.view.modules.fill(0);
 for(Module m:{Module::laser_array,Module::rail_lens,Module::orbit_drones,Module::emp_ring,Module::beam_splitter,Module::tractor_lens})s.view.modules[static_cast<unsigned>(m)]=3;
 s.view.research[4]=100;s.view.research[5]=50;s.evolve();s.sync();bool finite=true,distinct=true;std::size_t peak=0,bullets=0;
 // Stress a reachable six-slot III build with uncapped research for 60 active minutes.
 for(unsigned tick=0;tick<60*3600;++tick){while(g.snapshot().phase==Phase::choosing_upgrade){std::set<unsigned>ids;for(const auto&c:g.snapshot().choices){ids.insert(static_cast<unsigned>(c.research));finite=finite&&std::isfinite(c.gain)&&std::isfinite(c.before)&&std::isfinite(c.after);}distinct=distinct&&ids.size()==3;(void)g.choose_upgrade(draft(g.snapshot(),2));}g.advance(1.F/60,steer(g.snapshot(),2));const auto&v=g.snapshot();finite=finite&&std::isfinite(v.player.x)&&std::isfinite(v.player.z)&&std::isfinite(v.weapon_damage)&&std::isfinite(v.hull);peak=std::max(peak,v.entities.size());bullets=std::max(bullets,v.projectiles.size());}
 require(finite&&distinct,"hour-long saturated six-slot build has finite stats and distinct nonexhausted draft cards");
 std::cout<<"STRESS seconds="<<g.snapshot().stats.active_seconds<<" level="<<g.snapshot().stats.level<<" weapon_damage="<<g.snapshot().weapon_damage<<" peak_entities="<<peak<<" peak_projectiles="<<bullets<<" invulnerability_fixture=true\n";
}
void capture_fairness(){Game g;g.start();auto&s=*g.state_;s.actors.clear();s.bullets.clear();s.view.beams.clear();detail::Actor a;a.view.id=900;a.view.kind=EntityKind::cow;a.view.position={0,0};a.view.hull=a.view.maximum_hull=1;s.actors.push_back(a);s.capture(.5F);const float progress=s.actors[0].view.capture_progress;s.view.player.x=10;s.capture(.02F);s.view.player.x=0;s.capture(.01F);require(progress>0&&s.actors[0].view.capture_progress<progress,"leaving base capture radius resets progress rather than retaining free progress");
 s.view.phase=Phase::paused;const auto events=s.view.presentation_events;g.advance(100,{{1,1},true});require(std::equal(s.view.presentation_events.begin(),s.view.presentation_events.end(),events.begin(),[](const auto&a,const auto&b){return a.sequence==b.sequence;}),"paused simulation emits no new outcome events");}
void tactical_ablation(){
 for(unsigned variant=0;variant<3;++variant){std::vector<double>times;std::uint64_t captures=0;
  for(unsigned seed=21;seed<=32;++seed){Game g;g.start({.seed=seed});
   for(unsigned tick=0;tick<60*1200&&g.snapshot().phase!=Phase::dead;++tick){while(g.snapshot().phase==Phase::choosing_upgrade)(void)g.choose_upgrade(draft(g.snapshot(),1));
    // Counterfactual diagnostic only: no reward for retired threats, no stat boost.
    if(variant==1)for(auto&a:g.state_->actors)if(a.view.kind==EntityKind::boss)a.removed=true;
    if(variant==2)for(auto&a:g.state_->actors)if(a.view.kind==EntityKind::tank)a.removed=true;
    g.advance(1.F/60,steer(g.snapshot(),1));
   }times.push_back(g.snapshot().stats.active_seconds);captures+=g.snapshot().stats.captures;
  }std::sort(times.begin(),times.end());std::cout<<"ABLATION fresh_collector variant="<<variant<<" (0=production,1=retire_bosses,2=retire_tanks) median_seconds="<<(times[5]+times[6])/2<<" min="<<times.front()<<" max="<<times.back()<<" mean_captures="<<captures/12<<'\n';
 }
}
void journeys(){
 namespace p=human_resources::progression;
 for(unsigned policy=1;policy<=3;++policy)for(unsigned account=0;account<4;++account){
  progression_tests::MemoryBackend backend;p::AccountSession session(backend);(void)session.load();unsigned firstbuy=0,allunlocked=0,cap=0,firstunlock=0;double firstchoice=-1;std::uint64_t totalearned=0;bool okay=true;
  for(unsigned attempt=1;attempt<=30;++attempt){okay=okay&&session.begin_run().succeeded();Game g;g.start({.seed=1000+account*100+attempt,.permanent_ranks=session.account().ranks,.unlocked_blueprints=session.account().unlocked_blueprints});
   for(unsigned tick=0;tick<60*1200&&g.snapshot().phase!=Phase::dead;++tick){while(g.snapshot().phase==Phase::choosing_upgrade){if(attempt==1&&firstchoice<0)firstchoice=g.snapshot().stats.active_seconds;(void)g.choose_upgrade(draft(g.snapshot(),policy));}g.advance(1.F/60,steer(g.snapshot(),policy));}
   const auto&v=g.snapshot().stats;p::RunReceipt r{session.account().checkpoint.run_id,static_cast<std::uint64_t>(v.active_seconds*1000),v.score,v.captures,v.soldier_kills,v.jeep_kills,v.tank_kills,v.vehicle_captures,v.boss_kills};okay=okay&&session.settle(r).succeeded();totalearned+=session.account().results.base_credits;
   if(!firstunlock&&session.account().results.newly_unlocked)firstunlock=attempt;
   if(!allunlocked&&session.account().unlocked_blueprints==65535U)allunlocked=attempt;
   std::array<unsigned,4> order=policy==1?std::array<unsigned,4>{3,0,2,1}:policy==2?std::array<unsigned,4>{0,2,1,3}:std::array<unsigned,4>{1,2,0,3};
   bool bought;do{bought=false;for(auto track:order)if(session.store().hangar()[track].affordable){okay=okay&&session.purchase(static_cast<p::Track>(track)).succeeded();bought=true;if(!firstbuy)firstbuy=attempt;break;}}while(bought);
   if(!cap&&std::all_of(session.account().ranks.begin(),session.account().ranks.end(),[](auto rank){return rank==5;}))cap=attempt;
   p::AccountSession reopen(backend);okay=okay&&reopen.load().playable()&&p::encode_account(reopen.account())==p::encode_account(session.account());
  }
  std::cout<<"JOURNEY policy="<<policy<<" account="<<account<<" attempts=30 first_choice_seconds="<<firstchoice<<" first_purchase="<<firstbuy<<" first_unlock="<<firstunlock<<" all_unlocked="<<allunlocked<<" capped="<<cap<<" total_base_credits="<<totalearned<<" unlocked="<<std::popcount(session.account().unlocked_blueprints)<<" ranks=";for(auto rank:session.account().ranks)std::cout<<static_cast<unsigned>(rank);std::cout<<" saved_reopen="<<okay<<'\n';require(okay,"thirty-attempt native account journey banks purchases and reloads each attempt");
 }
}
int main(){saturated_and_boundary();capture_fairness();paced_runs();tactical_ablation();journeys();std::cout<<"FAILURES "<<failures<<'\n';return failures?1:0;}
