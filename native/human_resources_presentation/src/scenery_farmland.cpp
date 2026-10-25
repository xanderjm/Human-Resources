#include "scenery_compositions.h"
#include <algorithm>
#include <cmath>

namespace human_resources::presentation::scenery_detail {
namespace {
float edge(const Builder& b, float side, double station) {
  const float relative = static_cast<float>(station - static_cast<double>(b.sector) * 64);
  const float authored = farm_edge(side, relative);
  if (side < 0)
    return authored;
  const auto road = scenery_road(b.query.theme, b.query.seed, station);
  return std::min(authored, road.center - road.half_width - .3F);
}
void fence_run(Builder& b, float side, double start, double finish) {
  const int count = static_cast<int>(std::ceil((finish - start) / 1.9));
  const auto x_at = [&](double s) {
    // Short outward returns frame a group rather than outlining a corridor.
    return side < 0 ? std::lerp(-7.2F, -10.3F, static_cast<float>((s - start) / (finish - start))) : edge(b, side, s);
  };
  for (int i = 0; i <= count; ++i) {
    const double s = std::lerp(start, finish, static_cast<double>(i) / count);
    const float x = x_at(s);
    if (std::abs(x) < 6.6F)
      continue;
    const unsigned v = composition(b.query.seed, b.sector);
    const double barn = static_cast<double>(b.sector) * 64 + (v == 0 ? 19 : v == 1 ? -11
                                                                                   : 19);
    const bool apron = side < 0 && v != 2 && s > barn - 4 && s < barn + 4;
    if (!apron)
      b.box(x, s, .54F, {.075F, .54F, .075F}, {.42F, .18F, .065F, 1});
    if (i == count)
      break;
    const double next = std::lerp(start, finish, static_cast<double>(i + 1) / count);
    const float nx = x_at(next);
    if (std::abs(nx) < 6.6F)
      continue;
    const float ds = static_cast<float>(next - s), dx = nx - x;
    if (side < 0 && v != 2 && next > barn - 4 && s < barn + 4)
      continue;
    for (float h : {.4F, .83F})
      b.box((x + nx) / 2, (s + next) / 2, h, {.052F, .048F, std::sqrt(ds * ds + dx * dx) / 2}, {.52F, .24F, .085F, 1}, -std::atan2(dx, ds));
  }
}
enum class Bank { pocket,
  shoulder,
  dominant };
struct BankProfile {
  unsigned count, large_count;
  float inner, scale;
  std::array<float, 7> stations;
  unsigned understory_count;
};
constexpr std::array<BankProfile, 3> profiles{{{3, 2, 6.8F, 1.55F, {-2.5F, 0, 2.5F}, 1},
    {5, 4, 5.7F, 1.75F, {-4.5F, -1.5F, 0, 2, 4.5F}, 2},
    {7, 5, 4.8F, 1.85F, {-6, -2, 0, 2, 5, -5, 6}, 3}}};
struct TreeMass {
  float side, station;
  Bank bank;
};
constexpr std::array<std::array<TreeMass, 6>, 3> masses{{{{{-1, 29, Bank::pocket}, {-1, -4, Bank::dominant}, {-1, -27, Bank::shoulder},
                                                             {1, 30, Bank::pocket}, {1, 6, Bank::shoulder}, {1, -27, Bank::dominant}}},
    {{{-1, 29, Bank::shoulder}, {-1, 8, Bank::pocket}, {-1, -27, Bank::dominant},
        {1, 24, Bank::shoulder}, {1, -3, Bank::dominant}, {1, -31, Bank::pocket}}},
    {{{-1, 28, Bank::pocket}, {-1, -7, Bank::dominant}, {-1, -29, Bank::shoulder},
        {1, 25, Bank::shoulder}, {1, 4, Bank::pocket}, {1, -28, Bank::dominant}}}}};
float ground_x(const Builder& b, float side, double station, float desired, float radius_x, float radius_z, float minimum) {
  minimum = std::max(minimum, 6.55F + radius_x);
  float x = std::max(desired, minimum);
  if (side > 0) {
    float left = 100, right = -100;
    for (const float offset : {-radius_z, 0.F, radius_z}) {
      const auto road = scenery_road(b.query.theme, b.query.seed, station + offset);
      left = std::min(left, road.center - road.half_width - .1F);
      right = std::max(right, road.center + road.half_width + .1F);
    }
    if (x + radius_x > left && x - radius_x < right)
      x = left - radius_x >= minimum ? left - radius_x : right + radius_x;
  }
  return side * x;
}
void farmland_understory(Builder& b, float side, double station, float center) {
  const float angle = random(b.query.seed, static_cast<std::int64_t>(b.host), 10) * 6.28F;
  struct Part {
    std::string_view mesh;
    float outward, along, scale, yaw, half_x, half_z;
  };
  // Same six source props/scales as Builder::understory. Bring their fronts
  // into the 17m view individually; never use an invisible cluster centre as
  // evidence of safe or visible placement. Bounds include the entire low mesh.
  constexpr std::array<Part, 6> parts{{{"bush-low", .5F, .8F, .65F, 0, .53F, .50F},
      {"field-rock-a", -.45F, .5F, .55F, 1, .33F, .32F},
      {"grass-tuft", .25F, -.6F, .8F, 0, .13F, .16F},
      {"bush-low", -.9F, .7F, .75F, 2, .53F, .50F},
      {"field-rock-b", .1F, -1, .6F, 1, .30F, .27F},
      {"grass-tuft", -.5F, -.5F, .7F, 2, .13F, .16F}}};
  for (const auto& part : parts) {
    const float yaw = angle + part.yaw, c = std::abs(std::cos(yaw)), sn = std::abs(std::sin(yaw));
    const float rx = (part.half_x * c + part.half_z * sn) * part.scale;
    const float rz = (part.half_x * sn + part.half_z * c) * part.scale;
    const double s = station + part.along * .95;
    const float x = ground_x(b, side, s, center + part.outward * .95F, rx, rz, 0);
    b.prop(part.mesh, x, s, part.scale, yaw);
  }
}
void grove(Builder& b, const TreeMass& mass, double s, std::uint64_t id) {
  const auto& profile = profiles[static_cast<unsigned>(mass.bank)];
  const float side = mass.side;
  b.site(SiteKind::grove, side * (profile.inner + 3), s, 10.F, id);
  constexpr std::array<float, 7> depth{1.2F, 0, 2.8F, .8F, 1.3F, 2.8F, 2.5F};
  constexpr std::array<float, 7> scales{.9F, 1, 1.02F, .94F, .86F, .83F, .8F};
  constexpr std::array<float, 7> yaws{.3F, 1.4F, 3.1F, 2.2F, .8F, .8F, 1.4F};
  for (unsigned i = 0; i < profile.count; ++i) {
    const bool large = i < profile.large_count;
    const std::string_view mesh = large ? "tree-round-large" : "tree-round-small";
    const float scale = profile.scale * (large ? scales[i] : std::min(scales[i], .83F));
    const float yaw = yaws[i];
    const double station = s + profile.stations[i];
    // Full-crown clearance produced slivers in the actual 17m native view.
    // Keep opaque lower roots clear; permit bounded tall overhang under the
    // product's shared projected player/threat/warning occlusion protection.
    const float crown_radius = ((large ? 1.44F : 1.09F) * std::abs(std::cos(yaw)) + (large ? 1.52F : .84F) * std::abs(std::sin(yaw))) * scale;
    const float inner = profile.inner + crown_radius;
    // Triangle-clipped source Y=0..0.6m envelopes, rounded outwards. Current
    // scales are >=1, so this contains the world-space lower 0.6m too.
    const float c = std::abs(std::cos(yaw)), sn = std::abs(std::sin(yaw));
    const float rx = ((large ? .64F : .87F) * c + (large ? .61F : .62F) * sn) * scale;
    const float rz = ((large ? .64F : .87F) * sn + (large ? .61F : .62F) * c) * scale;
    const float px = ground_x(b, side, station, inner + depth[i], rx, rz, inner);
    b.prop(mesh, px, station, scale, yaw, 0, {.87F, .94F, .82F, 1});
  }
  for (unsigned i = 0; i < profile.understory_count; ++i) {
    const double offset = profile.understory_count == 1 ? 0 : profile.understory_count == 2 ? (i == 0 ? -2.5 : 2.5)
                                                                                            : (static_cast<double>(i) - 1) * 4;
    const double station = s + offset;
    const float center = 7.65F + static_cast<float>(3 - profile.understory_count) * .15F;
    farmland_understory(b, side, station, center);
  }
}

} // namespace
void farmland_sector(Builder& b) {
  const double center = static_cast<double>(b.sector) * 64;
  const unsigned variant = composition(b.query.seed, b.sector);
  const float barn_station = variant == 0 ? 19.F : -11.F;
  const float silo_station = variant == 0 ? -16.F : variant == 1 ? 18.F
                                                                 : -18.F;
  const float windmill_station = variant == 1 ? 11.F : -13.F;
  const float crop_station = variant == 0 ? 20.F : variant == 1 ? -21.F
                                                                : 8.F;
  const auto dominant = std::find_if(masses[variant].begin(), masses[variant].end(), [](const auto& mass) {
    return mass.side < 0 && mass.bank == Bank::dominant;
  })->station;
  // Ends disappear into groups; diagonal returns and unequal gaps avoid the
  // two parallel fence strips of the earlier arrangement.
  b.site(SiteKind::garden, -9, center, 0, 120);
  fence_run(b, -1, center + dominant - 7, center + dominant - 3);
  fence_run(b, -1, center + (variant == 2 ? crop_station + 7 : barn_station - 10), center + (variant == 2 ? crop_station + 11 : barn_station - 6));
  b.site(SiteKind::garden, 9, center, 0, 121);
  fence_run(b, 1, center + windmill_station - 8, center + windmill_station - 4);
  fence_run(b, 1, center + crop_station + 4, center + crop_station + 9);
  if (variant != 2) {
    const double s = center + barn_station;
    b.site(SiteKind::farmyard, -9.4F, s, 3.15F, 130);
    b.patch("ground-dirt", -9.4F, s, 3.1F, 4.3F, 0, .006F);
    b.prop("barn", -9.4F, s, 1.05F, pi / 2);
    b.prop("hay-bale", -7.8F, s - 3.2, .95F, .3F);
    b.prop("hay-bale", -9.F, s - 3.5, .85F, 1.65F);
    farmland_understory(b, -1, s - 4.2, 7.95F);
  }
  const double silo = center + silo_station;
  b.site(SiteKind::farmyard, -9.5F, silo, 1.2F, 131);
  b.prop("silo", -9.5F, silo, .9F);
  const double windmill = center + windmill_station;
  b.site(SiteKind::farmyard, 8.5F, windmill, 1, 132);
  b.prop("windmill-mast", 8.5F, windmill, .95F);
  b.prop("windmill-rotor", 8.5F, windmill - .2, .95F, 0, 2.98F);
  // Crop block is an authored corner mass; the reference does not alternate
  // identical crop rectangles down both sides of a narrow corridor.
  const double cs = center + crop_station;
  float crop_x = variant == 2 ? -8.6F : 7.8F;
  if (crop_x > 0)
    for (double offset = -4; offset <= 4; offset += .5) {
      const auto road = scenery_road(b.query.theme, b.query.seed, cs + offset);
      crop_x = std::min(crop_x, road.center - road.half_width - 2.F);
    }
  b.site(SiteKind::crops, crop_x, cs, 3.25F, 140);
  for (int column = 0; column < 5; ++column) {
    const float x = crop_x + static_cast<float>(column) * .66F - 1;
    b.patch("ground-soil", x, cs, .24F, 3.8F, 0, .004F, {.88F, .9F, .84F, 1});
    for (int row = 0; row < 10; ++row)
      b.prop("crop-seedling", x, cs - 3.4 + static_cast<double>(row) * .75, 1.05F);
  }
  unsigned index = 0;
  for (const auto& mass : masses[variant]) {
    const auto slot = index++;
    grove(b, mass, center + mass.station, 150 + slot);
  }
  unsigned bale = 0;
  for (const auto p : std::array<std::array<float, 2>, 5>{{{7.3F, windmill_station - 2}, {7.3F, windmill_station + 2},
           {variant == 2 ? -7.3F : 7.3F, crop_station - 4.7F}, {-7.3F, silo_station}, {-7.3F, dominant - 4}}}) {
    float x = p[0];
    if (x > 0) {
      const auto road = scenery_road(b.query.theme, b.query.seed, center + p[1]);
      if (x + .7F > road.center - road.half_width && x - .7F < road.center + road.half_width) {
        const float inside = road.center - road.half_width - .8F;
        x = inside >= 7.2F ? inside : road.center + road.half_width + .8F;
      }
    }
    b.site(SiteKind::garden, x, center + p[1], .7F, 180 + bale++);
    b.prop("hay-bale", x, center + p[1], .95F, .5F * static_cast<float>(bale));
  }
  // Flat, quiet field trace at the reference's upper-left composition anchor.
  const double circle = center + (variant == 0 ? 19.5 : variant == 1 ? 3
                                                                     : -13);
  const float circle_x = variant == 2 ? 2.6F : -3.5F;
  b.site(SiteKind::garden, circle_x, circle, 0, 190);
  if (std::abs(b.local(circle)) <= 51)
    for (float r : {1.5F, 2.F, 2.65F}) {
      b.prop("ring", circle_x, circle, 1, 0, .003F, {.16F, .19F, .065F, 1});
      auto& i = b.generation.scene.instances.back();
      i.scale = {r, 1, r};
      i.casts_shadow = false;
    }
}
} // namespace human_resources::presentation::scenery_detail
