#include <human_resources/gameplay/gameplay.h>
#include <human_resources/gameplay/tuning.h>
#include <cmath>

namespace human_resources::gameplay {
InfantryAttack infantry_attack(InfantryRole role) {
  if (role < InfantryRole::rifleman || role > InfantryRole::rocketeer)
    return {};
  const auto index = static_cast<std::size_t>(role) - 1;
  return {tuning::infantry_pellets[index], tuning::infantry_damage_fraction[index],
      tuning::infantry_spread_radians[index], tuning::infantry_projectile_speed[index],
      tuning::infantry_warning_seconds[index], tuning::infantry_cooldown_seconds[index],
      tuning::infantry_recovery_seconds[index], tuning::infantry_neutral_muzzle[index],
      tuning::infantry_weapon_yaw_radians[index]};
}
InfantryShot infantry_shot(InfantryRole role, Vec2 position, Vec2 aim) {
  const auto attack = infantry_attack(role);
  if (attack.pellets == 0 || !std::isfinite(position.x) || !std::isfinite(position.z) ||
      !std::isfinite(aim.x) || !std::isfinite(aim.z))
    return {};
  const float sine = std::sin(attack.weapon_yaw_radians), cosine = std::cos(attack.weapon_yaw_radians);
  // Source +Z is render-forward, while semantic game +Z becomes render -Z.
  // Rotate the measured muzzle into a gun-forward frame, preserving its side.
  const float side = sine * attack.neutral_muzzle[2] - cosine * attack.neutral_muzzle[0];
  const float forward = sine * attack.neutral_muzzle[0] + cosine * attack.neutral_muzzle[2];
  const float x = aim.x - position.x, z = aim.z - position.z;
  const float along_squared = x * x + z * z - side * side;
  if (!std::isfinite(along_squared) || along_squared <= 0)
    return {};
  const float along = std::sqrt(along_squared);
  if (along <= forward + .05F)
    return {}; // The committed point is inside/behind the physical muzzle.
  const float yaw = std::atan2(x, z) - std::atan2(side, along);
  const float s = std::sin(yaw), c = std::cos(yaw);
  return {{position.x + c * side + s * forward, position.z - s * side + c * forward},
      {s, c}, attack.neutral_muzzle[1], true};
}
} // namespace human_resources::gameplay
