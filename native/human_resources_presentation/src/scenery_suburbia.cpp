#include "scenery_compositions.h"
#include <algorithm>
#include <cmath>

namespace human_resources::presentation::scenery_detail {
namespace {
void home(Builder& b, unsigned slot, float side, double station) {
  const auto lot = b.sector;
  const double s = station;
  const auto road = scenery_road(b.query.theme, b.query.seed, s);
  const float edge = road.center + side * road.half_width;
  const auto curb = [&](double station, float offset) {
    const auto r = scenery_road(b.query.theme, b.query.seed, station);
    return r.center + side * (r.half_width + offset);
  };
  float outer = side * (edge + side * 1.25F);
  for (double offset = -2.7; offset <= 2.7; offset += .5) {
    const auto section = scenery_road(b.query.theme, b.query.seed, s + offset);
    outer = std::max(outer, side * section.center + section.half_width + 1.25F);
  }
  const float x = side * (outer + 2.85F);
  const bool park = composition(b.query.seed, b.sector) == 1 && slot == 2;

  b.site(park ? SiteKind::garden : SiteKind::home, x, s, 2.9F, 320 + slot * 10);
  if (park) {
    b.prop("tree-round-large", x, s, 1.1F, 1, 0, {.83F, .94F, .85F, 1});
    b.prop("tree-round-small", x + side * 1.8F, s + 2.1, .9F, .5F, 0, {.83F, .94F, .85F, 1});
    b.path("ground-pavement", edge + side, s, x, s, 1.3F, .04F);
    b.understory(x - side * .8F, s - 1.5, false);
  } else {
    const bool coral = key(b.query.seed, lot, 330 + slot * 10) % 2 == 0;
    const float yaw = side < 0 ? -pi / 2 : pi / 2;
    b.prop(coral ? "house-coral" : "house-teal", x, s, 1, yaw, 0, {.95F, .98F, .94F, 1});
    b.path("ground-pavement", curb(s + .25, .9F), s + .25, x, s + .25, 1.15F, .035F);
    // A driveway joins the sidewalk beside the house, not through its garden.
    b.path("ground-pavement", curb(s + 4.5, .8F), s + 4.5, x + side * .7F, s + 4.5, 2.25F, .025F);
    b.prop(coral ? "mailbox-red" : "mailbox-teal", curb(s - 2.1, .65F), s - 2.1, .9F, yaw);
    b.prop("wheelie-bin", curb(s + 3.4, 1.6F), s + 3.4, .8F, yaw + .1F);
    b.prop("bush-low", curb(s - 2.1, 2.2F), s - 2.1, .85F, .1F);
    // Hedged side boundary terminates before the footpath and driveway.
    for (int i = 0; i < 2; ++i)
      b.prop("hedge-module", curb(s - 3.5, 2.4F + static_cast<float>(i) * 1.7F), s - 3.5, .85F, 0);
    // Planting belongs to the frontage, with a gap at the entrance.
    for (float along : {-1.8F, -1.1F, 1.5F, 2.1F}) {
      b.prop("bush-low", side * (outer + .65F), s + along, .58F, along);
      b.prop("grass-tuft", side * (outer + .25F), s + along + .25, .7F, along);
    }
  }
  b.prop("streetlamp", curb(s - 4.7, .65F), s - 4.7, .95F, 0);
  b.prop("tree-round-large", curb(s + 9.2, 2.F), s + 9.2, 1.2F, random(b.query.seed, lot, 340) * 6, 0, {.83F, .94F, .85F, 1});
  b.understory(curb(s + 9.9, 2.F), s + 9.9, false, .7F);
}
} // namespace
void suburb_sector(Builder& b) {
  const double center = static_cast<double>(b.sector) * 64;
  unsigned slot = 0;
  for (const auto& anchor : homes(composition(b.query.seed, b.sector)))
    home(b, slot++, anchor.side, center + anchor.station);
}

} // namespace human_resources::presentation::scenery_detail
