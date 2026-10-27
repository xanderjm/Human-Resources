#pragma once
#include <algorithm>
#include <cmath>
#include <human_resources/presentation/style_scene.h>
namespace human_resources::presentation {
// Matches style_world.hlsl: widen the annulus without moving its inner bearing.
[[nodiscard]] inline float rotor_radius(float radius, float growth) {
  return radius + std::max(0.F, radius - .604F) * std::clamp(growth, 0.F, .6F);
}
// Conservative for any spin angle and pitch/bank once transformed by the model.
inline void expand_rotor_bounds(Vec3& minimum, Vec3& maximum, float growth) {
  if (growth <= 0)
    return;
  const float radius = std::max({std::abs(minimum.x), std::abs(maximum.x), std::abs(minimum.z), std::abs(maximum.z)});
  const float expanded = rotor_radius(radius, growth);
  minimum.x = minimum.z = -expanded;
  maximum.x = maximum.z = expanded;
}
} // namespace human_resources::presentation
