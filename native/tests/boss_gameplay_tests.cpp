#include "run_state.h"
#include "test_support.h"
#include <iostream>
#include <numbers>
#include <set>

namespace gp=human_resources::gameplay;
namespace tune=gp::tuning;
using test_support::require;
using test_support::require_near;
namespace {
constexpr float dt=1.F/60;
// Isolated authority fixtures deliberately suppress the director budgets and
// extend boss health. Public Game::advance still owns movement, timing, aiming,
// collision and releases; these fixtures are not natural balance/save evidence.
gp::Game isolated(gp::Vec2 player={},bool fast=false) {
  gp::Game game;game.start({.seed=714});auto& s=*game.state_;
  // Begin after authored jeep/tank introductions; negative budgets alone do
  // not suppress those one-off arrivals. Crowded admission is tested separately.
  s.actors.clear();s.view.beams.clear();s.view.stats.active_seconds=60;
  s.view.stats.captures=fast?10000:0;s.next_boss=1e9;
  s.biological_budget=s.soldier_budget=s.jeep_budget=s.tank_budget=-1000;
  s.view.player=player;s.view.runner_distance=0;s.view.invulnerability_seconds=1000;
  s.sync();s.view.velocity={0,s.view.forward_speed};
  gp::detail::Actor boss;boss.view.id=s.next_id++;boss.view.kind=gp::EntityKind::boss;
  boss.view.position={player.x>0?-3.5F:3.5F,10};boss.view.hull=boss.view.maximum_hull=1000000;
  boss.view.body_heading=0;s.actors.push_back(boss);s.boss_update(s.actors.front(),0);s.sync();return game;
}
void advance(gp::Game& game,gp::Control control={}) {
  if(game.snapshot().phase==gp::Phase::choosing_upgrade)require(game.choose_upgrade(0),"isolated earned upgrade resolves");
  game.advance(dt,control);
}
void commit_rake(gp::Game& game) {
  auto& s=*game.state_;auto& actor=s.actors.front();
  const float station=s.view.player.z-s.view.runner_distance;
  actor.view.position={std::clamp(s.view.player.x+(s.view.player.x>0?-3.5F:3.5F),-5.F,5.F),
      s.view.runner_distance+std::clamp(station+(station>6?-10.F:10.F),0.F,12.F)};
  actor.view.velocity={};actor.view.body_heading=0;
  auto& boss=*s.boss;boss.next_kind=gp::BossAttackKind::rake;boss.body_heading=0;
  boss.plan=gp::boss_attack_plan(actor.view,s.view.player,s.view.velocity,boss.next_kind);
  require(boss.plan.has_value(),"controlled rake has physical source solutions");
  boss.phase=gp::BossPhase::warning;boss.remaining_ticks=boss.total_ticks=81;boss.next_release=0;
  actor.view.warning_seconds=tune::boss_rake_warning_seconds;s.sync();
}
void stored_source_and_phase_schedule() {
  auto game=isolated();commit_rake(game);auto& s=*game.state_;
  const auto plan=*s.boss->plan;const auto root=s.actors.front().view.position;
  const auto id=s.actors.front().view.id;std::set<std::uint32_t> seen;unsigned releases=0;
  for(unsigned frame=1;frame<=117;++frame) {
    advance(game,{{-.8F,0},false});
    for(const auto& p:s.bullets)if(p.source==id&&seen.insert(p.view.id).second) {
      const auto& shot=plan.shells[releases++];
      require_near(p.view.launch_position.x,shot.shot.origin.x,1e-6F,"rake X source remains committed despite player dodge");
      require_near(p.view.launch_position.z,shot.shot.origin.z,1e-6F,"rake Z source remains committed despite player dodge");
      require_near(p.view.velocity.x,shot.trajectory.x*tune::projectile_speed[3],1e-6F,"released velocity consumes stored trajectory");
      require_near(p.view.launch_direction.x,shot.shot.direction.x,1e-6F,"physical launch axis remains canonical");
      require(frame==81+(releases-1)*18,"rake releases on exact 60Hz 0/.30/.60 schedule");
    }
    require_near(s.actors.front().view.position.x,root.x,0,"warning and salvo chassis X planted");
    require_near(s.actors.front().view.position.z,root.z,0,"warning and salvo chassis Z planted");
  }
  require(releases==3&&s.boss->phase==gp::BossPhase::recovery&&s.boss->next_kind==gp::BossAttackKind::fan,"complete rake uses three shells and advances exactly once");
  advance(game);require(gp::detail::length_squared(s.actors.front().view.velocity)>0,"weapon recovery allows real chassis reposition immediately");
}
void repeated_full_flight_cycles() {
  unsigned cases=0;
  for(bool fast:{false,true})for(const auto player:{gp::Vec2{0,0},gp::Vec2{0,-17},gp::Vec2{0,16.5F},gp::Vec2{-7.5F,-17},gp::Vec2{7.5F,16.5F}}) {
    auto game=isolated(player,fast);auto& s=*game.state_;unsigned fan=0,rake=0;auto previous=gp::BossPhase::approach;
    float heading=s.boss->body_heading;
    for(unsigned frame=0;frame<5400;++frame) {
      advance(game);require(s.boss.has_value(),"boss remains in legal full-flight arena during repeated cycles");
      const auto& boss=*s.boss;
      const float turn=std::abs(std::remainder(boss.body_heading-heading,2*std::numbers::pi_v<float>));heading=boss.body_heading;
      require(turn<=tune::boss_body_turn_radians_per_second*dt+.00001F,"real chassis turns within bounded angular speed");
      if(boss.phase==gp::BossPhase::warning&&previous!=boss.phase) {
        if(boss.plan->kind==gp::BossAttackKind::fan)++fan;else ++rake;
        require(s.valid_boss(),"actual committed phase satisfies durable boss contract");
      }
      previous=boss.phase;
    }
    require(fan>=8&&rake>=8,"centre, forward, backward and side camping all expose repeated fan/rake cycles");++cases;
    std::cout<<"BOSS cycle fast="<<fast<<" player="<<player.x<<','<<player.z<<" fan="<<fan<<" rake="<<rake<<'\n';
  }
  require(cases==10,"bounded actual-advance station cases exercised");
}
void real_tank_introduction_does_not_trap_boss() {
  auto game=isolated({},true);auto& s=*game.state_;s.view.stats.active_seconds=20;
  unsigned rake=0,fan=0,switches=0;bool tank_seen=false;auto previous=gp::BossPhase::approach;bool flank=s.boss->flank_right;
  for(unsigned frame=0;frame<5400;++frame) {
    advance(game);require(s.boss.has_value(),"real ordinary tank ingress cannot permanently trap and retire max-scroll boss");
    tank_seen=tank_seen||std::any_of(s.actors.begin(),s.actors.end(),[](const auto& a){return a.view.kind==gp::EntityKind::tank;});
    if(s.boss->flank_right!=flank){++switches;flank=s.boss->flank_right;}
    if(s.boss->phase==gp::BossPhase::warning&&previous!=s.boss->phase) {
      if(s.boss->plan->kind==gp::BossAttackKind::rake)++rake;else ++fan;
    }
    previous=s.boss->phase;
  }
  require(tank_seen&&rake>=6&&fan>=6,"crowded actual introduction retains repeated boss combat");
  require(switches>0&&switches<8,"blocked flank changes persist without per-tick lane oscillation");
  std::cout<<"BOSS actual tank ingress fan="<<fan<<" rake="<<rake<<" flank_switches="<<switches<<'\n';
}
void quiet_window_budget_and_cancellation() {
  auto game=isolated();auto& s=*game.state_;
  gp::detail::Actor soldier;soldier.view.id=s.next_id++;soldier.view.kind=gp::EntityKind::soldier;
  soldier.view.infantry_role=gp::InfantryRole::rifleman;soldier.view.position={-4,8};
  soldier.view.hull=soldier.view.maximum_hull=1000000;soldier.view.warning_seconds=.3F;soldier.aim=s.view.player;
  s.actors.insert(s.actors.begin(),soldier);
  bool committed=false;
  for(unsigned frame=0;frame<180;++frame) {
    advance(game);
    if(s.boss_leading()) {
      committed=true;
      for(const auto& actor:s.actors)if(actor.view.kind!=gp::EntityKind::boss)require(actor.view.warning_seconds==0,"older warnings drain before leading boss attack");
    }
  }
  require(committed,"ordinary warning churn cannot starve a ready boss");
  auto blocked=isolated();auto& b=*blocked.state_;b.bullets.resize(tune::projectile_active_cap-2);
  require(!b.boss_preparing(),"ineligible full projectile budget does not quiesce ordinary AI");
  auto cancelled=isolated();commit_rake(cancelled);auto& c=*cancelled.state_;
  for(unsigned frame=0;frame<82;++frame)advance(cancelled);
  require(c.boss->next_release==1,"cancellation fixture has one released shell");
  c.actors.front().view.disabled_seconds=1;advance(cancelled);
  require(c.boss->next_release==0&&!c.boss->plan&&c.boss->next_kind==gp::BossAttackKind::rake,"disable cancels unreleased remainder without advancing pattern");
  require(!c.bullets.empty(),"already released shell survives source cancellation");
  auto durable=isolated();commit_rake(durable);auto& d=*durable.state_;
  d.biological_budget=d.soldier_budget=d.jeep_budget=d.tank_budget=0;
  for(unsigned frame=0;frame<117;++frame)advance(durable);
  require(d.boss->phase==gp::BossPhase::recovery&&!durable.save_state().empty(),"actual recovery cache forms a valid continuation");
  d.actors.front().view.disabled_seconds=1;advance(durable);
  const auto bytes=durable.save_state();gp::Game cold;
  require(d.actors.front().view.fire_recovery_seconds==0&&!bytes.empty()&&cold.restore_state(bytes)==gp::SaveStateIssue::none,
      "disable during recovery clears its derived cache and cold-restores");
  auto impossible=isolated();commit_rake(impossible);auto& q=*impossible.state_;
  soldier.view.id=q.next_id++;q.actors.push_back(soldier);
  require(!q.valid_boss(),"impossible concurrent ordinary warning is rejected during a planted plan");
}
void ordinary_steering_defaults() {
  for(const auto kind:{gp::EntityKind::soldier,gp::EntityKind::jeep,gp::EntityKind::tank}) {
    auto game=isolated();auto& s=*game.state_;auto actor=s.actors.front();actor.view.kind=kind;actor.view.position={0,4};actor.view.velocity={1,-1};
    s.actors={actor};s.boss.reset();
    for(const auto desired:{gp::Vec2{},gp::Vec2{2,-1},gp::Vec2{-3,1}}) {
      const auto legacy=s.military_velocity(s.actors.front(),desired,dt);
      const auto explicit_zero=s.military_velocity(s.actors.front(),desired,dt,{});
      require(legacy.x==explicit_zero.x&&legacy.z==explicit_zero.z,"ordinary enemies retain zero-carry steering contract");
    }
  }
}
void dodge_paths_use_real_movement_and_collision() {
  unsigned cases=0,escapes=0;
  for(const auto player:{gp::Vec2{0,0},gp::Vec2{-7.5F,-17},gp::Vec2{7.5F,16.5F}})for(bool fast:{false,true}) {
    bool escaped=false;
    for(unsigned path=0;path<8;++path) {
      auto game=isolated(player,fast);commit_rake(game);auto& s=*game.state_;s.view.invulnerability_seconds=0;
      const float shield=s.view.shield,hull=s.view.hull;
      const float angle=static_cast<float>(path)*std::numbers::pi_v<float>/4;
      for(unsigned frame=0;frame<240;++frame) {
        // 200ms reaction, then 85% base speed: conservative capture-speed dodge.
        advance(game,{{frame<12?0:.85F*std::cos(angle),frame<12?0:.85F*std::sin(angle)},false});
        if(s.boss->phase==gp::BossPhase::recovery)s.actors.front().attack_timer=100;
      }
      if(s.view.shield==shield&&s.view.hull==hull){escaped=true;break;}
    }
    require(escaped,"readable rake has a no-Dash escape under actual acceleration and arena clamps");++cases;escapes+=escaped;
  }
  std::cout<<"BOSS real movement escape cases="<<escapes<<'/'<<cases<<'\n';
}
void stationary_and_dash_controls() {
  for(const auto kind:{gp::BossAttackKind::fan,gp::BossAttackKind::rake}) {
    auto game=isolated();commit_rake(game);auto& s=*game.state_;
    if(kind==gp::BossAttackKind::fan) {
      s.boss->next_kind=kind;s.boss->plan=gp::boss_attack_plan(s.actors.front().view,s.view.player,s.view.velocity,kind);
      s.boss->remaining_ticks=s.boss->total_ticks=75;s.actors.front().view.warning_seconds=tune::boss_fan_warning_seconds;
    }
    s.view.invulnerability_seconds=0;const float before=s.view.hull+s.view.shield;
    for(unsigned frame=0;frame<240;++frame)advance(game);
    require(s.view.hull+s.view.shield<before,"stationary pilot is actually hit by committed fan and rake controls");
  }
  auto dashed=isolated();commit_rake(dashed);auto& s=*dashed.state_;s.view.invulnerability_seconds=0;
  const float before=s.view.hull+s.view.shield;
  for(unsigned frame=0;frame<240;++frame)advance(dashed,{{},frame==105});
  require(s.view.hull+s.view.shield==before&&s.view.dash_cooldown_seconds>0,
      "timed starter Dash avoids the same stationary rake that otherwise hits");
}
}
int main() {
  stored_source_and_phase_schedule();quiet_window_budget_and_cancellation();ordinary_steering_defaults();
  dodge_paths_use_real_movement_and_collision();stationary_and_dash_controls();repeated_full_flight_cycles();real_tank_introduction_does_not_trap_boss();
  std::cout<<"Boss source, phase, quiet-window, cancellation, movement and cycle contracts passed\n";
}
