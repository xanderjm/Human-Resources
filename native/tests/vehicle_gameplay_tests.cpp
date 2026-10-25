#include "run_state.h"
#include "test_support.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <iostream>
#include <limits>
namespace gp=human_resources::gameplay;
namespace tune=gp::tuning;
using test_support::require;
using test_support::require_near;
namespace {
constexpr float dt=1.F/60;
using Point=std::array<double,3>;
Point add(Point a,Point b) {return {a[0]+b[0],a[1]+b[1],a[2]+b[2]};}
Point yaw(Point p,double a) {return {std::cos(a)*p[0]+std::sin(a)*p[2],p[1],-std::sin(a)*p[0]+std::cos(a)*p[2]};}
Point up(Point p,double a) {return {p[0],std::cos(a)*p[1]+std::sin(a)*p[2],-std::sin(a)*p[1]+std::cos(a)*p[2]};}
Point source_muzzle(const gp::EntityView& actor,const gp::MinigunShot& shot,Point opening,double body_yaw) {
  // Independent forward assembly evaluation from the accepted source contract.
  // This uses the solved angles as inputs; it never repeats the inverse solver.
  const Point anchor{0,.4960820179858087,-1};
  const Point pivot{0,.8037945054981808,-.005136986301369864};
  const Point pitch{0,.19791782326894247,0};
  opening[2]+=.01;
  const auto local=add(add(anchor,pivot),yaw(add(pitch,up(opening,shot.elevation_radians)),shot.yaw_radians));
  const auto world=yaw(local,body_yaw);
  return {actor.position.x+world[0],world[1],actor.position.z-world[2]};
}
constexpr std::array<Point,6> source_openings{{
  {-.04823874755381602,.05738747553816047,.4274951076320939},
  {.07818003913894327,.014970645792563603,.4274951076320939},
  {.03243639921722116,.07568493150684932,.4274951076320939},
  {-.07318982387475535,-.017465753424657535,.4274951076320939},
  {.05489236790606655,-.058219178082191785,.4274951076320939},
  {-.02162426614481406,-.07318982387475537,.4274951076320939}}};
gp::detail::Actor jeep() {
  gp::detail::Actor a;
  a.view.id=1;a.view.kind=gp::EntityKind::jeep;a.view.position={0,9};a.view.velocity={0,-1};
  a.view.hull=4;a.view.maximum_hull=400;a.attack_timer=.001F;a.aim=a.view.warning_aim={0,-2};
  return a;
}
void source_geometry() {
  unsigned samples=0;
  for(unsigned body=0;body<24;++body) for(unsigned aim=0;aim<24;++aim) for(unsigned barrel=0;barrel<6;++barrel) {
    auto a=jeep();const float body_yaw=static_cast<float>(body)*6.283185307F/24;
    a.view.velocity={std::sin(body_yaw),-std::cos(body_yaw)};
    const float angle=static_cast<float>(aim)*6.283185307F/24;
    const gp::Vec2 target{std::sin(angle)*12,a.view.position.z+std::cos(angle)*12};
    const auto shot=gp::jeep_minigun_shot(a.view,target,static_cast<std::uint8_t>(barrel));
    require(shot.valid,"all sourcebarrels aim across full body/gun yaw at ordinary range");
    const float x=target.x-shot.origin.x,z=target.z-shot.origin.z,distance=std::hypot(x,z);
    require_near(x/distance,shot.direction.x,2e-6F,"true muzzle shares committed X trajectory");
    require_near(z/distance,shot.direction.z,2e-6F,"true muzzle shares committed Z trajectory");
    require_near(shot.height+std::tan(shot.elevation_radians)*distance,tune::projectile_target_height,2e-6F,
        "actual muzzle ray intersects target plane without visual pitch clamp");
    require(distance>=4.5F && shot.elevation_radians>=0 && shot.elevation_radians<=tune::minigun_maximum_elevation,"eligible source envelope exact");
    const auto actual=source_muzzle(a.view,shot,source_openings[barrel],body_yaw);
    require_near(shot.origin.x,static_cast<float>(actual[0]),2e-6F,"solved origin X equals independent source assembly");
    require_near(shot.height,static_cast<float>(actual[1]),2e-6F,"solved height equals independent source assembly");
    require_near(shot.origin.z,static_cast<float>(actual[2]),2e-6F,"solved origin Z equals independent source assembly");
    const auto axis=yaw(up({0,0,1},shot.elevation_radians),body_yaw+shot.yaw_radians);
    const double hit_distance=(tune::projectile_target_height-actual[1])/axis[1];
    require_near(static_cast<float>(actual[0]+axis[0]*hit_distance),target.x,1e-5F,"physical source axis reaches committed target X");
    require_near(static_cast<float>(actual[2]-axis[2]*hit_distance),target.z,1e-5F,"physical source axis reaches committed target Z");
    ++samples;
  }
  auto a=jeep();
  for(unsigned barrel=0;barrel<6;++barrel) {
    require(!gp::jeep_minigun_shot(a.view,a.view.position,static_cast<std::uint8_t>(barrel)).valid,"inside pivot cannot fire");
    require(!gp::jeep_minigun_shot(a.view,{0,7},static_cast<std::uint8_t>(barrel)).valid,"close-range gun cannot fake a steep shot");
  }
  require(!gp::jeep_minigun_shot(a.view,{0,-20},6).valid,"unknown barrel rejected");
  for(unsigned body=0;body<8;++body) for(unsigned barrel=0;barrel<6;++barrel) {
    const double body_yaw=body*6.283185307179586/8;
    a.view.velocity={static_cast<float>(std::sin(body_yaw)),static_cast<float>(-std::cos(body_yaw))};
    unsigned eligible{},ineligible{};
    for(unsigned radius=200;radius<=800;++radius) {
      const gp::Vec2 target{static_cast<float>(radius)*.01F,a.view.position.z};
      const auto shot=gp::jeep_minigun_shot(a.view,target,static_cast<std::uint8_t>(barrel));
      if(shot.valid) {
        ++eligible;
        require(std::hypot(target.x-shot.origin.x,target.z-shot.origin.z)>=4.5F,"threshold scan never accepts a muzzle inside minimum range");
        const auto actual=source_muzzle(a.view,shot,source_openings[barrel],body_yaw);
        require_near(shot.height,static_cast<float>(actual[1]),2e-6F,"near-envelope solve preserves actual source height");
      } else ++ineligible;
    }
    require(eligible>0 && ineligible>0,"each body/barrel threshold scan crosses its actual engagement boundary");
  }
  const float invalid=std::numeric_limits<float>::quiet_NaN();
  require(!gp::jeep_minigun_shot(a.view,{invalid,0},0).valid,"invalid target rejects rather than poisoning muzzle authority");
  a.view.velocity.x=invalid;require(!gp::jeep_minigun_shot(a.view,{0,-20},0).valid,"invalid body orientation rejects");
  a=jeep();a.view.kind=gp::EntityKind::tank;require(!gp::jeep_minigun_shot(a.view,{0,-20},0).valid,"other vehicles cannot impersonate physical minigun");
  std::cout<<"MINIGUN source rays="<<samples<<'\n';
}
void commitment_and_budget() {
  gp::Game::State s;s.actors={jeep()};s.next_id=2;s.view.player={0,0};
  const auto random=s.random_state;
  s.enemies(dt);
  auto& a=s.actors.front();
  require(a.view.warning_seconds==tune::attack_warning_seconds[1] && a.view.minigun_barrel==1,"accepted commitment selects one next physical barrel");
  const auto aim=a.aim;
  s.view.player={3,2};s.enemies(dt);
  require(a.aim.x==aim.x && a.aim.z==aim.z,"moving target cannot change committed point");
  a.view.warning_seconds=dt*.5F;
  s.enemies(dt);
  require(s.bullets.size()==1,"one warning remains one projectile, not a six-shot buff");
  const auto& p=s.bullets.front();
  const auto at_fire=gp::jeep_minigun_shot(a.view,aim,a.view.minigun_barrel);
  require(at_fire.valid,"moving firing-time geometry remains eligible");
  require_near(p.view.position.x,at_fire.origin.x,1e-6F,"actual firing uses current moving muzzle X");
  require_near(p.view.position.z,at_fire.origin.z,1e-6F,"actual firing uses current moving muzzle Z");
  require_near(p.view.launch_height,at_fire.height,1e-6F,"actual firing uses current moving muzzle height");
  require_near(p.view.launch_direction.x,at_fire.direction.x,1e-6F,"physical launch axis X is explicit authority");
  require_near(p.view.launch_direction.z,at_fire.direction.z,1e-6F,"physical launch axis Z is explicit authority");
  require(p.view.kind==gp::EnemyProjectileKind::jeep_minigun && p.view.muzzle_index==1 && p.source==1,"typed source and selected barrel persist");
  require_near(p.view.damage,tune::enemy_damage[1],1e-6F,"jeep damage unchanged");
  require_near(std::hypot(p.view.velocity.x,p.view.velocity.z),tune::projectile_speed[1],1e-6F,"jeep projectile speed unchanged");
  require(p.view.radius==tune::projectile_radius[1] && a.attack_timer==tune::enemy_attack_period[1],"abstract2D radius and cooldown unchanged");
  require(s.random_state==random,"gun selection and source solving consume no gameplay RNG");
  a.view.warning_seconds=.5F;a.view.position={0,0};a.aim={0,0};const auto ids=s.next_id;
  s.enemies(dt);
  require(a.view.warning_seconds==0 && s.next_id==ids && s.bullets.size()==1 && a.attack_timer==tune::enemy_attack_period[1],
      "moving jeep rechecks range and cleanly cancels before a false shot");
  a.view.warning_seconds=0;a.attack_timer=0;s.view.player=a.view.position;
  const auto before_rejection=s.next_presentation_sequence;const auto previous_barrel=a.view.minigun_barrel;s.enemies(dt);
  require(a.view.warning_seconds==0 && a.attack_timer==tune::enemy_attack_period[1],"close target retries ordinary cooldown without committing");
  require(a.view.minigun_barrel==previous_barrel && s.next_id==ids && s.next_presentation_sequence==before_rejection,"rejected close commitment consumes no barrel, ID or event");
}
void capture_readiness() {
  for(const auto kind:{gp::EntityKind::jeep,gp::EntityKind::tank}) {
  gp::Game::State s;s.actors={jeep()};s.view.player={0,0};s.view.capture_radius=4;s.view.beam_capacity=2;
  s.view.modules[static_cast<std::size_t>(gp::Module::heavy_winch)]=3;
  auto& a=s.actors.front();a.view.kind=kind;a.view.position={0,5};a.view.capture_progress=.9F;a.lifted=true;
  s.view.beams.push_back({1,a.view.position,.9F,true});s.capture(dt);
  require(a.view.capture_progress==0 && s.view.beams.empty() && a.view.fire_recovery_seconds==gp::capture_return_seconds(),
      "actual range loss retains immediate reset and starts shared bounded mount return");
  const auto before=a.view.position;s.enemies(dt);
  require(a.view.position.z!=before.z && a.view.warning_seconds==0,"return readiness suppresses fire while preserving ground movement");
  const float timer=a.view.fire_recovery_seconds;
  a.view.disabled_seconds=.1F;s.enemies(dt);
  require(a.view.fire_recovery_seconds<timer && a.view.warning_seconds==0,"disable cannot bypass or extend active return clock");
  a.view.disabled_seconds=0;a.view.position={0,2};s.capture(dt);
  require(a.view.capture_progress>0 && a.lifted,"reacquisition remains immediate without delaying capture");
  s.enemies(dt);require(a.view.warning_seconds==0,"captured actor cannot fire");
  a.view.position={0,5};s.capture(dt);
  require(a.view.capture_progress==0 && a.view.fire_recovery_seconds==gp::capture_return_seconds(),"second cancellation restarts one honest bounded return");
  }
}
void barrel_cycle_and_cancellation() {
  gp::Game::State s;s.actors={jeep()};s.next_id=2;s.view.player={0,0};
  auto& a=s.actors.front();
  for(unsigned volley=0;volley<12;++volley) {
    a.view.position={0,10};a.attack_timer=0;s.enemies(dt);
    require(a.view.warning_seconds>0 && a.view.minigun_barrel==(volley+1)%6,"accepted warnings cycle each physical opening exactly once");
    a.view.warning_seconds=dt*.5F;s.enemies(dt);
    require(s.bullets.size()==volley+1,"each accepted warning remains one projectile across two full barrel cycles");
  }
  for(unsigned mode=0;mode<4;++mode) {
    a.view.position={0,10};a.view.capture_progress=0;a.lifted=false;a.view.disabled_seconds=0;a.view.fire_recovery_seconds=0;
    a.view.warning_seconds=.5F;a.attack_timer=tune::enemy_attack_period[1];
    if(mode==0) a.view.capture_progress=.01F;
    if(mode==1) a.view.fire_recovery_seconds=.5F;
    if(mode==2) a.lifted=true;
    if(mode==3) a.view.disabled_seconds=.5F;
    const auto id=s.next_id;const auto event=s.next_presentation_sequence;const auto barrel=a.view.minigun_barrel;
    s.enemies(dt);
    require(a.view.warning_seconds==0 && s.next_id==id && s.next_presentation_sequence==event && s.bullets.size()==12,
        "capture, landing, lift and disable cancel warnings without a projectile or fake firing event");
    require(a.view.minigun_barrel==barrel,"cancellation cannot advance the physical opening");
  }
  a=jeep();a.view.minigun_barrel=4;s.bullets.resize(tune::projectile_active_cap);
  const auto id=s.next_id;const auto event=s.next_presentation_sequence;s.shoot(a);
  require(s.bullets.size()==96 && s.next_id==id && s.next_presentation_sequence==event,"full shared projectile capacity suppresses ID and shot event");
}
void durable_landing_and_pause() {
  gp::Game run;run.start({.seed=774,.unlocked_blueprints=0xffff});auto& s=*run.state_;
  s.actors={jeep()};s.next_id=2;
  // Synthetic durable-capture fixture stays damage-eligible while surviving
  // the real player lasers during the resumed-state comparison.
  s.actors.front().view.hull=400;s.actors.front().view.maximum_hull=4000;
  s.view.modules[static_cast<std::size_t>(gp::Module::laser_array)]=1;
  s.view.modules[static_cast<std::size_t>(gp::Module::beam_splitter)]=2;
  s.view.modules[static_cast<std::size_t>(gp::Module::heavy_winch)]=1;s.sync();
  auto& a=s.actors.front();a.view.position={0,5};a.view.capture_progress=.7F;a.lifted=true;
  s.view.beams.push_back({1,a.view.position,.7F,true});s.capture(dt);s.sync();
  require(a.view.fire_recovery_seconds==gp::capture_return_seconds() && a.view.capture_progress==0,"real cancellation produces the durable landing fixture");
  run.pause();const auto paused=run.save_state();require(!paused.empty(),"paused landing is a valid continuation");
  run.advance(1000,{{1,1},true});require(run.save_state()==paused,"public pause freezes actual remaining landing, motion and attack clocks");
  gp::Game copy;require(copy.restore_state(paused)==gp::SaveStateIssue::none && copy.save_state()==paused,"cold restore retains exact paused landing readiness");
  require(copy.snapshot().entities.front().fire_recovery_seconds==gp::capture_return_seconds(),"cold actor cannot skip unfinished source return");
  run.resume();copy.resume();
  const float timer=run.snapshot().entities.front().fire_recovery_seconds;
  run.advance(dt);copy.advance(dt);
  require_near(run.snapshot().entities.front().fire_recovery_seconds,timer-dt,1e-6F,"resuming decrements landing by active time only");
  for(unsigned frame=0;frame<240;++frame) {
    if(run.snapshot().phase==gp::Phase::choosing_upgrade) {require(run.choose_upgrade(0),"landing fixture earned draft");require(copy.choose_upgrade(0),"restored landing earned draft");}
    run.advance(dt);copy.advance(dt);require(run.save_state()==copy.save_state(),"landing continuation remains exactly identical through resumed activity");
  }
}
void codec_and_source_death() {
  gp::Game run;run.start({.seed=514});auto& s=*run.state_;
  s.actors={jeep()};s.next_id=2;s.shoot(s.actors.front());s.sync();
  require(s.bullets.size()==1,"actual jeep emits codec fixture");
  s.actors.clear();s.sync();const auto saved=run.save_state();require(!saved.empty(),"projectile persists without live source");
  gp::Game copy;require(copy.restore_state(saved)==gp::SaveStateIssue::none,"typed minigun cold restore succeeds after source death");
  require(copy.snapshot().projectiles.front().kind==gp::EnemyProjectileKind::jeep_minigun && copy.snapshot().projectiles.front().muzzle_index==0,
      "cold projectile source/profile/barrel do not depend on live actor lookup");
  for(unsigned frame=0;frame<600;++frame) {
    if(run.snapshot().phase==gp::Phase::choosing_upgrade) {require(run.choose_upgrade(0),"draft selected");require(copy.choose_upgrade(0),"restored draft selected");}
    run.advance(dt);copy.advance(dt);require(run.save_state()==copy.save_state(),"full authority future remains byte-exact");
  }
  copy.pause();const auto paused=copy.save_state();copy.advance(.25F);
  require(copy.save_state()==paused,"pause freezes weapon readiness and source ages");
  auto old=saved;old[8]=std::byte{2};old[9]=old[10]=old[11]=std::byte{};
  require(gp::inspect_saved_state(old).issue==gp::SaveStateIssue::incompatible_rules,"authority2 bytes reject before changed layout decode");
  require(copy.restore_state(old)==gp::SaveStateIssue::incompatible_rules && copy.save_state()==paused,"old authority rejection preserves the current live state transactionally");
}
void malformed_vehicle_authority() {
  gp::Game run;run.start({.seed=775});auto& s=*run.state_;
  s.actors={jeep()};s.actors.front().view.minigun_barrel=3;s.next_id=2;s.shoot(s.actors.front());s.sync();
  const auto good=run.save_state();require(!good.empty(),"actual minigun wire fixture is valid");
  constexpr std::size_t actor=378,projectile=actor+101+4;
  auto seal=[](auto& bytes) {
    std::uint64_t hash=14695981039346656037ULL;
    for(std::size_t i=0;i<bytes.size()-8;++i) hash=(hash^std::to_integer<std::uint8_t>(bytes[i]))*1099511628211ULL;
    for(unsigned i=0;i<8;++i)bytes[bytes.size()-8+i]=static_cast<std::byte>((hash>>(8*i))&255U);
  };
  auto reject=[&](auto bytes) {
    seal(bytes);
    require(gp::inspect_saved_state(bytes).issue==gp::SaveStateIssue::malformed && run.restore_state(bytes)==gp::SaveStateIssue::malformed,"sealed invalid vehicle metadata fails full validation");
    require(run.save_state()==good,"malformed vehicle restore is transactional");
  };
  auto bad=good;bad[actor+54]=std::byte{6};reject(bad);
  bad=good;bad[actor+4]=static_cast<std::byte>(gp::EntityKind::tank);reject(bad);
  bad=good;bad[projectile+54]=std::byte{255};reject(bad);
  bad=good;bad[projectile+55]=std::byte{6};reject(bad);
  bad=good;bad[projectile+54]=static_cast<std::byte>(gp::EnemyProjectileKind::vehicle_missile);reject(bad);
  bad=good;bad[projectile+37]=static_cast<std::byte>(gp::InfantryRole::rifleman);reject(bad);
  const auto excessive=std::bit_cast<std::uint32_t>(1.6F);bad=good;
  for(unsigned i=0;i<4;++i)bad[actor+50+i]=static_cast<std::byte>((excessive>>(8*i))&255U);
  reject(bad);
  bad=good;
  for(unsigned i=0;i<8;++i)bad[projectile+56+i]=std::byte{};
  reject(bad);
  bad=good;bad[projectile+54]=static_cast<std::byte>(gp::EnemyProjectileKind::boss_missile);reject(bad);
}
void cannon_geometry_and_typed_futures() {
  unsigned rays{};
  for(const auto kind:{gp::EntityKind::tank,gp::EntityKind::boss}) {
    const float scale=kind==gp::EntityKind::boss ? 1.6F:1.F;
    for(unsigned body=0;body<24;++body)for(unsigned aim=0;aim<24;++aim) {
      auto a=jeep();a.view.kind=kind;
      const float angle=static_cast<float>(body)*6.283185307F/24;
      a.view.velocity={std::sin(angle),-std::cos(angle)};
      const float target_angle=static_cast<float>(aim)*6.283185307F/24;
      const gp::Vec2 target{a.view.position.x+12*std::sin(target_angle),a.view.position.z+12*std::cos(target_angle)};
      const auto shot=gp::vehicle_cannon_shot(a.view,target);
      require(shot.valid,"normal and boss cannons solve all ordinary body/aim quadrants");
      const Point measured{-.006042052872934473,.5281190927719316,1.294246575342466};
      const Point socket{-.04,.955,-.24};
      const auto local=add(socket,yaw(measured,shot.yaw_radians));const auto world=yaw(local,angle);
      require_near(shot.origin.x,a.view.position.x+scale*static_cast<float>(world[0]),3e-6F,"cannon origin X comes from independently measured inner aperture");
      require_near(shot.origin.z,a.view.position.z-scale*static_cast<float>(world[2]),3e-6F,"cannon origin Z applies source socket and boss scale once");
      require_near(shot.height,scale*static_cast<float>(world[1]),1e-6F,"level cannon retains actual source aperture height");
      const float dx=target.x-shot.origin.x,dz=target.z-shot.origin.z;
      require_near(std::hypot(shot.direction.x,shot.direction.z),1,1e-6F,"physical cannon axis is unit length");
      require_near(dx*shot.direction.z-dz*shot.direction.x,0,1e-5F,"level physical cannon axis meets the committed ground point");
      require(dx*shot.direction.x+dz*shot.direction.z>0,"cannon fires forward through its aperture");
      ++rays;
    }
    gp::Game run;run.start({.seed=781});auto& s=*run.state_;
    auto a=jeep();a.view.kind=kind;s.actors={a};s.next_id=2;
    if(kind==gp::EntityKind::boss) {
      s.boss.emplace();s.boss->actor_id=a.view.id;
      s.boss->plan=gp::boss_attack_plan(a.view,a.aim,{},gp::BossAttackKind::fan);
      require(s.boss->plan.has_value(),"source fixture has a legal committed boss fan");
      for(unsigned i=0;i<3;++i)s.boss_release(s.actors.front());
    } else s.shoot(s.actors.front());
    s.sync();
    const unsigned count=kind==gp::EntityKind::boss?3:1;
    const auto profile=kind==gp::EntityKind::boss?gp::EnemyProjectileKind::boss_missile:gp::EnemyProjectileKind::vehicle_missile;
    require(s.bullets.size()==count,"tank and boss retain existing one/three projectile budgets");
    const auto physical=gp::vehicle_cannon_shot(a.view,a.aim);
    for(const auto& p:s.view.projectiles) {
      require(p.kind==profile && p.source_id==a.view.id && p.muzzle_index==0,"cannon publishes typed source identity without minigun barrel data");
      require_near(p.launch_position.x,physical.origin.x,1e-6F,"all boss fan missiles originate at the physical central aperture X");
      require_near(p.launch_position.z,physical.origin.z,1e-6F,"all boss fan missiles originate at the physical central aperture Z");
      require_near(p.launch_direction.x,physical.direction.x,1e-6F,"boss fan retains central physical departure axis X");
      require_near(p.launch_direction.z,physical.direction.z,1e-6F,"boss fan retains central physical departure axis Z");
      const auto index=static_cast<std::size_t>(kind)-static_cast<std::size_t>(gp::EntityKind::soldier);
      require_near(p.damage,tune::enemy_damage[index],1e-6F,"cannon source correction does not change damage");
      require_near(std::hypot(p.velocity.x,p.velocity.z),tune::projectile_speed[index],1e-6F,"cannon source correction does not change speed");
    }
    s.actors.clear();s.sync();const auto bytes=run.save_state();require(!bytes.empty(),"typed cannon projectiles remain valid after source death");
    gp::Game restored;require(restored.restore_state(bytes)==gp::SaveStateIssue::none && restored.save_state()==bytes,"source-dead cannon kind and central axis cold-restore exactly");
    require(restored.snapshot().projectiles.size()==count && restored.snapshot().projectiles.front().kind==profile,"cold cannon profile does not depend on live source lookup");
    for(unsigned frame=0;frame<360;++frame) {
      if(run.snapshot().phase==gp::Phase::choosing_upgrade){require(run.choose_upgrade(0),"cannon earned draft");require(restored.choose_upgrade(0),"restored cannon earned draft");}
      run.advance(dt);restored.advance(dt);require(run.save_state()==restored.save_state(),"typed cannon continuation preserves exact future authority");
    }
  }
  require_near(gp::cannon_geometry().visible_missile_scale,.2F,1e-7F,"finite missile keeps source-qualified uniform scale");
  std::cout<<"CANNON source rays="<<rays<<'\n';
}
}
int main() {source_geometry();commitment_and_budget();capture_readiness();barrel_cycle_and_cancellation();durable_landing_and_pause();codec_and_source_death();malformed_vehicle_authority();cannon_geometry_and_typed_futures();std::cout<<"Vehicle gameplay contracts passed: eight focused groups.\n";}
