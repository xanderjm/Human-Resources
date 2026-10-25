#include <human_resources/gameplay/gameplay.h>
#include <human_resources/gameplay/tuning.h>
#include <cmath>
#include <algorithm>
namespace human_resources::gameplay {
CannonGeometry cannon_geometry() {
  return {tuning::cannon_socket, tuning::cannon_muzzle, tuning::cannon_visible_missile_scale, tuning::cannon_launch_forward_offset};
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
std::optional<BossAttackPlan> boss_attack_plan(const EntityView& actor, Vec2 player, Vec2 velocity, BossAttackKind kind) {
  if (actor.kind != EntityKind::boss || kind > BossAttackKind::rake ||
      !std::isfinite(velocity.x) || !std::isfinite(velocity.z)) return {};
  const auto direct = vehicle_cannon_shot(actor, player);
  if (!direct.valid) return {};
  const float separation = std::hypot(player.x - direct.origin.x, player.z - direct.origin.z);
  const float speed = std::hypot(velocity.x, velocity.z);
  BossAttackPlan plan;
  plan.kind = kind;
  plan.warning_seconds = kind == BossAttackKind::fan ? tuning::boss_fan_warning_seconds : tuning::boss_rake_warning_seconds;
  plan.salvo_seconds = kind == BossAttackKind::fan ? 0 : 2 * tuning::boss_rake_interval_seconds;
  plan.locked_root = actor.position;
  plan.locked_body_heading = vehicle_body_yaw(actor);
  plan.initial_turret_yaw = direct.yaw_radians;
  const float lead = speed > .0001F ? std::min(plan.warning_seconds, tuning::boss_aim_lead_fraction * separation / speed) : 0;
  const Vec2 centre{player.x + velocity.x * lead, player.z + velocity.z * lead};
  const auto physical = vehicle_cannon_shot(actor, centre);
  if (!physical.valid) return {};
  for (unsigned i = 0; i < plan.shells.size(); ++i) {
    const float angle = (static_cast<float>(i) - 1) * (kind == BossAttackKind::fan ? tuning::boss_fan_spread_radians : tuning::boss_rake_spread_radians);
    auto& shell = plan.shells[i];
    if (kind == BossAttackKind::fan) {
      shell.shot = physical;
      shell.aim = centre;
      shell.trajectory = {physical.direction.x * std::cos(angle) - physical.direction.z * std::sin(angle),
          physical.direction.x * std::sin(angle) + physical.direction.z * std::cos(angle)};
    } else {
      const Vec2 ray{centre.x - actor.position.x, centre.z - actor.position.z};
      shell.aim = {actor.position.x + ray.x * std::cos(angle) - ray.z * std::sin(angle),
          actor.position.z + ray.x * std::sin(angle) + ray.z * std::cos(angle)};
      shell.shot = vehicle_cannon_shot(actor, shell.aim);
      if (!shell.shot.valid) return {};
      shell.trajectory = shell.shot.direction;
    }
  }
  if (kind == BossAttackKind::rake) {
    const auto intersection = [player](const BossShellPlan& shell) {
      return std::abs(shell.trajectory.z) > .001F ? shell.shot.origin.x +
          shell.trajectory.x * (player.z - shell.shot.origin.z) / shell.trajectory.z : shell.aim.x;
    };
    const bool right_first = std::abs(velocity.x) > .1F ? velocity.x > 0 :
        std::abs(player.x) > .001F ? player.x > 0 : (actor.id & 1U) == 0;
    if ((intersection(plan.shells[0]) > intersection(plan.shells[2])) != right_first)
      std::swap(plan.shells[0], plan.shells[2]);
    for (unsigned i = 0; i < plan.shells.size(); ++i)
      plan.shells[i].release_offset = static_cast<float>(i) * tuning::boss_rake_interval_seconds;
  }
  return plan;
}
} // namespace human_resources::gameplay
