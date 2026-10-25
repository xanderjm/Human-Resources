#include "scenery_internal.h"
#include <algorithm>
#include <cmath>

namespace human_resources::presentation::scenery_detail {
namespace {
void ribbon(Builder& b, std::string_view material, double s0, double s1, float left0, float right0, float left1, float right1, float height, Color tint = {1, 1, 1, 1}, float detail = .24F) {
  const std::array<Vec3, 4> p{{{left0, height, b.local(s0)}, {right0, height, b.local(s0)}, {right1, height, b.local(s1)}, {left1, height, b.local(s1)}}};
  const float v0 = static_cast<float>((s0 - b.uv_origin) / 6), v1 = static_cast<float>((s1 - b.uv_origin) / 6);
  b.quad(b.surface(material, tint, detail), p, {{{left0 / 6, v0}, {right0 / 6, v0}, {right1 / 6, v1}, {left1 / 6, v1}}});
}
} // namespace
void roads(Builder& b) {
  const auto theme = b.query.theme;
  // Absolute UVs, with an integer-period floating origin, keep the terrain
  // attached to the world at sector and floating-origin boundaries.
  const double first = std::floor((b.query.distance - 52) * 2) / 2;
  const double last = first + 105;
  const auto ground = theme == SceneryTheme::desert ? "ground-sand" : "ground-grass";
  ribbon(b, ground, first, last, -36, 36, -36, 36, -.04F, {1, 1, 1, 1}, .16F);
  for (double s = first; s < last; s += .5) {
    const auto a = scenery_road(theme, b.query.seed, s), c = scenery_road(theme, b.query.seed, s + .5);
    const float l0 = a.center - a.half_width, r0 = a.center + a.half_width, l1 = c.center - c.half_width, r1 = c.center + c.half_width;
    if (theme == SceneryTheme::suburbia) {
      ribbon(b, "ground-pavement", s, s + .5, l0 - 1.25F, r0 + 1.25F, l1 - 1.25F, r1 + 1.25F, .055F, {1, 1, 1, 1}, .12F);
      ribbon(b, "plane", s, s + .5, l0 - .08F, r0 + .08F, l1 - .08F, r1 + .08F, .071F, {.73F, .69F, .59F, 1}, 1);
      ribbon(b, "ground-asphalt", s, s + .5, l0, r0, l1, r1, .081F, {1, 1, 1, 1}, .12F);
    } else if (theme == SceneryTheme::desert) {
      ribbon(b, "ground-dirt", s, s + .5, l0 - .45F, r0 + .45F, l1 - .45F, r1 + .45F, -.009F, {1, 1, 1, 1}, .16F);
      ribbon(b, "ground-asphalt", s, s + .5, l0, r0, l1, r1, .005F, {1.1F, .91F, 1.08F, 1}, .14F);
    } else {
      ribbon(b, "ground-dirt", s, s + .5, l0, r0, l1, r1, -.01F, {1, 1, 1, 1}, .2F);
    }
  }
  // All posts, joints and marks are anchored to absolute stations, never to a
  // moving window edge. Seeking cannot slide/reset their spacing.
  if (theme == SceneryTheme::desert) {
    const auto start = static_cast<std::int64_t>(std::floor((b.query.distance - 50) / 4));
    for (auto n = start; n < start + 27; ++n) {
      const double s = static_cast<double>(n) * 4;
      const auto r = scenery_road(theme, b.query.seed, s);
      b.sector = n;
      b.site(SiteKind::garden, r.center, s, 0, 105);
      if (n % 2 == 0)
        b.prop("road-bollard", r.center + r.half_width + .65F, s, .85F);
      const float yaw = -std::atan(r.slope);
      b.patch("plane", r.center, s, .055F, .65F, yaw, .014F, {.71F, .67F, .57F, 1});
    }
  }
  if (theme == SceneryTheme::suburbia) {
    const auto start = static_cast<std::int64_t>(std::floor((b.query.distance - 50) / 2));
    for (auto n = start; n < start + 52; ++n) {
      const double s = static_cast<double>(n) * 2;
      const auto r = scenery_road(theme, b.query.seed, s);
      for (float side : {-1.F, 1.F}) {
        const float x = r.center + side * (r.half_width + .64F);
        // Slab joints are geometry, absent from the pavement albedo.
        b.path("plane", x - side * .59F, s, x + side * .59F, s, .022F, .062F);
      }
    }
  }
}
} // namespace human_resources::presentation::scenery_detail
