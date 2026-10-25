#include "pose_math.h"
#include <algorithm>
#include <cmath>

namespace human_resources::animation::detail {
namespace {
Quaternion slerp(Quaternion a, Quaternion b, float t) {
  float dot = 0;
  for (std::size_t i = 0; i < 4; ++i)
    dot += a[i] * b[i];
  if (dot < 0) {
    dot = -dot;
    for (float& v : b)
      v = -v;
  }
  float wa = 1 - t, wb = t;
  if (dot < .9995F) {
    const float theta = std::acos(std::clamp(dot, 0.F, 1.F));
    const float denominator = std::sin(theta);
    wa = std::sin((1 - t) * theta) / denominator;
    wb = std::sin(t * theta) / denominator;
  }
  float length = 0;
  for (std::size_t i = 0; i < 4; ++i) {
    a[i] = wa * a[i] + wb * b[i];
    length += a[i] * a[i];
  }
  const float inverse = 1 / std::sqrt(length);
  for (float& v : a)
    v *= inverse;
  return a;
}
bool normal_matrix(const Matrix& m, std::array<float, 9>& output) {
  // Cofactor/determinant in double avoids intermediate overflow for bounded
  // affine transforms. Stored column-major as inverse transpose.
  const double a = m[0], b = m[4], c = m[8], d = m[1], e = m[5], f = m[9];
  const double g = m[2], h = m[6], i = m[10];
  const double det = a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g);
  if (!std::isfinite(det) || std::abs(det) < 1e-20)
    return false;
  const std::array<double, 9> cofactors{e * i - f * h, c * h - b * i, b * f - c * e,
      f * g - d * i, a * i - c * g, c * d - a * f,
      d * h - e * g, b * g - a * h, a * e - b * d};
  for (std::size_t k = 0; k < 9; ++k) {
    output[k] = static_cast<float>(cofactors[k] / det);
    if (!std::isfinite(output[k]))
      return false;
  }
  return true;
}
} // namespace

bool valid_transform(const Transform& t) {
  for (float v : t.translation)
    if (!std::isfinite(v) || std::abs(v) > 10000)
      return false;
  for (float v : t.scale)
    if (!std::isfinite(v) || v < .5F || v > 2.F)
      return false;
  float length = 0;
  for (float v : t.rotation) {
    if (!std::isfinite(v))
      return false;
    length += v * v;
  }
  return std::abs(length - 1) < .001F;
}
bool finite_matrix(const Matrix& m) {
  return std::all_of(m.begin(), m.end(), [](float v) { return std::isfinite(v) && std::abs(v) <= 1e30F; });
}
Transform interpolate(const Transform& a, const Transform& b, float t) {
  Transform result;
  for (std::size_t i = 0; i < 3; ++i) {
    result.translation[i] = a.translation[i] + (b.translation[i] - a.translation[i]) * t;
    result.scale[i] = a.scale[i] + (b.scale[i] - a.scale[i]) * t;
  }
  result.rotation = slerp(a.rotation, b.rotation, t);
  return result;
}
Matrix matrix(const Transform& t) {
  const auto& q = t.rotation;
  const float x = q[0], y = q[1], z = q[2], w = q[3];
  return {(1 - 2 * (y * y + z * z)) * t.scale[0], (2 * (x * y + z * w)) * t.scale[0], (2 * (x * z - y * w)) * t.scale[0], 0,
      (2 * (x * y - z * w)) * t.scale[1], (1 - 2 * (x * x + z * z)) * t.scale[1], (2 * (y * z + x * w)) * t.scale[1], 0,
      (2 * (x * z + y * w)) * t.scale[2], (2 * (y * z - x * w)) * t.scale[2], (1 - 2 * (x * x + y * y)) * t.scale[2], 0,
      t.translation[0], t.translation[1], t.translation[2], 1};
}
Matrix multiply(const Matrix& a, const Matrix& b) {
  Matrix out{};
  for (std::size_t column = 0; column < 4; ++column)
    for (std::size_t row = 0; row < 4; ++row)
      for (std::size_t k = 0; k < 4; ++k)
        out[column * 4 + row] += a[k * 4 + row] * b[column * 4 + k];
  return out;
}
bool compose(const Asset& asset, Pose& pose) {
  if (asset.nodes().empty() || asset.nodes().size() > max_nodes || asset.joints().size() > max_joints)
    return false;
  pose.node_count = asset.nodes().size();
  pose.joint_count = asset.joints().size();
  for (std::size_t n = 0; n < pose.node_count; ++n) {
    const auto parent = asset.nodes()[n].parent;
    const auto local = matrix(pose.local[n]);
    pose.world[n] = parent < 0 ? local : multiply(pose.world[static_cast<std::size_t>(parent)], local);
    if (!finite_matrix(pose.world[n]))
      return false;
  }
  for (std::size_t j = 0; j < pose.joint_count; ++j) {
    const auto& joint = asset.joints()[j];
    pose.palette[j] = multiply(pose.world[joint.node], joint.inverse_bind);
    if (!finite_matrix(pose.palette[j]) || !normal_matrix(pose.palette[j], pose.normal_palette[j]))
      return false;
  }
  return true;
}
} // namespace human_resources::animation::detail
