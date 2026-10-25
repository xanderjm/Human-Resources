#include "run_state.h"
#include "test_support.h"
#include <chrono>
#include <iostream>

namespace game = human_resources::gameplay;
namespace tune = game::tuning;
using test_support::require;
using test_support::require_near;
namespace {
constexpr float dt = 1.F / 60;
unsigned checks{};
void pass(const char* label) { ++checks; std::cout << "PASS " << label << '\n'; }
game::detail::Actor actor(unsigned id, game::EntityKind kind, game::Vec2 p = {}, game::Vec2 velocity = {}) {
  game::detail::Actor a;
  a.view.id = id; a.view.kind = kind; a.view.position = p;
  a.view.velocity = a.wander_velocity = velocity;
  a.view.hull = a.view.maximum_hull = 1;
  return a;
}
float distance(game::Vec2 a, game::Vec2 b) { return std::hypot(a.x-b.x,a.z-b.z); }
bool equal(game::Vec2 a, game::Vec2 b) { return a.x == b.x && a.z == b.z; }
void coincident() {
  game::Game::State s;
  s.actors = {actor(1,game::EntityKind::cow),actor(2,game::EntityKind::cow)};
  const float radius = .436F;
  for (unsigned i=0;i<60;++i) {
    const auto before=s.actors;
    s.move_biological(dt);
    for(unsigned j=0;j<2;++j) {
      const auto& a=s.actors[j];
      require(std::isfinite(a.view.position.x)&&std::isfinite(a.view.position.z),"coincident positions stay finite");
      require(distance(a.view.position,before[j].view.position)<=.6001F*dt,"coincident correction obeys .6m/s budget");
      require_near(a.view.velocity.x,(a.view.position.x-before[j].view.position.x)/dt,.0001F,"reported velocity matches actual X displacement");
      require_near(a.view.velocity.z,(a.view.position.z-before[j].view.position.z)/dt,.0001F,"reported velocity matches actual Z displacement");
    }
  }
  require(distance(s.actors[0].view.position,s.actors[1].view.position)>.7F*radius*2,"exact coincidence resolves visibly without teleport");
  require(equal(s.actors[0].wander_velocity,{}),"separation does not contaminate private wander velocity");
  pass("exact coincidence finite, smoothly separated, truthful velocity, wander preserved");
}
void boundaries() {
  for (float side : {-1.F,1.F}) {
    game::Game::State s;
    s.actors={actor(1,game::EntityKind::pig,{side*tune::field_half_width,0},{side*.6F,0}),
      actor(2,game::EntityKind::cow,{side*(tune::field_half_width-.436F),0})};
    for(unsigned i=0;i<180;++i) {
      const auto before=s.actors;
      s.move_biological(dt);
      for(unsigned j=0;j<s.actors.size();++j) {
        const auto& a=s.actors[j];
        require(std::fabs(a.view.position.x)<=tune::field_half_width+.0001F,"moving roots respect accepted field edges");
        const float budget=std::hypot(before[j].wander_velocity.x,before[j].wander_velocity.z)+.6F;
        require(distance(a.view.position,before[j].view.position)<=budget*dt+.0001F,"legitimate edge roots never teleport to a new body inset");
        require(std::isfinite(a.view.position.z),"edge correction stays finite");
      }
    }
  }
  pass("both field edges constrain roots without changing accepted movement bounds or teleporting");
}
void exclusions() {
  for(unsigned mode=0;mode<3;++mode) {
    game::Game::State s;
    auto excluded=actor(2,game::EntityKind::cow);
    if(mode==0)excluded.lifted=true;
    if(mode==1)excluded.view.capture_progress=.001F;
    if(mode==2)excluded.removed=true;
    s.actors={actor(1,game::EntityKind::cow,{},{.1F,.2F}),excluded};
    s.move_biological(dt);
    require_near(s.actors[0].view.position.x,.1F*dt,.00001F,"excluded neighbour never pushes free target X");
    require_near(s.actors[0].view.position.z,.2F*dt,.00001F,"excluded neighbour never pushes free target Z");
    require(equal(s.actors[1].view.position,excluded.view.position),"capturing/lifted/removed body never moves");
  }
  pass("lifted, partial capture and removed actors excluded as movers and neighbours");
}
void military_unchanged() {
  game::Game::State s;
  s.actors={actor(1,game::EntityKind::cow)};
  for(unsigned k=4;k<8;++k) {
    auto a=actor(k,static_cast<game::EntityKind>(k),{}, {.3F,.4F});
    a.attack_timer=3.7F; a.age=1.25F; a.aim={4,5};a.view.warning_aim={2,3};
    a.view.warning_seconds=.7F;a.slow_timer=.2F;a.focus_timer=.3F;
    s.actors.push_back(a);
  }
  const auto before=s.actors;
  s.move_biological(dt);
  for(unsigned i=1;i<s.actors.size();++i) {
    const auto& a=s.actors[i];const auto& b=before[i];
    require(equal(a.view.position,b.view.position)&&equal(a.view.velocity,b.view.velocity),"military position and velocity unchanged");
    require(equal(a.aim,b.aim)&&equal(a.view.warning_aim,b.view.warning_aim),"military committed aim unchanged");
    require(a.attack_timer==b.attack_timer&&a.age==b.age&&a.view.warning_seconds==b.view.warning_seconds&&a.slow_timer==b.slow_timer&&a.focus_timer==b.focus_timer,"military timers unchanged");
  }
  require(distance(s.actors[0].view.position,{})>0,"soldier is a stationary obstacle to passive movement");
  pass("soldier avoidance preserves all military movement, aim and timers");
}
void spawn_contract() {
  game::Game::State s;
  auto existing=actor(1,game::EntityKind::cow,{0,10});s.actors={existing};
  auto incoming=actor(2,game::EntityKind::cow);
  const auto rng=s.random_state;const auto attempts=s.view.director_spawn_attempts;const auto hash=s.view.director_schedule_hash;
  require(s.place_biological(incoming,.5F,0.F,false),"sparse spawn candidate can be placed");
  require(s.random_state==rng&&s.view.director_spawn_attempts==attempts&&s.view.director_schedule_hash==hash,"candidate placement has no random/schedule side effects");
  require(std::fabs(incoming.view.position.x)<=tune::field_half_width-.436F+.0001F,"spawn core is within field width");
  for(unsigned seed=1;seed<=100;++seed) {
    game::Game run;run.start({.seed=seed});
    const auto& entities=run.snapshot().entities;
    auto radius=[](game::EntityKind k){return std::array<float,4>{.237F,.436F,.255F,.221F}[static_cast<unsigned>(k)];};
    for(unsigned i=0;i<entities.size();++i) {
      const auto& a=entities[i];
      require(std::fabs(a.position.x)<=tune::field_half_width-radius(a.kind)+.0001F,"initial target core-safe at lateral bound");
      for(unsigned j=i+1;j<entities.size();++j)
        require(distance(a.position,entities[j].position)+.0001F>=radius(a.kind)+radius(entities[j].kind),"initial spawn roots avoid core overlap");
    }
  }
  pass("100 seeded initial fields have core-safe bounded roots; placement does not consume RNG");
  game::Game::State a,b;
  a.next_id=b.next_id=100;a.random_state=b.random_state=341;
  b.actors={actor(1,game::EntityKind::cow,{0,10}),actor(2,game::EntityKind::soldier,{0,13})};
  a.spawn(game::EntityKind::cow);b.spawn(game::EntityKind::cow);
  require(a.random_state==b.random_state&&a.view.director_spawn_attempts==b.view.director_spawn_attempts&&a.view.director_schedule_hash==b.view.director_schedule_hash,"neighbour-dependent placement preserves encounter random stream and schedule hash");
  pass("different populations preserve spawn random samples, attempts and schedule hash");
}
void paused_and_schedules() {
  game::Game run;run.start();run.pause();const auto snapshot=run.snapshot();
  run.advance(100,{{1,1},true});
  require(run.snapshot().entities.size()==snapshot.entities.size(),"pause retains population");
  for(unsigned i=0;i<snapshot.entities.size();++i)
    require(equal(snapshot.entities[i].position,run.snapshot().entities[i].position)&&equal(snapshot.entities[i].velocity,run.snapshot().entities[i].velocity),"pause freezes all body positions and velocities");
  require(run.snapshot().stats.active_seconds==snapshot.stats.active_seconds&&run.snapshot().director_schedule_hash==snapshot.director_schedule_hash,"pause freezes encounter clock/hash");
  pass("public pause freezes separation with the rest of gameplay");
  for(unsigned seed=1;seed<=8;++seed) {
    game::Game a,b;a.start({.seed=seed});b.start({.seed=seed,.permanent_ranks={5,5,5,5},.unlocked_blueprints=0xffff});
    unsigned steps{};
    for(;steps<60*60;++steps) {
      if(a.snapshot().phase==game::Phase::choosing_upgrade)require(a.choose_upgrade(0),"first paired draft accepted");
      if(b.snapshot().phase==game::Phase::choosing_upgrade)require(b.choose_upgrade(2),"second paired draft accepted");
      if(a.snapshot().phase!=game::Phase::active||b.snapshot().phase!=game::Phase::active)break;
      a.advance(dt,{{-1,0},false});b.advance(dt,{{1,1},false});
      require(a.snapshot().director_spawn_attempts==b.snapshot().director_spawn_attempts&&a.snapshot().director_schedule_hash==b.snapshot().director_schedule_hash,"public paired encounter schedules independent of account, movement, drafts and body contacts");
    }
    require(steps>=60*20,"paired public trace reaches meaningful 20-second horizon");
  }
  pass("eight public paired schedule prefixes remain identical");
}
void microbench() {
  game::Game::State s;
  for(unsigned i=0;i<28;++i)s.actors.push_back(actor(i+1,static_cast<game::EntityKind>(i%4),{(static_cast<float>(i%7)-3)*.7F,static_cast<float>(i/7)*.7F},{.1F,.1F}));
  for(unsigned i=0;i<18;++i)s.actors.push_back(actor(i+29,game::EntityKind::soldier,{(static_cast<float>(i%6)-2.5F)*.6F,static_cast<float>(i/6)*.8F}));
  const auto started=std::chrono::steady_clock::now();
  for(unsigned i=0;i<20000;++i)s.move_biological(dt);
  const auto micros=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-started).count()/20000;
  std::cout<<"BENCH 28 passive + 18 soldier obstacles: "<<micros<<" us/helper step (optimized host only)\n";
}
}
int main() {
  coincident();boundaries();exclusions();military_unchanged();spawn_contract();paused_and_schedules();microbench();
  std::cout<<"RESULT "<<checks<<" groups passed, zero failures\n";
}
