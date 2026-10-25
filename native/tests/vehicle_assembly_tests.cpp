#include "test_support.h"
#include <human_resources/product/presentation.h>
#include <human_resources/product/scene_effects.h>
#include <human_resources/gameplay/tuning.h>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <numbers>
namespace hr=human_resources;
using test_support::require;
using test_support::require_near;
namespace {
using M=hr::presentation::Matrix;
using V=hr::presentation::Vec3;
M local(V p={},V r={}) {return hr::presentation::model_matrix({.position=p,.rotation=r});}
M mul(const M& a,const M& b) {return hr::presentation::multiply(a,b);}
V point(const M& m,V p) {return {m[0]*p.x+m[1]*p.y+m[2]*p.z+m[3],m[4]*p.x+m[5]*p.y+m[6]*p.z+m[7],m[8]*p.x+m[9]*p.y+m[10]*p.z+m[11]};}
const hr::presentation::StyleInstance& mesh(const hr::presentation::StyleFrame& frame,std::string_view id) {
  const auto i=std::find_if(frame.instances.begin(),frame.instances.end(),[&](const auto& part){return part.mesh==id;});
  require(i!=frame.instances.end(),"vehicle source part exists");return *i;
}
void matrix_near(const M& a,const M& b) {for(std::size_t k=0;k<16;++k) require_near(a[k],b[k],.00005F,"flat part equals complete resolved parent/local matrix");}
void rigid_assemblies() {
  for(float progress:{0.F,.2F,.6F,.9F,.99F}) for(unsigned heading=0;heading<24;++heading) {
    hr::gameplay::Snapshot run;run.stats.active_seconds=1.231;run.player={0,-10};
    const float yaw=static_cast<float>(heading)*2*std::numbers::pi_v<float>/24;
    run.entities.push_back({.id=7,.kind=hr::gameplay::EntityKind::jeep,.position={1,2},
        .velocity={std::sin(yaw),-std::cos(yaw)},.capture_progress=progress});
    const auto frame=hr::product::scene(run,390.F/844.F,false);
    const auto chassis=hr::presentation::model_matrix(mesh(frame,"jeep-chassis"));
    const auto root=mul(chassis,local({0,-.12F,0}));
    std::vector<const hr::presentation::StyleInstance*> wheels;
    for(const auto& part:frame.instances) if(part.mesh=="jeep-wheel") wheels.push_back(&part);
    require(wheels.size()==4,"one coherent four-wheel assembly");
    const std::array<V,4> sockets{{{.78F,.18F,1.02F},{-.78F,.18F,1.02F},{.78F,.18F,-1.04F},{-.78F,.18F,-1.04F}}};
    for(std::size_t k=0;k<4;++k) matrix_near(hr::presentation::model_matrix(*wheels[k]),mul(chassis,local(sockets[k],{0,k%2 ? std::numbers::pi_v<float>:0,0})));
    const auto geometry=hr::gameplay::minigun_geometry();
    const auto anchor=mul(root,local({geometry.anchor[0],geometry.anchor[1],geometry.anchor[2]}));
    matrix_near(hr::presentation::model_matrix(mesh(frame,"jeep-minigun-fixed-post")),anchor);
    run.entities[0].kind=hr::gameplay::EntityKind::tank;
    const auto tank=hr::product::scene(run,390.F/844.F,false);
    const auto hull=hr::presentation::model_matrix(mesh(tank,"tank-hull"));
    const auto expected=point(hull,{-.04F,.955F,-.24F});const auto actual=mesh(tank,"tank-turret").position;
    require_near(actual.x,expected.x,1e-5F,"turret source socket follows parent pitch/roll X");
    require_near(actual.y,expected.y,1e-5F,"turret source socket follows parent pitch/roll Y");
    require_near(actual.z,expected.z,1e-5F,"turret source socket follows parent pitch/roll Z");
  }
}
void true_muzzle_and_static_source_death() {
  for(bool stopped : {false, true}) for(unsigned barrel=0;barrel<6;++barrel) for(unsigned heading=0;heading<24;++heading) {
    hr::gameplay::Snapshot run;run.stats.active_seconds=10;run.player={0,0};
    const float yaw=static_cast<float>(heading)*2*std::numbers::pi_v<float>/24;
    run.entities.push_back({.id=7,.kind=hr::gameplay::EntityKind::jeep,.position={1,12},
        .velocity=stopped ? hr::gameplay::Vec2{} : hr::gameplay::Vec2{std::sin(yaw),-std::cos(yaw)},.warning_aim=run.player,.warning_seconds=.5F,
        .minigun_barrel=static_cast<std::uint8_t>(barrel), .body_heading=stopped ? std::optional{yaw} : std::nullopt});
    const auto shot=hr::gameplay::jeep_minigun_shot(run.entities[0],run.player,static_cast<std::uint8_t>(barrel));
    require(shot.valid,"source solution eligible for native assembly proof");
    run.presentation_events[0]={.sequence=1,.active_seconds=10,.kind=hr::gameplay::PresentationEventKind::enemy_shot,.actor_id=7};
    const auto frame=hr::product::scene(run,390.F/844.F,true);
    const auto head=hr::presentation::model_matrix(mesh(frame,"jeep-minigun-head"));
    auto raw=hr::gameplay::minigun_geometry().muzzles[barrel];raw[2]+=hr::gameplay::minigun_geometry().spawn_forward_offset;
    const auto muzzle=point(head,{raw[0],raw[1],raw[2]});
    require_near(muzzle.x,shot.origin.x,3e-6F,"native head actual muzzle equals portable source X");
    require_near(muzzle.y,shot.height,3e-6F,"native head actual muzzle equals portable source height");
    require_near(muzzle.z,2.3F-shot.origin.z,3e-6F,"native head actual muzzle equals portable source station");
    const auto flash=std::find_if(frame.instances.begin(),frame.instances.end(),[](const auto& part){return part.effect && part.mesh=="cube" && part.tint[1]==.66F;});
    require(flash!=frame.instances.end(),"typed enemy shot creates one actual selected opening flash");
    require_near(flash->position.x,muzzle.x,3e-6F,"flash shares exact assembled muzzle");
    require_near(flash->position.y,muzzle.y,3e-6F,"flash shares exact assembled muzzle elevation");
    run.entities.clear();run.presentation_events={};
    run.projectiles.push_back({.id=8,.position=shot.origin,.velocity={shot.direction.x*7,shot.direction.z*7},.radius=.28F,
        .source_id=7,.launch_position=shot.origin,.age_seconds=.1F,.launch_height=shot.height,
        .rise_per_second=(2.5F-shot.height)*7/std::hypot(shot.origin.x,shot.origin.z),
        .kind=hr::gameplay::EnemyProjectileKind::jeep_minigun,.muzzle_index=static_cast<std::uint8_t>(barrel)});
    const auto orphan=hr::product::scene(run,390.F/844.F,true);
    require(std::any_of(orphan.instances.begin(),orphan.instances.end(),[](const auto& part){return part.effect && part.mesh=="cube" && part.tint[1]==.65F;}),
        "typed static projectile remains visible without live source or invented flash");
    run.projectiles.front().age_seconds=.2F;
    const auto readable=hr::product::scene(run,390.F/844.F,true);
    bool optical=false,core=false,footprint=false,underlay=false;
    unsigned optical_count=0,core_count=0,underlay_count=0;
    for(const auto& part:readable.instances) if(part.effect && part.mesh=="cube") {
      const float pixels=part.scale.x*std::abs(readable.camera[0])*390.F;
      if(part.tint[1]==.24F && part.position.y>.1F) {
        ++optical_count;
        optical=part.scale.x>=.07F && part.scale.y==part.scale.x && pixels>=1.99F && pixels<=3.F;
      }
      if(part.tint[1]==.94F) {
        ++core_count;
        core=part.scale.x>=.025F && part.scale.y==part.scale.x && pixels>=.99F && pixels<=1.5F;
      }
      if(part.tint[0]==.025F && part.tint[1]==.07F) {
        ++underlay_count;
        underlay=part.scale.y==part.scale.x && pixels>=3.29F && pixels<=4.5F;
      }
      if(part.tint[3]==.52F) footprint=std::hypot(part.scale.x,part.scale.z)<run.projectiles.front().radius;
    }
    require(optical && core && underlay && optical_count==1 && core_count==1 && underlay_count==1,
        "fine physical minigun has one bounded phone-readable optical trail/core and contrast underlay");
    require(footprint,"minigun hit cue stays inscribed inside the unchanged collision radius");
  }
  require(std::sqrt(3.F)*hr::gameplay::tuning::minigun_visible_flash_radius<.024565F &&
      std::sqrt(2.F)*hr::gameplay::tuning::minigun_visible_tracer_radius<.024565F,
      "finite visible flash/tracer fit inside qualified ray clearance; abstract collision radius is separate");
}
void vehicle_capture_enter_return() {
  hr::gameplay::Snapshot run;run.stats.active_seconds=1;run.player={0,0};
  run.entities.push_back({.id=7,.kind=hr::gameplay::EntityKind::jeep,.position={1,2},.velocity={1,0}});
  hr::product::SceneEffects history;
  const auto before=mesh(hr::product::scene(run,390.F/844.F,false,false,&history),"jeep-chassis");
  run.entities[0].velocity={};run.entities[0].capture_progress=.01F;run.stats.active_seconds+=1.0/60;
  auto raised=mesh(hr::product::scene(run,390.F/844.F,false,false,&history),"jeep-chassis");
  require_near(std::remainder(raised.rotation.y-before.rotation.y,2*std::numbers::pi_v<float>),0,1e-6F,
      "zero lifted velocity never snaps vehicle to ID-derived yaw");
  for(unsigned frame=0;frame<60;++frame) {run.entities[0].capture_progress=.9F;run.stats.active_seconds+=1.0/60;
    raised=mesh(hr::product::scene(run,390.F/844.F,false,false,&history),"jeep-chassis");}
  run.entities[0].capture_progress=0;run.entities[0].velocity={0,1};run.stats.active_seconds+=1.0/60;
  const auto cancelled=mesh(hr::product::scene(run,390.F/844.F,false,false,&history),"jeep-chassis");
  matrix_near(hr::presentation::model_matrix(raised),hr::presentation::model_matrix(cancelled));
  for(unsigned frame=0;frame<90;++frame) {run.stats.active_seconds+=1.0/60;run.entities[0].position.x+=.01F;
    (void)hr::product::scene(run,390.F/844.F,false,false,&history);}
  matrix_near(hr::presentation::model_matrix(mesh(hr::product::scene(run,390.F/844.F,false,false,&history),"jeep-chassis")),
      hr::presentation::model_matrix(mesh(hr::product::scene(run,390.F/844.F,false),"jeep-chassis")));
}
void cannon_origin_and_guided_departure() {
  for(bool stopped : {false, true}) for(const auto kind:{hr::gameplay::EntityKind::tank,hr::gameplay::EntityKind::boss})
    for(unsigned heading=0;heading<24;++heading) {
      hr::gameplay::Snapshot run;run.stats.active_seconds=10;run.player={-2,0};
      const float yaw=static_cast<float>(heading)*2*std::numbers::pi_v<float>/24;
      run.entities.push_back({.id=7,.kind=kind,.position={1,12},.velocity=stopped ? hr::gameplay::Vec2{} : hr::gameplay::Vec2{std::sin(yaw),-std::cos(yaw)},
          .warning_aim=run.player,.warning_seconds=.5F,.body_heading=stopped ? std::optional{yaw} : std::nullopt});
      const auto shot=hr::gameplay::vehicle_cannon_shot(run.entities.front(),run.player);
      require(shot.valid,"tank and boss cannon source solver accepts normal attack");
      run.presentation_events[0]={.sequence=1,.active_seconds=10,.kind=hr::gameplay::PresentationEventKind::enemy_shot,.actor_id=7};
      const auto frame=hr::product::scene(run,390.F/844.F,true);
      auto raw=hr::gameplay::cannon_geometry().muzzle;raw[2]+=hr::gameplay::cannon_geometry().launch_forward_offset;
      const auto turret=hr::presentation::model_matrix(mesh(frame,"tank-turret"));
      const auto muzzle=point(turret,{raw[0],raw[1],raw[2]});
      require_near(muzzle.x,shot.origin.x,6e-6F,"tank/boss actual aperture matches saved authority X");
      require_near(muzzle.y,shot.height,6e-6F,"tank/boss actual aperture matches saved authority height");
      require_near(muzzle.z,2.3F-shot.origin.z,6e-6F,"tank/boss actual aperture matches saved authority station");
      run.entities.clear();run.presentation_events={};
      const float flight=std::hypot(run.player.x-shot.origin.x,run.player.z-shot.origin.z)/8;
      for(float angle:{-.22F,0.F,.22F}) {
        const hr::gameplay::Vec2 direction{shot.direction.x*std::cos(angle)-shot.direction.z*std::sin(angle),
            shot.direction.x*std::sin(angle)+shot.direction.z*std::cos(angle)};
        hr::gameplay::ProjectileView projectile{.id=8,.position=shot.origin,.velocity={direction.x*8,direction.z*8},.radius=.4F,
            .source_id=7,.launch_position=shot.origin,.launch_height=shot.height,.rise_per_second=(2.5F-shot.height)/flight,
            .kind=kind==hr::gameplay::EntityKind::boss ? hr::gameplay::EnemyProjectileKind::boss_missile : hr::gameplay::EnemyProjectileKind::vehicle_missile,
            .launch_direction=shot.direction};
        for(float age:{0.F,.00001F,.12F,flight,flight+1}) {
          projectile.age_seconds=age;projectile.position={shot.origin.x+projectile.velocity.x*age,shot.origin.z+projectile.velocity.z*age};
          run.projectiles={projectile};
          const auto orphan=hr::product::scene(run,390.F/844.F,true);
          const auto& missile=mesh(orphan,"missile");
          const float scale=.2F*(kind==hr::gameplay::EntityKind::boss ? 1.6F : 1.F);
          require_near(missile.scale.x,scale,1e-7F,"visible missile retains qualified uniform aperture size");
          require_near(missile.scale.y,scale,1e-7F,"visible missile never uses abstract hit radius as diameter");
          require_near(missile.scale.z,scale,1e-7F,"no unqualified missile length stretch");
          if(age==0) {
            require_near(missile.position.x,muzzle.x,6e-6F,"orphan launch begins at actual source aperture");
            require_near(missile.position.y,muzzle.y,6e-6F,"orphan launch height persists without live source");
            const auto matrix=hr::presentation::model_matrix(missile);
            require_near(matrix[6]/scale,0,1e-6F,"cannon departure is horizontal");
            require_near(matrix[2]/scale,shot.direction.x,1e-5F,"all fan pellets initially leave real barrel axis");
            require_near(matrix[10]/scale,-shot.direction.z,1e-5F,"saved barrel axis survives source death");
          } else if(age<.001F) {
            require_near((missile.position.x-muzzle.x)/age,shot.direction.x*8,.1F,"initial launch tangent follows actual cannon");
            require_near(missile.position.y,muzzle.y,1e-5F,"initial launch has no hidden upward ray");
          }
          if(age>=.12F) {
            require_near(missile.position.x,projectile.position.x,1e-5F,"short source-axis bridge joins unchanged 2D path");
            require_near(missile.position.z,2.3F-projectile.position.z,1e-5F,"fan presentation rejoins authoritative station");
          }
          if(age>=flight) require_near(missile.position.y,2.5F,1e-6F,"guided elevation reaches committed UFO plane");
        }
      }
    }
}
}
int main() {rigid_assemblies();true_muzzle_and_static_source_death();vehicle_capture_enter_return();cannon_origin_and_guided_departure();std::cout<<"Vehicle assembly contracts passed.\n";}
