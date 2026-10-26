#include "run_state.h"
#include "test_support.h"
#include <iostream>

namespace gp = human_resources::gameplay;
namespace tune = gp::tuning;
using test_support::require;
using test_support::require_near;
namespace {
constexpr float dt = 1.F / 60;
float distance(gp::Vec2 a, gp::Vec2 b) {
  return std::hypot(a.x - b.x, a.z - b.z);
}
gp::detail::Actor actor(gp::EntityKind kind, unsigned id, gp::Vec2 position) {
  gp::detail::Actor a;
  a.view.id = id;
  a.view.kind = kind;
  a.view.position = position;
  a.view.hull = a.view.maximum_hull = 90;
  if (kind == gp::EntityKind::soldier)
    a.view.infantry_role = gp::InfantryRole::rifleman;
  a.attack_timer = gp::detail::vehicle(kind)?tune::enemy_attack_period[gp::detail::military_index(kind)]*.5F:100;
  if (gp::detail::vehicle(kind)) a.locomotion_velocity={0,-1};
  return a;
}
void bounded_approach_and_planted_sources() {
  for (auto moving : {gp::EntityKind::soldier, gp::EntityKind::jeep, gp::EntityKind::tank, gp::EntityKind::boss}) {
    for (auto fixed : {gp::EntityKind::soldier, gp::EntityKind::jeep, gp::EntityKind::tank, gp::EntityKind::boss}) {
      gp::Game::State s;
      s.view.player = {0, 0};
      // Existing boss-ingress quiet window isolates approach without falsely
      // encoding an enormous timer as the vehicle's planted shot recovery.
      s.view.stats.active_seconds=tune::director_boss_first_second-.5;
      const float gap = tune::military_body_radius[static_cast<unsigned>(moving) - 4] +
                        tune::military_body_radius[static_cast<unsigned>(fixed) - 4] + tune::military_spacing_margin;
      s.actors.push_back(actor(fixed, 1, {0, 9}));
      s.actors.push_back(actor(moving, 3, {0, 9 + gap + 1}));
      s.actors[0].view.disabled_seconds = 100;
      const auto original = s.actors[0].view.position;
      bool moved = false;
      for (unsigned frame = 0; frame < 240; ++frame) {
        auto& a = s.actors[1];
        const auto before = a.view.position;
        const auto wanted = gp::detail::scaled(gp::detail::direction({0 - before.x, 4 - before.z}), tune::military_movement_speed[static_cast<unsigned>(moving) - 4]);
        if(gp::detail::vehicle(moving)) s.enemies(dt); // Real facing/translation commit before weapons.
        else {
          const auto velocity = s.military_velocity(a, wanted, dt);
          a.view.velocity = velocity;
          a.view.position = gp::detail::add(a.view.position, gp::detail::scaled(velocity, dt));
        }
        moved = moved || distance(a.view.position, before) > .001F;
        require(distance(a.view.position, original) + .0001F >= gap, "every military body pair keeps its measured approach envelope");
        require(distance(a.view.position, before) <= tune::military_movement_speed[static_cast<unsigned>(moving) - 4] * dt + .00001F,
            "avoidance changes ordinary bounded velocity, never teleports or pushes a root");
        require(std::abs(a.view.position.x) <= tune::field_half_width - tune::military_body_radius[static_cast<unsigned>(moving) - 4] + .0001F,
            "accepted military body motion stays within the whole-body field edge");
      }
      if(!(moved && distance(s.actors[1].view.position, original)>gap))std::cerr<<"fixed-pair moving="<<static_cast<unsigned>(moving)<<" fixed="<<static_cast<unsigned>(fixed)<<" moved="<<moved<<" final="<<s.actors[1].view.position.x<<','<<s.actors[1].view.position.z<<" gap="<<gap<<'\n';
      require(moved && distance(s.actors[1].view.position, original) > gap, "approaching military can route around a fixed body without hiding it");
      require(distance(s.actors[0].view.position, original) == 0, "local approach never changes the obstacle root");
    }
  }
  std::cout << "PASS all sixteen military body pairs route around fixed bodies with bounded motion\n";
}
void locked_aim_and_recovery() {
  for (auto kind : {gp::EntityKind::soldier, gp::EntityKind::jeep, gp::EntityKind::tank, gp::EntityKind::boss}) {
    gp::Game::State state;
    state.view.player = {0, 0};
    auto source = actor(kind, 1, {3, 9});
    source.view.warning_seconds = .4F;
    source.aim = source.view.warning_aim = {0, 0};
    source.locomotion_velocity = {0, -1};
    state.actors.push_back(source);
    state.actors.push_back(actor(gp::EntityKind::jeep, 3, {3, 16}));
    if(kind==gp::EntityKind::boss) {
      state.boss.emplace();state.boss->actor_id=source.view.id;
      state.boss->plan=gp::boss_attack_plan(source.view,source.aim,{},gp::BossAttackKind::fan);
      require(state.boss->plan.has_value(),"spacing boss fixture has a source-exact committed plan");
      state.boss->phase=gp::BossPhase::warning;state.boss->next_kind=gp::BossAttackKind::fan;
      state.boss->total_ticks=75;state.boss->remaining_ticks=24;
      state.boss->body_heading=state.boss->plan->locked_body_heading;
    }
    const auto original = source.view.position;
    for (unsigned frame = 0; frame < 20; ++frame) {
      state.enemies(dt);
      const auto& actual = state.actors[0];
      require(distance(actual.view.position, original) == 0 && distance(actual.view.warning_aim, source.aim) == 0,
          "spacing cannot displace a live committed source or retarget its warning");
      require(distance(actual.view.velocity, {}) == 0, "committed source remains visibly planted");
    }
  }
  std::cout << "PASS spacing leaves all committed military roots and warning aims exact\n";
}
void capture_projection_and_edge_return() {
  gp::Game::State state;
  state.actors.push_back(actor(gp::EntityKind::soldier, 1, {0, 9}));
  state.actors.push_back(actor(gp::EntityKind::jeep, 3, {0, 14}));
  state.actors[0].lifted = true;
  state.actors[0].view.capture_progress = .5F;
  for (unsigned frame = 0; frame < 180; ++frame) {
    if (frame == 90) {
      state.actors[0].lifted = false;
      state.actors[0].view.capture_progress = 0;
    }
    state.enemies(dt);
    const auto& moving = state.actors[1];
    require(distance(moving.view.position, state.actors[0].view.position) >= tune::military_body_radius[0] + tune::military_body_radius[1],
        "capture reserves its ground projection so cancellation never returns into another body");
  }
  auto released = actor(gp::EntityKind::soldier, 5, {7.3F, 9});
  state.actors = {released};
  for (unsigned frame = 0; frame < 60; ++frame) {
    auto& a = state.actors[0];
    const auto velocity = state.military_velocity(a, {-3.2F, 0}, dt);
    a.view.position = gp::detail::add(a.view.position, gp::detail::scaled(velocity, dt));
  }
  require(state.actors[0].view.position.x < tune::field_half_width - tune::military_body_radius[0],
      "a soldier released beyond the whole-body field edge returns through normal movement");
  std::cout << "PASS capture projection reservation and bounded inward return after edge release\n";
}
float swept_distance(gp::Vec2 point,gp::Vec2 from,gp::Vec2 to) {
  const gp::Vec2 travel{to.x-from.x,to.z-from.z},offset{point.x-from.x,point.z-from.z};
  const float square=gp::detail::length_squared(travel);
  const float t=square>0?std::clamp((offset.x*travel.x+offset.z*travel.z)/square,0.F,1.F):0;
  return distance(point,gp::detail::add(from,gp::detail::scaled(travel,t)));
}
void physical_reverse_escape_and_reopened_cage() {
  unsigned failures=0;
  unsigned physical_failures=0;
  for(const auto kind:{gp::EntityKind::jeep,gp::EntityKind::tank}) {
    for(unsigned fixture=0;fixture<6;++fixture) {
      gp::Game::State state;state.view.player={0,0};state.view.hull=state.view.maximum_hull=100;
      state.view.stats.active_seconds=tune::director_boss_first_second-.5;
      const float own=tune::military_body_radius[gp::detail::military_index(kind)],edge=tune::field_half_width-own;
      auto moving=actor(kind,3,{fixture==0||fixture==3?edge:-2.F,14});
      moving.locomotion_velocity={1,0};moving.view.body_heading=1.57079632679F;
      if(fixture==5)moving.view.position={0,14};
      state.actors.push_back(moving);
      auto fixed=[&](unsigned id,gp::EntityKind body,gp::Vec2 position) {
        auto a=actor(body,id,position);a.view.disabled_seconds=100;state.actors.push_back(a);
      };
      const float gap=own+tune::military_body_radius[0]+tune::military_spacing_margin;
      if(fixture==1||fixture==2||fixture==4)fixed(10,gp::EntityKind::soldier,{-2+gap,14});
      if(fixture==2)fixed(11,gp::EntityKind::soldier,{-3.2F,10.2F});
      if(fixture==3)fixed(10,gp::EntityKind::soldier,{edge-gap,12});
      if(fixture==4) {fixed(11,gp::EntityKind::soldier,{-5.5F,10});fixed(12,gp::EntityKind::soldier,{-5.5F,18});}
      if(fixture==5)for(unsigned side=0;side<4;++side) {
        const float angle=static_cast<float>(side)*1.57079632679F;
        fixed(10+side,gp::EntityKind::soldier,{gap*std::cos(angle),14+gap*std::sin(angle)});
      }
      unsigned forward_ticks=0,reverse_ticks=0,longest_stop=0,stopped=0,reverse_run=0,longest_reverse=0,side_flips=0;
      float previous_turn=0;
      float maximum_turn=0,minimum_margin=100;
      bool physical_reported=false;
      const auto start=state.actors[0].view.position;
      bool escaped=false;
      for(unsigned frame=0;frame<600;++frame) {
        if(fixture==5 && frame==60) {state.actors[2].removed=true;state.actors[3].removed=true;}
        const auto before=state.actors[0];state.enemies(dt);const auto& a=state.actors[0];
        require(!a.removed,"finite escape never succeeds by retirement or deleting the moving body");
        const auto facing=gp::detail::direction(a.locomotion_velocity);
        const float speed=std::hypot(a.view.velocity.x,a.view.velocity.z);
        const float longitudinal=a.view.velocity.x*facing.x+a.view.velocity.z*facing.z;
        const float turn=std::abs(std::remainder(std::atan2(facing.x,-facing.z)-std::atan2(before.locomotion_velocity.x,-before.locomotion_velocity.z),6.28318530718F));
        const float signed_turn=std::remainder(std::atan2(facing.x,-facing.z)-std::atan2(before.locomotion_velocity.x,-before.locomotion_velocity.z),6.28318530718F);
        if(std::abs(signed_turn)>.0001F && std::abs(previous_turn)>.0001F && signed_turn*previous_turn<0)++side_flips;
        previous_turn=signed_turn;
        maximum_turn=std::max(maximum_turn,turn);
        require(turn<=tune::vehicle_turn_radians_per_second*dt+.00001F,"physical ordinary chassis obeys authoritative per-tick angular limit");
        require(turn<=speed*dt/tune::vehicle_minimum_turn_radius_metres+.00001F,"physical back-turn respects its minimum turning radius at actual signed speed");
        require(std::abs(a.view.velocity.x*facing.z-a.view.velocity.z*facing.x)<.00002F,"actual translation is forward or reverse along physical facing, never sideways");
        const auto old_facing=gp::detail::direction(before.locomotion_velocity);
        const float old_longitudinal=before.view.velocity.x*old_facing.x+before.view.velocity.z*old_facing.z;
        require(longitudinal*old_longitudinal>=-.00001F,"reverse-to-forward physical gear change passes through a complete stop");
        // These fixtures contain static known geometry; reopening only removes
        // obstacles. Emergency stops for newly crossing bodies are separate.
        const float old_speed=std::abs(old_longitudinal),change=speed-old_speed;
        if(!(change<=tune::vehicle_acceleration*dt+.0001F && -change<=tune::vehicle_braking*dt+.0001F)) {
          ++physical_failures;
          if(!physical_reported)std::cerr<<"static-motion kind="<<static_cast<unsigned>(kind)<<" fixture="<<fixture<<" frame="<<frame<<" signed_speed="<<old_longitudinal<<"->"<<longitudinal<<" before="<<before.view.position.x<<','<<before.view.position.z<<" after="<<a.view.position.x<<','<<a.view.position.z<<" face_before="<<before.locomotion_velocity.x<<','<<before.locomotion_velocity.z<<" face_after="<<a.locomotion_velocity.x<<','<<a.locomotion_velocity.z<<" phase="<<static_cast<unsigned>(before.escape.phase)<<"->"<<static_cast<unsigned>(a.escape.phase)<<" turn="<<before.escape.turn_remaining<<"->"<<a.escape.turn_remaining<<" exit="<<before.escape.forward_remaining<<"->"<<a.escape.forward_remaining<<'\n';
          physical_reported=true;
        }
        require(std::abs(a.view.position.x)<=edge+.0001F,"reverse escape preserves whole-body field boundary");
        if(speed<.0001F) {++stopped;longest_stop=std::max(longest_stop,stopped);require(turn<.00001F,"stopped chassis never rotates cosmetically");}
        else {stopped=0;if(longitudinal<0){++reverse_ticks;++reverse_run;longest_reverse=std::max(longest_reverse,reverse_run);}else {++forward_ticks;reverse_run=0;}}
        for(std::size_t i=1;i<state.actors.size();++i)if(!state.actors[i].removed) {
          const auto& other=state.actors[i];
          const float margin=swept_distance(other.view.position,before.view.position,a.view.position)-own-tune::military_body_radius[gp::detail::military_index(other.view.kind)]-tune::military_spacing_margin;
          minimum_margin=std::min(minimum_margin,margin);
          require(margin>=-.0002F,"complete actual reverse/forward step preserves reserved swept body gap");
        }
        if(fixture==5 && frame<60)require(distance(a.view.position,start)<.0001F,"closed cage holds honestly until an actual opening appears");
        const bool beyond=fixture==0?a.view.position.x<edge-2.F:
            fixture==3?(a.view.position.x<edge-1.F && a.view.position.z<11.F):
            fixture==5?(a.view.position.x>1.5F && a.view.position.z<11.F):
            (a.view.position.x>-2+gap+gap*.5F && a.view.position.z<12.5F);
        if((fixture!=5||frame>=60) && longitudinal>.1F && beyond) {escaped=true;break;}
      }
      const auto& final=state.actors[0];
      std::cout<<"escape kind="<<static_cast<unsigned>(kind)<<" fixture="<<fixture<<" forward="<<forward_ticks<<" reverse="<<reverse_ticks<<" max_reverse="<<longest_reverse<<" max_stop="<<longest_stop<<" side_flips="<<side_flips<<" max_turn="<<maximum_turn<<" min_margin="<<minimum_margin<<" escaped="<<escaped<<" final="<<final.view.position.x<<','<<final.view.position.z<<" facing="<<final.locomotion_velocity.x<<','<<final.locomotion_velocity.z<<'\n';
      if(!escaped)++failures;
      if(fixture==0)require(reverse_ticks>0,"head-on field edge requires real backing before forward escape");
    }
  }
  require(failures==0,"each available physical maneuver reaches finite forward progress beyond its initial obstruction");
  require(physical_failures==0,"known static escape geometry preserves bounded acceleration and braking");
}
void moving_vehicle_crossings_keep_both_bodies_and_progress() {
  for(bool crossing:{false,true}) {
    gp::Game::State s;s.view.player={0,0};s.view.hull=s.view.maximum_hull=100;
    s.view.stats.active_seconds=tune::director_boss_first_second-.5;
    auto first=actor(gp::EntityKind::jeep,3,{-4,14});first.locomotion_velocity={1,0};
    auto second=actor(gp::EntityKind::tank,4,crossing?gp::Vec2{2,19}:gp::Vec2{4,14});second.locomotion_velocity=crossing?gp::Vec2{0,-1}:gp::Vec2{-1,0};
    s.actors={first,second};float minimum=100;
    const float gap=tune::military_body_radius[1]+tune::military_body_radius[2]+tune::military_spacing_margin;
    for(unsigned frame=0;frame<600;++frame) {
      const auto a=s.actors[0].view.position,b=s.actors[1].view.position;s.enemies(dt);
      const auto after_a=s.actors[0].view.position,after_b=s.actors[1].view.position;
      const float swept=swept_distance({}, {a.x-b.x,a.z-b.z}, {after_a.x-after_b.x,after_a.z-after_b.z});
      minimum=std::min(minimum,swept-gap);
      require(swept>=gap-.0002F,"opposing and crossing actual vehicle sweeps preserve both measured envelopes");
      require(!s.actors[0].removed && !s.actors[1].removed,"moving-neighbor escape cannot delete or retire pressure");
    }
    std::cout<<"moving-neighbor crossing="<<crossing<<" min_margin="<<minimum<<" first="<<s.actors[0].view.position.x<<','<<s.actors[0].view.position.z<<" second="<<s.actors[1].view.position.x<<','<<s.actors[1].view.position.z<<'\n';
    require(s.actors[0].view.position.x>0 && s.actors[0].view.position.z<12 && s.actors[1].view.position.x<0 && s.actors[1].view.position.z<13,"both opposing/crossing vehicles make finite forward progress toward their actual flank stations");
  }
}
void ingress_and_random_stream() {
  for (unsigned seed = 1; seed <= 40; ++seed) {
    gp::Game::State state, control;
    state.random_state = control.random_state = seed;
    for (unsigned i = 0; i < 27; ++i) {
      const auto kind = i < 18 ? gp::EntityKind::soldier : i < 23 ? gp::EntityKind::jeep
                                                       : i < 26   ? gp::EntityKind::tank
                                                                  : gp::EntityKind::boss;
      state.spawn(kind);
      for (unsigned sample = 0; sample < 4; ++sample)
        (void)control.random_unit();
    }
    require(state.actors.size() == 27, "bounded clearance keeps every authored maximum-population arrival");
    require(state.random_state == control.random_state && state.view.director_spawn_attempts == 27,
        "military placement consumes no additional encounter random samples or spawn attempts");
    for (std::size_t i = 0; i < state.actors.size(); ++i)
      for (std::size_t j = i + 1; j < state.actors.size(); ++j) {
        const auto& a = state.actors[i];
        const auto& b = state.actors[j];
        const float gap = tune::military_body_radius[static_cast<unsigned>(a.view.kind) - 4] + tune::military_body_radius[static_cast<unsigned>(b.view.kind) - 4];
        require(distance(a.view.position, b.view.position) + .0001F >= gap,
            "even a maximum-density authored ingress cannot start military bodies overlapped");
      }
  }
  std::cout << "PASS maximum-cap ingress retains actors and encounter RNG without overlapping bodies\n";
}
void public_reproduction_and_restore() {
  for (unsigned seed : {1U, 514U, 7841U}) {
    gp::Game run;
    run.start({.seed = seed});
    bool checkpointed = false;
    std::vector<gp::EntityView> previous;
    for (unsigned frame = 0; frame < 60 * 150 && run.snapshot().phase != gp::Phase::dead; ++frame) {
      if (run.snapshot().phase == gp::Phase::choosing_upgrade)
        require(run.choose_upgrade(0), "ordinary earned choice resumes the public spacing reproduction");
      // This checkpoint fixture tests recovery of a live crowded encounter.
      // Seed 7841's idle pilot naturally dies before 32 seconds; use ordinary
      // semantic flight and ready Dash while preserving the same seed/time.
      gp::Control control{};
      if (seed == 7841U) {
        control.movement = {static_cast<float>(std::sin(run.snapshot().stats.active_seconds * .8)), 1};
        control.dash = run.snapshot().dash_cooldown_seconds <= 0;
      }
      run.advance(dt, control);
      const auto& s = run.snapshot();
      for(const auto& a:s.entities)if(gp::detail::vehicle(a.kind) && a.capture_progress==0) {
        const auto before=std::find_if(previous.begin(),previous.end(),[&](const auto& old){return old.id==a.id && old.capture_progress==0;});
        if(before==previous.end())continue;
        const float turn=std::abs(std::remainder(gp::vehicle_body_yaw(a)-gp::vehicle_body_yaw(*before),6.28318530718F));
        require(turn<=tune::vehicle_turn_radians_per_second*dt+.00005F,"actual public Game ordinary body headings stay within the physical per-tick turn bound");
        const float yaw=gp::vehicle_body_yaw(a);
        require(std::abs(a.velocity.x*std::cos(yaw)+a.velocity.z*std::sin(yaw))<.0001F,"actual public Game motion remains longitudinal in the published body frame");
      }
      for (std::size_t i = 0; i < s.entities.size(); ++i)
        for (std::size_t j = i + 1; j < s.entities.size(); ++j) {
          const auto& a = s.entities[i];
          const auto& b = s.entities[j];
          if (a.kind < gp::EntityKind::soldier || b.kind < gp::EntityKind::soldier || a.capture_progress > 0 || b.capture_progress > 0)
            continue;
          const float gap = tune::military_body_radius[static_cast<unsigned>(a.kind) - 4] + tune::military_body_radius[static_cast<unsigned>(b.kind) - 4];
          require(distance(a.position, b.position) + .001F >= gap, "fresh public Game no longer collapses military body roots into shared flank targets");
        }
      if (!checkpointed && s.stats.active_seconds > 32) {
        checkpointed = true;
        run.pause();
        const auto bytes = run.save_state();
        run.advance(10, {{1, 1}, true});
        require(run.save_state() == bytes, "paused approaches preserve exact saved roots and clocks");
        gp::Game copy;
        require(copy.restore_state(bytes) == gp::SaveStateIssue::none && copy.save_state() == bytes, "actual crowded run round trips byte exactly");
        run.resume();
        copy.resume();
        for (unsigned future = 0; future < 240; ++future) {
          if (run.snapshot().phase == gp::Phase::choosing_upgrade)
            require(run.choose_upgrade(0), "original future accepts earned choice");
          if (copy.snapshot().phase == gp::Phase::choosing_upgrade)
            require(copy.choose_upgrade(0), "restored future accepts earned choice");
          gp::Control c{{std::sin(static_cast<float>(future) * .04F), .1F}, future % 137 == 0};
          run.advance(dt, c);
          copy.advance(dt, c);
          require(run.save_state() == copy.save_state(), "body avoidance preserves exact future after pause, cold restore and Dash");
        }
      }
      previous=run.snapshot().entities;
    }
    if(!checkpointed)std::cerr<<"public spacing seed="<<seed<<" phase="<<static_cast<unsigned>(run.snapshot().phase)<<" active="<<run.snapshot().stats.active_seconds<<" captures="<<run.snapshot().stats.captures<<'\n';
    require(checkpointed, "real public reproduction reaches actual crowded military encounters");
  }
  std::cout << "PASS fresh public spacing reproduction and exact paused/restored futures\n";
}
} // namespace
int main() {
  bounded_approach_and_planted_sources();
  locked_aim_and_recovery();
  capture_projection_and_edge_return();
  ingress_and_random_stream();
  physical_reverse_escape_and_reopened_cage();
  moving_vehicle_crossings_keep_both_bodies_and_progress();
  public_reproduction_and_restore();
}
