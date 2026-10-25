#include "vehicle_assembly.h"
#include "vehicle_assembly.generated.h"
#include <human_resources/gameplay/tuning.h>
#include <algorithm>
#include <cmath>

namespace human_resources::product::detail {
namespace {
using namespace presentation;
Matrix translate(std::array<float,3> p) { return model_matrix({.position={p[0],p[1],p[2]}}); }
Matrix rotate(float x,float y=0) { return model_matrix({.rotation={x,y,0}}); }
// All vehicle children are rigid and share one positive uniform parent scale.
// Flatten their composed matrix into the existing value contract; no graph or
// renderer parent lookup, and no child Euler interpolation across gimbals.
void part(StyleFrame& frame,std::string_view mesh,const Matrix& m,float scale,
    bool effect=false,std::array<float,4> tint={1,1,1,1}) {
  const float pitch=std::asin(std::clamp(-m[6]/scale,-1.F,1.F));
  const bool ordinary=std::abs(std::cos(pitch))>1e-5F;
  const float yaw=ordinary ? std::atan2(m[2],m[10]) : std::atan2(-m[8],m[0]);
  const float roll=ordinary ? std::atan2(m[4],m[5]) : 0;
  frame.instances.push_back({.mesh=mesh,.position={m[3],m[7],m[11]},.scale={scale,scale,scale},
      .rotation={pitch,yaw,roll},.tint=tint,.effect=effect,.casts_shadow=!effect});
}
float turn(float from, float to, float fraction) {
  const float difference = std::remainder(to - from, 6.283185307179586F);
  const float t = std::clamp(fraction, 0.F, 1.F);
  return from + difference * t * t * (3 - 2 * t);
}
float boss_turret_yaw(const gameplay::EntityView& entity, const gameplay::BossCombatView* boss,
    float idle, bool reduced) {
  if (!boss || boss->actor_id != entity.id || !boss->plan)
    return idle;
  const auto& plan = *boss->plan;
  const float elapsed = std::max(0.F, boss->phase_duration - boss->phase_remaining);
  const float heading = plan.locked_body_heading - gameplay::vehicle_body_yaw(entity);
  if (boss->phase == gameplay::BossPhase::warning)
    return reduced ? plan.shells[0].shot.yaw_radians :
        turn(plan.initial_turret_yaw, plan.shells[0].shot.yaw_radians,
            elapsed / std::max(.001F, boss->phase_duration));
  if (boss->phase == gameplay::BossPhase::salvo) {
    const auto next = std::min<std::size_t>(boss->next_release, plan.shells.size());
    if (next == 0)
      return plan.shells[0].shot.yaw_radians;
    const auto& previous = plan.shells[next - 1];
    if (next == plan.shells.size() || reduced)
      return previous.shot.yaw_radians;
    constexpr float flash_hold = .09F;
    const float duration = plan.shells[next].release_offset - previous.release_offset;
    return turn(previous.shot.yaw_radians, plan.shells[next].shot.yaw_radians,
        (elapsed - previous.release_offset - flash_hold) / std::max(.001F, duration - flash_hold));
  }
  if (boss->phase == gameplay::BossPhase::recovery) {
    const float fired = plan.shells.back().shot.yaw_radians + heading;
    if (elapsed < .09F)
      return fired;
    return reduced ? idle : turn(fired, idle, (elapsed - .09F) / std::max(.001F, boss->phase_duration - .09F));
  }
  return idle;
}
}
void append_jeep(StyleFrame& frame,const StyleInstance& actor,const gameplay::EntityView& entity,
    gameplay::Vec2 aim,float wheel_angle,float flash_age) {
  const auto root=model_matrix(actor);
  const float scale=actor.scale.x;
  const auto chassis=multiply(root,translate(jeep_chassis_offset));
  part(frame,"jeep-chassis",chassis,scale);
  for(const auto& wheel:jeep_wheels) {
    const float spin=wheel.yaw>1 ? -wheel_angle : wheel_angle;
    const auto local=multiply(translate(wheel.position),multiply(rotate(0,wheel.yaw),rotate(spin)));
    part(frame,"jeep-wheel",multiply(chassis,local),scale);
  }
  const auto geometry=gameplay::minigun_geometry();
  const auto shot=gameplay::jeep_minigun_shot(entity,aim,entity.minigun_barrel);
  // Unreachable idle targets may turn the mount, but never fake a pitched shot.
  const float body=gameplay::vehicle_body_yaw(entity);
  const float dx=aim.x-entity.position.x,dz=aim.z-entity.position.z;
  const float yaw=shot.valid ? shot.yaw_radians : std::atan2(std::cos(body)*dx+std::sin(body)*dz,
      std::sin(body)*dx-std::cos(body)*dz);
  const float elevation=shot.valid ? shot.elevation_radians : 0;
  const auto anchor=multiply(root,translate(geometry.anchor));
  const auto cap=multiply(multiply(anchor,translate(geometry.yaw_pivot)),rotate(0,yaw));
  const auto head=multiply(multiply(cap,translate(geometry.pitch_offset)),rotate(-elevation));
  part(frame,"jeep-minigun-fixed-post",anchor,scale);
  part(frame,"jeep-minigun-yaw-cap",cap,scale);
  part(frame,"jeep-minigun-head",head,scale);
  if(shot.valid && flash_age>=0 && flash_age<.09F) {
    auto muzzle=geometry.muzzles[entity.minigun_barrel];
    muzzle[2]+=geometry.spawn_forward_offset;
    // A compact visible flash has a sqrt(3)*12mm bound, below the qualified
    // 24.565mm centreline clearance. Gameplay's 0.28m radius stays abstract2D.
    const float radius=gameplay::tuning::minigun_visible_flash_radius;
    const auto flash=multiply(multiply(head,translate(muzzle)),model_matrix({.scale={radius,radius,radius}}));
    part(frame,"cube",flash,
        scale*gameplay::tuning::minigun_visible_flash_radius,true,{1,.66F,.22F,1-flash_age/.09F});
  }
}
void append_tank(StyleFrame& frame,const StyleInstance& actor,const gameplay::EntityView& entity,gameplay::Vec2 aim,float flash_age,
    const gameplay::BossCombatView* boss, bool reduced_motion) {
  const auto root=model_matrix(actor);
  part(frame,"tank-hull",root,actor.scale.x);
  const auto socket=multiply(root,translate(tank_turret_offset));
  // Convert the committed direction through the complete parent orientation.
  const auto shot=gameplay::vehicle_cannon_shot(entity,aim);
  const float body=gameplay::vehicle_body_yaw(entity);
  const float dx=aim.x-entity.position.x,dz=aim.z-entity.position.z;
  const float idle=shot.valid ? shot.yaw_radians : std::atan2(std::cos(body)*dx+std::sin(body)*dz,
      std::sin(body)*dx-std::cos(body)*dz);
  const float yaw=boss_turret_yaw(entity,boss,idle,reduced_motion);
  const auto turret=multiply(socket,rotate(0,yaw));
  part(frame,"tank-turret",turret,actor.scale.x);
  if((shot.valid || (boss && boss->actor_id == entity.id && boss->plan)) && flash_age>=0 && flash_age<.09F) {
    const auto geometry=gameplay::cannon_geometry();auto muzzle=geometry.muzzle;muzzle[2]+=geometry.launch_forward_offset;
    const float radius=.035F;
    const auto flash=multiply(multiply(turret,translate(muzzle)),model_matrix({.scale={radius,radius,radius}}));
    part(frame,"cube",flash,actor.scale.x*radius,true,{1,.66F,.22F,1-flash_age/.09F});
  }
}
} // namespace human_resources::product::detail
