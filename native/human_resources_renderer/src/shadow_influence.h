#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <human_resources/presentation/style_scene.h>
#include <limits>
namespace human_resources::renderer::shadow_detail {
using Matrix = human_resources::presentation::Matrix;
using Vec3 = human_resources::presentation::Vec3;
using Point = std::array<double, 3>;
inline Point point(const Matrix& m, Point p) {
  return {m[0] * p[0] + m[1] * p[1] + m[2] * p[2] + m[3], m[4] * p[0] + m[5] * p[1] + m[6] * p[2] + m[7], m[8] * p[0] + m[9] * p[1] + m[10] * p[2] + m[11]};
}
inline std::array<Point, 8> corners(const Matrix& model, Vec3 minimum, Vec3 maximum) {
  std::array<Point, 8> result{};
  unsigned i = 0;
  for (float x : {minimum.x, maximum.x})
    for (float y : {minimum.y, maximum.y})
      for (float z : {minimum.z, maximum.z})
        result[i++] = point(model, {x, y, z});
  return result;
}
inline double receiver_floor(const Matrix& model, Vec3 minimum, Vec3 maximum) {
  double result = std::numeric_limits<double>::infinity();
  constexpr double e = std::numeric_limits<float>::epsilon();
  constexpr double gamma64 = 64 * e / (1 - 64 * e);
  for (float x : {minimum.x, maximum.x})
    for (float y : {minimum.y, maximum.y})
      for (float z : {minimum.z, maximum.z}) {
        const double a = static_cast<double>(model[4]) * x, b = static_cast<double>(model[5]) * y, c = static_cast<double>(model[6]) * z, d = model[7];
        result = std::min(result, a + b + c + d - gamma64 * (std::abs(a) + std::abs(b) + std::abs(c) + std::abs(d) + 1));
      }
  return result;
}
struct Influence final {
  Matrix camera{};
  std::array<Point, 4> offsets{};
  Point depth{};
  double floor{}, light_condition{};
  bool valid{};
  Influence(const Matrix& cam, const Matrix& light, double receiver_floor, unsigned shadow_size) : camera(cam), floor(receiver_floor) {
    if (!shadow_size || !std::isfinite(floor))
      return;
    for (float value : cam)
      if (!std::isfinite(value))
        return;
    for (float value : light)
      if (!std::isfinite(value))
        return;
    // The current world/shadow projections are affine orthographic. Other
    // projections retain all casters instead of guessing a new contract.
    for (const auto* m : {&cam, &light})
      if ((*m)[12] != 0 || (*m)[13] != 0 || (*m)[14] != 0 || (*m)[15] != 1)
        return;
    const double a = light[0], b = light[1], c = light[2], d = light[4], e = light[5], f = light[6], g = light[8], h = light[9], i = light[10];
    const double det = a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g);
    if (!std::isfinite(det) || std::abs(det) < 1e-15)
      return;
    const std::array<double, 9> inv{(e * i - f * h) / det, (c * h - b * i) / det, (b * f - c * e) / det, (f * g - d * i) / det, (a * i - c * g) / det, (c * d - a * f) / det, (d * h - e * g) / det, (b * g - a * h) / det, (a * e - b * d) / det};
    const double light_norm = std::max({std::abs(a) + std::abs(b) + std::abs(c), std::abs(d) + std::abs(e) + std::abs(f), std::abs(g) + std::abs(h) + std::abs(i)});
    const double inverse_norm = std::max({std::abs(inv[0]) + std::abs(inv[1]) + std::abs(inv[2]), std::abs(inv[3]) + std::abs(inv[4]) + std::abs(inv[5]), std::abs(inv[6]) + std::abs(inv[7]) + std::abs(inv[8])});
    light_condition = std::max(1., light_norm * inverse_norm);
    const Point across{inv[0], inv[3], inv[6]}, up{inv[1], inv[4], inv[7]};
    depth = {inv[2], inv[5], inv[8]};
    if (depth[1] >= -1e-8)
      return; // No finite downward floor intersection.
    // Current 3x3 PCF offsets are ±1texel. Linear comparison can reach a
    // further texel centre, hence a conservative total ±2texels (4/N NDC).
    const double texel = 4. / static_cast<double>(shadow_size);
    // Shader max(.00035,.0011*(1-diffuse)) lies in [0,.0011]. Positive
    // bias shrinks shadowing; retaining its entire reverse extent is safer.
    constexpr double maximum_bias = .0011;
    unsigned n = 0;
    for (double x : {-texel, texel})
      for (double y : {-texel, texel}) {
        for (unsigned k = 0; k < 3; ++k)
          offsets[n][k] = across[k] * x + up[k] * y - depth[k] * maximum_bias;
        ++n;
      }
    valid = true;
  }
  bool rejects(const Matrix& model, Vec3 minimum, Vec3 maximum) const {
    if (!valid)
      return false;
    std::array<bool, 4> outside{true, true, true, true};
    double model_magnitude = 1;
    for (unsigned row = 0; row < 3; ++row) {
      double terms = std::abs(static_cast<double>(model[row * 4 + 3]));
      const std::array<double, 3> lo{minimum.x, minimum.y, minimum.z}, hi{maximum.x, maximum.y, maximum.z};
      for (unsigned column = 0; column < 3; ++column)
        terms += std::abs(static_cast<double>(model[row * 4 + column])) * std::max(std::abs(lo[column]), std::abs(hi[column]));
      model_magnitude = std::max(model_magnitude, terms);
    }
    const auto include = [&](Point p) {
      if (!std::all_of(p.begin(), p.end(), [](double x) { return std::isfinite(x); })) {
        outside = {false, false, false, false};
        return;
      }
      for (unsigned axis = 0; axis < 2; ++axis) {
        const auto r = axis * 4;
        const double x = camera[r] * p[0] + camera[r + 1] * p[1] + camera[r + 2] * p[2] + camera[r + 3];
        const double magnitude = std::abs(camera[r] * p[0]) + std::abs(camera[r + 1] * p[1]) + std::abs(camera[r + 2] * p[2]) + std::abs(camera[r + 3]) + 1;
        // gamma64 bounds substantially more float multiplies/adds than the
        // affine vertex/projection chain; use its scale-dependent outward pad.
        constexpr double e = std::numeric_limits<float>::epsilon();
        constexpr double gamma64 = 64 * e / (1 - 64 * e);
        const double camera_norm = std::abs(static_cast<double>(camera[r])) + std::abs(static_cast<double>(camera[r + 1])) + std::abs(static_cast<double>(camera[r + 2]));
        const double point_magnitude = std::max({std::abs(p[0]), std::abs(p[1]), std::abs(p[2]), 1.});
        // Propagate absolute model/coordinate magnitudes through inverse-light
        // conditioning as well, retaining cancellation/scale uncertainty.
        const double guard = gamma64 * (magnitude + camera_norm * (model_magnitude + point_magnitude + 1) * light_condition);
        outside[axis * 2] = outside[axis * 2] && x < -1 - guard;
        outside[axis * 2 + 1] = outside[axis * 2 + 1] && x > 1 + guard;
      }
    };
    for (const auto p : corners(model, minimum, maximum))
      for (const auto offset : offsets) {
        Point start{p[0] + offset[0], p[1] + offset[1], p[2] + offset[2]};
        if (start[1] < floor)
          return false;
        include(start);
        const double distance = (floor - start[1]) / depth[1];
        include({start[0] + depth[0] * distance, floor, start[2] + depth[2] * distance});
      }
    return std::any_of(outside.begin(), outside.end(), [](bool value) { return value; });
  }
};
} // namespace human_resources::renderer::shadow_detail
