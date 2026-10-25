#include "scenery_compositions.h"
#include <algorithm>
#include <cmath>

namespace human_resources::presentation::scenery_detail {
namespace {
void wall(Builder& b, float x, double s, float scale, std::uint64_t channel) {
  b.site(SiteKind::outcrop, x, s, 3, channel);
  for (unsigned k = 0; k < 4; ++k) {
    const float offset = static_cast<float>(k) * 2.5F - 3.5F;
    const float px = x + (k % 2 == 0 ? .0F : 1.05F) * (x < 0 ? -1.F : 1.F);
    b.prop(k % 2 == 0 ? "cliff-module-a" : "cliff-module-b", px, s + offset, scale * (k == 1 ? 1.12F : k == 2 ? .83F
                                                                                                              : 1.F),
        static_cast<float>(k) * .8F, 0, {.96F, .88F, .99F, 1});
  }
  const float inward = x + (x < 0 ? 1.8F : -1.8F);
  b.prop("desert-boulder-a", inward, s - 2, .85F, .6F);
  b.prop("desert-boulder-b", inward - .25F, s + 2.2, .7F, 1.8F);
  b.understory(inward, s + 3.1, true, .8F);
}
void cactus(Builder& b, float x, double s, std::uint64_t channel, float scale = 1) {
  b.site(SiteKind::cactus, x, s, .75F, channel);
  b.prop("cactus-tall", x, s, scale, .7F, 0, {.88F, .97F, .9F, 1});
  b.prop("cactus-short", x + .85F, s + .8, scale * .67F, 1.7F, 0, {.88F, .97F, .9F, 1});
  b.understory(x, s + 1.2, true, .65F);
}
void service_stop(Builder& b, double s) {
  float outer = 100;
  for (double d = -3.5; d <= 3.5; d += .5) {
    const auto r = scenery_road(b.query.theme, b.query.seed, s + d);
    outer = std::min(outer, r.center - r.half_width);
  }
  const float x = outer - 2.3F;
  b.site(SiteKind::service_stop, x, s, 2.3F, 420);
  b.patch("ground-pavement", x - 1, s, 3.2F, 4.6F, 0, .009F, {.96F, .92F, .85F, 1});
  for (double offset = -4.5; offset < 4.5; offset += .5) {
    const double a = s + offset, c = a + .5;
    const auto ra = scenery_road(b.query.theme, b.query.seed, a), rc = scenery_road(b.query.theme, b.query.seed, c);
    const float ea = ra.center - ra.half_width, ec = rc.center - rc.half_width;
    const std::array<Vec3, 4> points{{{x, .009F, b.local(a)}, {ea, .009F, b.local(a)}, {ec, .009F, b.local(c)}, {x, .009F, b.local(c)}}};
    const float va = static_cast<float>((a - b.uv_origin) / 5), vc = static_cast<float>((c - b.uv_origin) / 5);
    b.quad(b.surface("ground-pavement", {.96F, .92F, .85F, 1}), points, {{{x / 5, va}, {ea / 5, va}, {ec / 5, vc}, {x / 5, vc}}});
  }
  b.prop("service-canopy", x, s, .95F, pi / 2);
  b.prop("fuel-pump-red", x - .5F, s - 1.2, .8F, pi / 2);
  b.prop("fuel-pump-blue", x + .5F, s + 1.2, .8F, pi / 2);
  b.prop("road-bollard", outer - .5F, s - 3.8, .8F);
  b.prop("road-bollard", outer - .5F, s + 3.8, .8F);
}
} // namespace
void desert_sector(Builder& b) {
  const double center = static_cast<double>(b.sector) * 64;
  const unsigned variant = composition(b.query.seed, b.sector);
  // Overlapping cliff masses form a continuous broken silhouette. Recesses and
  // foreground bulges are part of the composition, not independent rock drops.
  wall(b, 10.4F, center + 23, 2.05F, 410);
  wall(b, variant == 1 ? 10.5F : 10.2F, center + (variant == 0 ? -2 : 4), 1.9F, 411);
  wall(b, variant == 2 ? 9.2F : 8.5F, center - 12, .85F, 412);
  wall(b, 10.5F, center - 27, 2.1F, 413);
  float left = 100;
  for (double d = 18; d < 29; d += 1) {
    const auto r = scenery_road(b.query.theme, b.query.seed, center + d);
    left = std::min(left, r.center - r.half_width);
  }
  wall(b, left - 3.F, center + 24, .8F, 414);
  cactus(b, 8.8F, center + 16, 430, 1.35F);
  cactus(b, 6.6F, center - 20, 431, 1.3F);
  const auto verge = scenery_road(b.query.theme, b.query.seed, center - 3);
  cactus(b, verge.center + verge.half_width + 1.2F, center - 3, 432, .85F);
  const auto top = scenery_road(b.query.theme, b.query.seed, center + 23);
  cactus(b, top.center - top.half_width - 1.25F, center + 23, 433, 1.1F);
  b.site(SiteKind::outcrop, 5.9F, center + 3, .7F, 440);
  b.prop("desert-boulder-b", 5.9F, center + 3, .8F, 2.3F);
  b.prop("desert-scrub", 6.4F, center + 1.5, .8F);
  b.site(SiteKind::outcrop, -3.8F, center - 9, 1, 441);
  b.prop("desert-boulder-a", -3.8F, center - 9, .9F, .3F);
  b.understory(-3.8F, center - 8.5, true, .8F);
  if (variant != 1)
    service_stop(b, center + (variant == 0 ? -17 : 15));
}
} // namespace human_resources::presentation::scenery_detail
