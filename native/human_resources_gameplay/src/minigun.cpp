#include <human_resources/gameplay/gameplay.h>
#include <human_resources/gameplay/tuning.h>
#include <cmath>

namespace human_resources::gameplay {
float capture_return_seconds() { return tuning::capture_return_seconds; }
MinigunGeometry minigun_geometry() {
  return {tuning::minigun_anchor, tuning::minigun_yaw_pivot, tuning::minigun_pitch_offset,
      tuning::minigun_muzzles, tuning::minigun_spawn_forward_offset,
      tuning::minigun_maximum_elevation, tuning::minigun_minimum_range};
}
float vehicle_body_yaw(const EntityView& actor) {
  if (actor.body_heading)
    return *actor.body_heading;
  return std::hypot(actor.velocity.x, actor.velocity.z) > .08F ? std::atan2(actor.velocity.x, -actor.velocity.z)
      : static_cast<float>(actor.id) * 1.7F;
}
MinigunShot jeep_minigun_shot(const EntityView& actor, Vec2 aim, std::uint8_t barrel) {
  if (actor.kind != EntityKind::jeep || barrel >= 6 || !std::isfinite(aim.x) || !std::isfinite(aim.z) ||
      !std::isfinite(actor.position.x) || !std::isfinite(actor.position.z) ||
      !std::isfinite(actor.velocity.x) || !std::isfinite(actor.velocity.z) ||
      (actor.body_heading && !std::isfinite(*actor.body_heading))) return {};
  const auto geometry = minigun_geometry();
  const auto& muzzle = geometry.muzzles[barrel];
  const float body_yaw = vehicle_body_yaw(actor), c = std::cos(body_yaw), s = std::sin(body_yaw);
  const float dx = aim.x - actor.position.x, dz = aim.z - actor.position.z;
  const float x = c * dx + s * dz - geometry.anchor[0] - geometry.yaw_pivot[0];
  const float z = s * dx - c * dz - geometry.anchor[2] - geometry.yaw_pivot[2];
  const float side_squared = x * x + z * z - muzzle[0] * muzzle[0];
  if (!std::isfinite(side_squared) || side_squared <= 0) return {};
  const float along = std::sqrt(side_squared);
  const float y = tuning::projectile_target_height - geometry.anchor[1] - geometry.yaw_pivot[1] - geometry.pitch_offset[1];
  const float plane_distance = std::hypot(y, along);
  if (plane_distance <= std::abs(muzzle[1])) return {};
  const float elevation = std::atan2(y, along) - std::asin(muzzle[1] / plane_distance);
  if (elevation < 0 || elevation > geometry.maximum_elevation_radians) return {};
  const float yaw = std::atan2(x, z) - std::atan2(muzzle[0], along);
  const float ce = std::cos(elevation), se = std::sin(elevation), cy = std::cos(yaw), sy = std::sin(yaw);
  const float nose = muzzle[2] + geometry.spawn_forward_offset;
  const float local_z = -se * muzzle[1] + ce * nose;
  const float local_y = ce * muzzle[1] + se * nose;
  const float local_x = geometry.anchor[0] + geometry.yaw_pivot[0] + cy * muzzle[0] + sy * local_z;
  const float local_forward = geometry.anchor[2] + geometry.yaw_pivot[2] - sy * muzzle[0] + cy * local_z;
  const Vec2 origin{actor.position.x + c * local_x + s * local_forward,
      actor.position.z + s * local_x - c * local_forward};
  if (std::hypot(aim.x - origin.x, aim.z - origin.z) < geometry.minimum_range) return {};
  return {origin, {c * sy + s * cy, s * sy - c * cy},
      geometry.anchor[1] + geometry.yaw_pivot[1] + geometry.pitch_offset[1] + local_y,
      yaw, elevation, barrel, true};
}
} // namespace human_resources::gameplay
