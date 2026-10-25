#include <human_resources/gameplay/gameplay.h>
#include <human_resources/gameplay/tuning.h>
#include <cmath>
namespace human_resources::gameplay {
CannonGeometry cannon_geometry() {
  return {tuning::cannon_socket,tuning::cannon_muzzle,tuning::cannon_visible_missile_scale,tuning::cannon_launch_forward_offset};
}
CannonShot vehicle_cannon_shot(const EntityView& actor,Vec2 aim) {
  if((actor.kind!=EntityKind::tank && actor.kind!=EntityKind::boss) ||
      !std::isfinite(actor.position.x) || !std::isfinite(actor.position.z) ||
      !std::isfinite(actor.velocity.x) || !std::isfinite(actor.velocity.z) ||
      !std::isfinite(aim.x) || !std::isfinite(aim.z)) return {};
  const auto geometry=cannon_geometry();
  const float scale=actor.kind==EntityKind::boss ? 1.6F : 1.F;
  const float body=vehicle_body_yaw(actor),s=std::sin(body),c=std::cos(body);
  const float dx=(aim.x-actor.position.x)/scale,dz=(aim.z-actor.position.z)/scale;
  const float x=c*dx+s*dz-geometry.socket[0],z=s*dx-c*dz-geometry.socket[2];
  const float squared=x*x+z*z-geometry.muzzle[0]*geometry.muzzle[0];
  if(!std::isfinite(squared) || squared<=0) return {};
  const float along=std::sqrt(squared),nose=geometry.muzzle[2]+geometry.launch_forward_offset;
  if(along<=nose+.05F) return {};
  const float yaw=std::atan2(x,z)-std::atan2(geometry.muzzle[0],along),sy=std::sin(yaw),cy=std::cos(yaw);
  const float local_x=geometry.socket[0]+cy*geometry.muzzle[0]+sy*nose;
  const float local_z=geometry.socket[2]-sy*geometry.muzzle[0]+cy*nose;
  return {{actor.position.x+scale*(c*local_x+s*local_z),actor.position.z+scale*(s*local_x-c*local_z)},
      {c*sy+s*cy,s*sy-c*cy},scale*(geometry.socket[1]+geometry.muzzle[1]),yaw,true};
}
} // namespace human_resources::gameplay
