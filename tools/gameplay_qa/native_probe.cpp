// Independent native QA fixtures. Access relaxation is confined to this test build;
// execution uses the production C++ gameplay methods, never the Python simulator.
#include "run_state.h"
#include <human_resources/progression/progression.h>
#include "progression_test_backend.h"
#include <iostream>
#include <set>
#include <string>

using namespace human_resources::gameplay;
namespace p = human_resources::progression;
using detail::Actor;
int failures{};
void check(bool value, const std::string& label) {
  std::cout << (value ? "PASS " : "FAIL ") << label << '\n';
  if (!value) ++failures;
}
void fixture(Game& game) {
  game.start({.unlocked_blueprints=65535});
  game.state_->actors.clear(); game.state_->bullets.clear();
  game.state_->view.beams.clear(); game.state_->sync();
}
void module(Game::State& s, Module m, unsigned rank) {
  s.view.modules[static_cast<std::size_t>(m)]=static_cast<std::uint8_t>(rank);
  s.sync();
}
Actor target(EntityKind kind, unsigned id, Vec2 position={0,1}, float hp=10000) {
  Actor a; a.view.id=id; a.view.kind=kind; a.view.position=position;
  if(kind==EntityKind::soldier)a.view.infantry_role=InfantryRole::rifleman;
  a.view.hull=a.view.maximum_hull=hp; a.attack_timer=100;
  return a;
}
void vehicle_cooldown() {
  Game g; fixture(g); auto& s=*g.state_;
  module(s,Module::beam_splitter,3); module(s,Module::tractor_lens,3);
  module(s,Module::heavy_winch,3); s.view.evolution_bits=1U<<static_cast<unsigned>(Evolution::great_vacuum); s.sync();
  for(unsigned id=1;id<=3;++id) {
    auto a=target(EntityKind::jeep,id,{static_cast<float>(id)*0.1F,1});
    a.view.hull=100; a.view.capture_progress=0.9999F; s.actors.push_back(a);
  }
  s.capture(1.0F/60);
  check(s.view.stats.vehicle_captures==1,"one vehicle completion per eight seconds; observed="+std::to_string(s.view.stats.vehicle_captures));
}
void dash_edge() {
  Game g; fixture(g); module(*g.state_,Module::phase_drive,1);
  g.advance(1.0F/120,{.dash=true}); g.advance(1.0F/120,{});
  check(g.snapshot().dash_cooldown_seconds>0,"dash press survives a render frame with no fixed step");
}
void portrait_bounds() {
  const float half_width=tuning::field_half_width;
  Game g;g.start({.seed=9});
  for(unsigned tick=0;tick<300;++tick) {
    if(g.snapshot().phase==Phase::choosing_upgrade)(void)g.choose_upgrade(0);
    g.advance(1.0F/60,{{1,0},false});
  }
  bool bounded=std::abs(g.snapshot().player.x-half_width)<0.001F;
  g.start({.seed=9});
  for(unsigned tick=0;tick<300;++tick) {
    if(g.snapshot().phase==Phase::choosing_upgrade)(void)g.choose_upgrade(0);
    g.advance(1.0F/60,{{-1,0},false});
  }
  bounded=bounded && std::abs(g.snapshot().player.x+half_width)<0.001F;
  module(*g.state_,Module::phase_drive,3);g.advance(1.0F/60,{{-1,0},true});
  check(bounded && g.snapshot().player.x>=-half_width,"portrait candidate reaches both configured field edges and clamps outward dash");
}
void reward_exclusion() {
  Game g; fixture(g); auto& s=*g.state_;
  s.actors.push_back(target(EntityKind::soldier,1));
  s.captured(s.actors[0]); s.hit(s.actors[0],100000);
  check(s.view.stats.captures==1 && s.view.stats.soldier_kills==0 && s.view.xp==1,"captured soldier never also earns kill XP/counter");
  fixture(g); module(s,Module::beam_splitter,1); module(s,Module::heavy_winch,1);
  s.actors.push_back(target(EntityKind::jeep,1)); s.captured(s.actors[0]); s.hit(s.actors[0],100000);
  check(s.view.stats.vehicle_captures==1 && s.view.stats.jeep_kills==0 && s.view.stats.captures==0 && s.view.xp==2,"captured vehicle has one reward and no kill/biological counter");
  fixture(g); s.actors.push_back(target(EntityKind::soldier,1,{},1));
  s.hit(s.actors[0],10); s.hit(s.actors[0],10);
  check(s.view.stats.soldier_kills==1 && s.view.xp==0.5F,"repeated lethal hits settle once");
}
void drafts_and_rank_effects() {
  Game g; fixture(g); auto& s=*g.state_;
  bool all_ranks=true;
  for(unsigned m=0;m<static_cast<unsigned>(Module::count);++m) {
    fixture(g);
    for(unsigned rank=1;rank<=3;++rank) {
      auto choice=s.make_choice(true,m,tuning::module_rarity[m]);
      s.apply(choice); s.sync();
      all_ranks=all_ranks && s.view.modules[m]==rank;
    }
  }
  check(all_ranks,"all sixteen production module cards install and reach III");
  fixture(g);
  for(unsigned r=0;r<7;++r) {
    const auto old=s.view.research[r]; s.view.hull=50;
    const auto c=s.make_choice(false,r,0); s.apply(c); s.sync();
    check(r==6 ? s.view.hull>50 : s.view.research[r]>old,"research family "+std::to_string(r)+" applies a real effect");
  }
  fixture(g); bool increments=true;
  for(unsigned rank=1;rank<=3;++rank) {
    s.apply(s.make_choice(true,static_cast<unsigned>(Module::beam_splitter),0));s.sync();
    increments=increments && s.view.beam_capacity==rank+1;
  }
  check(increments,"beam picks add exactly one: 1 to 2 to 3 to 4");
  fixture(g); bool tail=true;
  for(Module m:{Module::laser_array,Module::rail_lens,Module::orbit_drones,Module::emp_ring,Module::beam_splitter,Module::tractor_lens}) module(s,m,3);
  s.view.research[4]=100;s.view.research[5]=50;s.view.hull=s.view.maximum_hull;
  for(unsigned n=0;n<10000;++n) {
    s.picks=n;s.draw_choices();std::set<unsigned> seen;
    for(const auto& c:s.view.choices) {
      tail=tail && !c.hardware && static_cast<unsigned>(c.research)<4;
      seen.insert(static_cast<unsigned>(c.research));
    }
    tail=tail && seen.size()==3;
  }
  check(tail,"ten thousand exhausted-hardware drafts retain three distinct eligible research options");
  bool recipes=true;
  for(unsigned e=0;e<6;++e) {
    fixture(g);
    for(auto ingredient:tuning::evolution_ingredients[e])s.view.modules[ingredient]=3;
    s.evolve();s.sync();recipes=recipes && (s.view.evolution_bits&(1U<<e));
  }
  check(recipes,"all six native recipes trigger from their actual ingredient ranks");
  fixture(g); for(auto& m:s.view.modules)m=3;s.evolve();s.sync();
  check(s.view.stats.evolutions==2 && s.view.evolution_bits==3,"first two evolutions cap and catalogue tie order");
}
void combat_effects() {
  Game g; fixture(g);auto& s=*g.state_;bool laser_effect=true;
  for(unsigned rank=0;rank<=2;++rank) {
    fixture(g);module(s,Module::laser_array,rank);s.actors.push_back(target(EntityKind::boss,1));
    s.laser_fire();laser_effect=laser_effect && std::abs((10000-s.actors[0].view.hull)-(8+4*rank))<0.01F;
  }
  check(laser_effect,"added lasers actually deal half original damage");
  fixture(g);module(s,Module::rail_lens,3);
  for(unsigned id=1;id<=4;++id)s.actors.push_back(target(EntityKind::soldier,id,{0,static_cast<float>(id)*2}));
  s.laser_fire();check(std::abs(s.actors[3].view.hull-9996.8F)<0.01F,"Rail Lens III damages third aligned secondary target");
  fixture(g);module(s,Module::orbit_drones,3);s.actors.push_back(target(EntityKind::boss,1));s.drone_fire();
  check(std::abs(s.actors[0].view.hull-9994)<0.01F,"three drones deal six damage per volley");
  fixture(g);module(s,Module::emp_ring,3);s.actors.push_back(target(EntityKind::boss,1,{0,4.8F}));s.pulse();
  check(std::abs(s.actors[0].view.hull-9978)<0.01F,"EMP III deals twenty-two damage inside five metres");
  fixture(g);module(s,Module::heavy_winch,3);s.actors.push_back(target(EntityKind::tank,1));s.actors[0].view.hull=3500;
  check(s.capture_eligible(s.actors[0]) && s.capture_seconds(s.actors[0])>=2,"Heavy Winch III accepts 35-percent tank with two-second minimum");
  fixture(g);module(s,Module::shield_generator,3);module(s,Module::mirror_plating,3);
  s.view.evolution_bits=1U<<static_cast<unsigned>(Evolution::mirrorball);s.reflected_damage=40;s.actors.push_back(target(EntityKind::boss,1));s.combat(1.0F/60);
  check(s.actors[0].view.hull<9961,"Mirrorball executes stored reflected damage");
}
void conditional_effects() {
  Game g;fixture(g);auto& s=*g.state_;
  module(s,Module::beam_splitter,2);module(s,Module::overtime_reactor,3);module(s,Module::reserve_battery,3);
  s.view.beams={{1,{},0,true}};
  check(std::abs(s.fire_multiplier()-1.1F)<0.001F && std::abs(s.damage_multiplier()-1.16F)<0.001F,"vehicle counts as one active beam and consumes two idle slots");
  fixture(g);module(s,Module::repair_swarm,3);s.view.hull=50;s.view.shield=0;
  for(unsigned i=0;i<12;++i){auto a=target(EntityKind::cow,i+1);s.captured(a);}
  check(s.view.hull==54 && s.view.shield==4,"Repair Swarm III heals four per twelve captures and caps same-second shield at four");
  fixture(g);module(s,Module::bounty_scanner,3);
  for(unsigned i=0;i<10;++i){auto a=target(EntityKind::cow,i+1);s.captured(a);}
  check(s.score>65 && s.view.xp==10,"Bounty score and streak increase score without XP inflation");
  fixture(g);module(s,Module::tractor_lens,3);
  auto a=target(EntityKind::cow,1,{5.4F,0});s.actors.push_back(a);s.capture(1);
  check(s.view.capture_radius==5.2F && s.actors[0].view.position.x<5.4F && s.actors[0].view.position.x>=5.262F,"Tractor Lens III expands radius and pulls within target movement cap");
  fixture(g);module(s,Module::vector_thrusters,3);s.move(1.0F/60,{{1,0},false});
  check(std::abs(s.view.movement_speed-7.08F)<0.001F,"Vector Thrusters III changes actual movement speed");
  fixture(g);module(s,Module::phase_drive,3);s.move(1.0F/60,{{1,0},true});
  check(s.view.dash_cooldown_seconds==10 && std::abs(s.view.invulnerability_seconds-0.3F)<0.001F && s.view.player.x>3,"Phase Drive III performs three-metre dash with correct cooldown/protection");
  fixture(g);module(s,Module::arc_coil,3);s.actors.push_back(target(EntityKind::boss,1,{0,2},1000000));
  for(unsigned i=2;i<=4;++i)s.actors.push_back(target(EntityKind::soldier,i,{static_cast<float>(i-2),3},1000000));
  for(unsigned trial=0;trial<1000;++trial)s.secondary_hits(s.actors[0],10);
  check(s.actors[1].view.hull<1000000 && s.actors[2].view.hull==s.actors[1].view.hull && s.actors[3].view.hull==s.actors[1].view.hull,"Arc Coil III actually damages three distinct nearby targets");
  fixture(g);module(s,Module::mirror_plating,3);s.actors.push_back(target(EntityKind::soldier,1,{},1000000));
  unsigned reflected=0;
  for(unsigned trial=0;trial<1000;++trial) {
    s.view.hull=100;s.view.shield=20;s.view.invulnerability_seconds=0;
    detail::Projectile b;b.source=1;b.view.damage=10;b.view.radius=0.1F;s.bullets={b};
    const auto old=s.actors[0].view.hull;s.projectiles(0);reflected+=s.actors[0].view.hull<old;
  }
  check(reflected>100 && reflected<200,"Mirror Plating III actually returns original projectile damage at seeded fifteen-percent chance");
  fixture(g);s.view.evolution_bits=1U<<static_cast<unsigned>(Evolution::employee_benefits);s.view.shield=20;
  detail::Projectile b;b.view.damage=10;b.view.radius=0.1F;s.bullets={b};s.projectiles(0);
  check(s.view.shield==17 && s.view.hull==100 && s.intercept_cooldown==6,"Employee Benefits intercept spends three shield and starts squad cooldown");
  fixture(g);module(s,Module::orbit_drones,1);s.view.evolution_bits=1U<<static_cast<unsigned>(Evolution::employee_benefits);
  auto bio=target(EntityKind::cow,5);s.captured(bio);s.actors.push_back(target(EntityKind::boss,1));s.drone_fire();
  check(std::abs(s.actors[0].view.hull-9997.6F)<0.01F,"Employee Benefits capture burst increases native drone damage twenty percent");
  fixture(g);module(s,Module::emp_ring,3);s.view.evolution_bits=1U<<static_cast<unsigned>(Evolution::storm_drain);
  s.actors.push_back(target(EntityKind::boss,1,{0,5.8F}));s.pulse();
  check(s.actors[0].view.hull==9978 && s.actors[0].slow_timer==1,"Storm Drain reaches six metres and applies real slow timer");
  fixture(g);module(s,Module::heavy_winch,3);s.view.evolution_bits=1U<<static_cast<unsigned>(Evolution::hostile_acquisition);
  auto jeep=target(EntityKind::jeep,1);auto tank=target(EntityKind::tank,2);tank.view.hull=5500;
  check(s.capture_eligible(jeep) && s.capture_eligible(tank),"Hostile Acquisition accepts full jeep and fifty-five-percent tank");
  fixture(g);s.view.evolution_bits=1U<<static_cast<unsigned>(Evolution::office_shredder);s.actors.push_back(target(EntityKind::boss,1));s.laser_fire();
  check(std::abs(s.actors[0].view.hull-9990.4F)<0.01F,"Office Shredder increases actual boss laser damage twenty percent");
}
void director_fairness() {
  Game a,b;fixture(a);fixture(b);a.state_->view.stats.active_seconds=b.state_->view.stats.active_seconds=180;
  b.state_->setup.permanent_ranks={5,5,5,5};b.state_->sync();
  a.state_->director(60);b.state_->director(60);
  bool same=a.state_->actors.size()==b.state_->actors.size();
  for(std::size_t i=0;same && i<a.state_->actors.size();++i) {
    const auto& x=a.state_->actors[i].view;const auto& y=b.state_->actors[i].view;
    same=x.kind==y.kind && x.maximum_hull==y.maximum_hull && x.position.x==y.position.x && x.position.z==y.position.z;
  }
  check(same,"director does not match enemies to purchased account power");
  auto before=a.snapshot().stats.active_seconds;a.pause();a.advance(30);a.resume();
  check(a.snapshot().stats.active_seconds==before,"pause time does not increase encounter clock");
  Game gates;fixture(gates);auto& s=*gates.state_;
  s.view.stats.active_seconds=29.99;s.director(1);
  bool exact=s.jeep_budget==0 && s.tank_budget==0;
  s.view.stats.active_seconds=30;s.director(1);exact=exact && s.jeep_budget>0 && s.tank_budget==0;
  s.view.stats.active_seconds=99.99;s.director(1);exact=exact && s.tank_budget==0;
  s.view.stats.active_seconds=100;s.director(1);exact=exact && s.tank_budget>0;
  check(exact && tuning::enemy_start_second[1]==30 && tuning::enemy_start_second[2]==100,"jeep/tank spawn budgets begin at accepted thirty/one-hundred active seconds");
}
void upgrade_boundaries() {
  Game g;fixture(g);auto& s=*g.state_;
  s.view.xp=100;s.check_level();unsigned picks=0;
  while(g.snapshot().phase==Phase::choosing_upgrade && picks<10){(void)g.choose_upgrade(0);++picks;}
  check(picks==4 && s.view.stats.level==5 && s.view.xp==6,"XP overflow processes four separate earned levels without active time");
  fixture(g);s.picks=15;
  const auto bundled=s.make_choice(false,static_cast<unsigned>(Research::weapon_damage),0);
  check(bundled.gain==6,"sixteenth choice doubles uncapped Common research gain");
  s.view.research[4]=99;const auto clipped=s.make_choice(false,4,2);s.apply(clipped);
  check(clipped.gain==1 && s.view.research[4]==100 && !s.research_eligible(4),"near-cap research grants remaining one point and disappears");
  fixture(g);const auto winch=static_cast<unsigned>(Module::heavy_winch);
  bool requirements=!s.module_eligible(winch);module(s,Module::beam_splitter,2);requirements=requirements && !s.module_eligible(winch);
  module(s,Module::laser_array,1);requirements=requirements && s.module_eligible(winch);
  check(requirements,"Heavy Winch uses Archero override of Splitter II plus Laser I");
  fixture(g);s.setup.permanent_ranks={5,5,5,5};s.sync();
  check(s.view.maximum_hull==130 && s.view.maximum_shield==40 && s.view.weapon_damage==10 && s.view.beam_capacity==1 && s.view.laser_count==1,"maxed Hangar uses selected caps without granting extra beams/lasers");
  fixture(g);module(s,Module::beam_splitter,2);module(s,Module::tractor_lens,3);
  const auto completing=s.make_choice(true,static_cast<unsigned>(Module::beam_splitter),0);s.apply(completing);s.sync();
  check(completing.after==static_cast<float>(s.view.beam_capacity),"evolution-completing beam card reports actual resulting capacity; preview="+std::to_string(completing.after)+" actual="+std::to_string(s.view.beam_capacity));
  fixture(g);module(s,Module::beam_splitter,3);module(s,Module::heavy_winch,2);
  const auto winch_completing=s.make_choice(true,static_cast<unsigned>(Module::heavy_winch),1);s.apply(winch_completing);s.sync();
  check(winch_completing.after==100,"Hostile Acquisition completing card reports actual full-health jeep threshold; preview="+std::to_string(winch_completing.after));
}
void paired_schedules() {
  bool same=true;
  for(unsigned seed=1;seed<=16;++seed) {
    Game a,b;a.start({.seed=seed});b.start({.seed=seed,.permanent_ranks={5,5,5,5},.unlocked_blueprints=65535});
    // Prevent death only, so five minutes of paired active scheduling is compared.
    // Opposite movement and different drafts/combat consume different non-world RNG.
    a.state_->view.invulnerability_seconds=b.state_->view.invulnerability_seconds=100000;
    for(unsigned tick=0;tick<60*300;++tick) {
      while(a.snapshot().phase==Phase::choosing_upgrade)(void)a.choose_upgrade(0);
      while(b.snapshot().phase==Phase::choosing_upgrade)(void)b.choose_upgrade(2);
      a.advance(1.0F/60,{{std::sin(static_cast<float>(tick)*0.01F),-1},false});
      b.advance(1.0F/60,{{std::cos(static_cast<float>(tick)*0.03F),1},false});
      same=same && a.snapshot().director_spawn_attempts==b.snapshot().director_spawn_attempts && a.snapshot().director_schedule_hash==b.snapshot().director_schedule_hash;
    }
  }
  check(same,"sixteen five-minute paired director schedules match across account power, movement, drafts and combat RNG");
}
void accounts() {
  progression_tests::MemoryBackend backend;p::AccountStore store(backend);(void)store.load();(void)store.begin_run();
  p::RunReceipt r{.run_id=store.account().checkpoint.run_id,.active_milliseconds=90000,.score=500};
  check(store.settle(r).succeeded() && store.account().credits==111,"native account settles exact credit formula");
  check(store.settle(r).issue==p::MutationIssue::duplicate,"native account rejects repeated settlement");
  check(store.credit_bonus(r.run_id,p::PlacementOutcome::completed).succeeded() && store.account().credits==166,"credit bonus adds floor(base/2)");
  check(store.credit_bonus(r.run_id,p::PlacementOutcome::completed).issue==p::MutationIssue::duplicate,"credit bonus cannot be replayed");
}
void public_runs() {
  for(unsigned policy=0;policy<3;++policy) {
    unsigned alive=0;double minimum=1e9;std::uint64_t maximum_score=0;float minimum_hull=100;
    for(unsigned seed=1;seed<=8;++seed) {
      Game g;g.start({.seed=seed});
      for(unsigned tick=0;tick<60*1200 && g.snapshot().phase!=Phase::dead;++tick) {
        if(g.snapshot().phase==Phase::choosing_upgrade){(void)g.choose_upgrade(0);continue;}
        Control c;if(policy==1)c.movement={0,1};if(policy==2)c.movement={0,-1};g.advance(1.0F/60,c);
      }
      alive+=g.snapshot().phase!=Phase::dead;
      minimum=std::min(minimum,g.snapshot().stats.active_seconds);maximum_score=std::max(maximum_score,g.snapshot().stats.score);
      minimum_hull=std::min(minimum_hull,g.snapshot().hull);
    }
    std::cout<<"OBS public policy="<<policy<<" alive_at_20m="<<alive<<"/8 minimum_active_seconds="<<minimum<<" max_score="<<maximum_score<<" minimum_hull="<<minimum_hull<<'\n';
  }
}
int main(){vehicle_cooldown();dash_edge();portrait_bounds();reward_exclusion();drafts_and_rank_effects();combat_effects();conditional_effects();director_fairness();upgrade_boundaries();paired_schedules();accounts();public_runs();std::cout<<"FAILURES "<<failures<<'\n';return failures?1:0;}
