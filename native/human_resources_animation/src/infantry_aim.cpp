#include <human_resources/animation/animation.h>
#include "pose_math.h"
#include <algorithm>
#include <cmath>

namespace human_resources::animation {
bool aim_infantry_pose(const Asset& asset, const Pose& base, const Pose& reference, Vec3 target, Pose& output, float weight) {
  const auto count = asset.nodes().size(), chest = asset.find_node("chest"), weapon = asset.find_node("socket_weapon");
  if (!std::isfinite(weight) || weight < 0 || weight > 1 || base.node_count != count || reference.node_count != count || chest >= count || weapon >= count ||
      !std::all_of(target.begin(), target.end(), [](float v) { return std::isfinite(v) && std::abs(v) < 1000; }))
    return false;
  std::array<std::size_t, 2> muzzles{asset.find_node("socket_muzzle"), count};
  if (muzzles[0] >= count) {
    muzzles = {asset.find_node("socket_muzzle_left"), asset.find_node("socket_muzzle_right")};
    if (muzzles[0] >= count)
      muzzles = {asset.find_node("socket_nose"), count};
  }
  if (muzzles[0] >= count)
    return false;
  const auto descendant = [&](std::size_t node) {
    for (std::size_t depth = 0; depth < max_nodes; ++depth) {
      if (node == chest) return true;
      const auto parent = asset.nodes()[node].parent;
      if (parent < 0) return false;
      node = static_cast<std::size_t>(parent);
    }
    return false;
  };
  if (!descendant(weapon) || !descendant(muzzles[0]) || (muzzles[1] < count && !descendant(muzzles[1])))
    return false;
  Vec3 muzzle{}, forward{}, pivot{};
  for (std::size_t i = 0; i < 3; ++i) {
    muzzle[i] = reference.world[muzzles[0]][12 + i];
    if (muzzles[1] < count) muzzle[i] = (muzzle[i] + reference.world[muzzles[1]][12 + i]) * .5F;
    forward[i] = reference.world[weapon][8 + i];
    pivot[i] = reference.world[chest][12 + i];
  }
  const auto dot = [](Vec3 a, Vec3 b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; };
  const float length = std::sqrt(dot(forward, forward)), horizontal = std::hypot(forward[0], forward[2]);
  if (length < .001F || horizontal < .001F)
    return false;
  for (float& v : forward) v /= length;
  const Vec3 axis{-reference.world[weapon][10] / horizontal, 0, reference.world[weapon][8] / horizontal};
  const Vec3 up{-axis[2] * forward[1], axis[2] * forward[0] - axis[0] * forward[2], axis[0] * forward[1]};
  const Vec3 ray{target[0] - pivot[0], target[1] - pivot[1], target[2] - pivot[2]};
  const Vec3 offset{muzzle[0] - pivot[0], muzzle[1] - pivot[1], muzzle[2] - pivot[2]};
  const float f = dot(ray, forward), v = dot(ray, up), h = dot(offset, up), distance = std::hypot(f, v);
  if (!std::isfinite(distance) || distance < std::abs(h) + .001F)
    return false;
  const float pitch = std::clamp(std::atan2(v, f) - std::asin(std::clamp(h / distance, -1.F, 1.F)), -.35F, .45F) * weight;
  Vec3 local_axis = axis;
  const auto parent = asset.nodes()[chest].parent;
  if (parent >= 0) {
    const auto& frame = base.world[static_cast<std::size_t>(parent)];
    for (std::size_t column = 0; column < 3; ++column) {
      const float norm = std::sqrt(frame[column * 4] * frame[column * 4] + frame[column * 4 + 1] * frame[column * 4 + 1] + frame[column * 4 + 2] * frame[column * 4 + 2]);
      if (std::abs(norm - 1) > .001F) return false;
      local_axis[column] = frame[column * 4] * axis[0] + frame[column * 4 + 1] * axis[1] + frame[column * 4 + 2] * axis[2];
    }
  }
  const float sine = std::sin(pitch * .5F), cosine = std::cos(pitch * .5F);
  const Quaternion turn{local_axis[0] * sine, local_axis[1] * sine, local_axis[2] * sine, cosine};
  const auto& q = base.local[chest].rotation;
  Pose candidate = base;
  candidate.local[chest].rotation = {
      turn[3] * q[0] + turn[0] * q[3] + turn[1] * q[2] - turn[2] * q[1],
      turn[3] * q[1] - turn[0] * q[2] + turn[1] * q[3] + turn[2] * q[0],
      turn[3] * q[2] + turn[0] * q[1] - turn[1] * q[0] + turn[2] * q[3],
      turn[3] * q[3] - turn[0] * q[0] - turn[1] * q[1] - turn[2] * q[2]};
  if (!detail::compose(asset, candidate)) return false;
  output = candidate;
  return true;
}
} // namespace human_resources::animation
