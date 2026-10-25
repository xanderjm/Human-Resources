#include <human_resources/presentation/scenery.h>

#include "scenery_compositions.h"
#include "scenery_internal.h"
#include <algorithm>
#include <cmath>
#include <utility>

namespace human_resources::presentation::scenery_detail {
std::uint64_t key(std::uint64_t seed, std::int64_t index, std::uint64_t channel) {
  auto x = seed ^ (static_cast<std::uint64_t>(index) * 0x9e3779b97f4a7c15ULL) ^ (channel * 0xd1b54a32d192ed03ULL);
  x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
  x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
  return x ^ (x >> 31);
}
float random(std::uint64_t seed, std::int64_t index, std::uint64_t channel) {
  return static_cast<float>(key(seed, index, channel) >> 40) / 16777216.F;
}
Builder::Builder(SceneryQuery value) : query(value) {
  uv_origin = std::floor((query.distance - 64) / 480) * 480;
  generation.scene.location = static_cast<unsigned>(query.theme);
  generation.scene.show_overlay = false;
  generation.scene.instances.reserve(700);
  generation.sites.reserve(70);
  generation.scene.surfaces.reserve(12);
}
float Builder::local(double along) const {
  return static_cast<float>(query.distance - along);
}
std::uint64_t Builder::site(SiteKind kind, float x, double along, float radius, std::uint64_t channel) {
  host = key(query.seed, sector, channel + static_cast<unsigned>(query.theme) * 4096U);
  part = 0;
  generation.sites.push_back({host, sector, kind, x, along, radius});
  return host;
}
void Builder::prop(std::string_view mesh, float x, double along, float scale, float yaw, float y, Color tint) {
  const auto id = key(host, static_cast<std::int64_t>(++part), 71);
  const float z = local(along);
  if (z < -51 || z > 51)
    return;
  generation.scene.instances.push_back({.mesh = mesh, .position = {x, y, z}, .scale = {scale, scale, scale}, .rotation = {0, yaw, 0}, .tint = tint, .texture_detail = (mesh == "tree-round-large" || mesh == "tree-round-small") ? .72F : 1.F, .stable_id = id});
}
void Builder::box(float x, double along, float y, Vec3 half, Color tint, float yaw) {
  const auto previous = generation.scene.instances.size();
  prop("cube", x, along, 1, yaw, y, tint);
  if (generation.scene.instances.size() == previous)
    return;
  auto& i = generation.scene.instances.back();
  i.scale = half;
}
StyleSurface& Builder::surface(std::string_view material, Color tint, float detail) {
  for (auto& s : generation.scene.surfaces)
    if (s.material == material && s.tint == tint && s.texture_detail == detail)
      return s;
  generation.scene.surfaces.push_back({.material = material, .tint = tint, .texture_detail = detail});
  return generation.scene.surfaces.back();
}
void Builder::quad(StyleSurface& s, std::array<Vec3, 4> p, std::array<std::array<float, 2>, 4> uv) {
  for (unsigned i : {0U, 1U, 2U, 0U, 2U, 3U})
    s.vertices.push_back({p[i], {0, 1, 0}, uv[i]});
}
void Builder::patch(std::string_view material, float x, double along, float hx, float hz, float yaw, float height, Color tint) {
  const float c = std::cos(yaw), sn = std::sin(yaw);
  std::array<Vec3, 4> p{};
  std::array<std::array<float, 2>, 4> uv{};
  unsigned i = 0;
  for (const auto d : std::array<std::array<float, 2>, 4>{{{-hx, -hz}, {-hx, hz}, {hx, hz}, {hx, -hz}}}) {
    const float px = x + c * d[0] + sn * d[1];
    const double abs_s = along + static_cast<double>(sn * d[0] - c * d[1]);
    p[i] = {px, height, local(abs_s)};
    uv[i] = {px / 5, static_cast<float>((abs_s - uv_origin) / 5)};
    ++i;
  }
  quad(surface(material, tint), p, uv);
}
void Builder::path(std::string_view material, float x0, double s0, float x1, double s1, float width, float height) {
  const float dx = x1 - x0, ds = static_cast<float>(s1 - s0);
  const float length = std::sqrt(dx * dx + ds * ds);
  if (length < .01F)
    return;
  const float nx = ds / length * width / 2, ns = -dx / length * width / 2;
  std::array<Vec3, 4> p{{{x0 - nx, height, local(s0 - ns)}, {x0 + nx, height, local(s0 + ns)}, {x1 + nx, height, local(s1 + ns)}, {x1 - nx, height, local(s1 - ns)}}};
  std::array<std::array<float, 2>, 4> uv{};
  for (unsigned i = 0; i < 4; ++i)
    uv[i] = {p[i].x / 5, static_cast<float>((query.distance - static_cast<double>(p[i].z) - uv_origin) / 5)};
  quad(surface(material), p, uv);
}
void Builder::understory(float x, double s, bool desert, float spread) {
  const float a = random(query.seed, static_cast<std::int64_t>(host), 10) * 6.28F;
  prop(desert ? "desert-scrub" : "bush-low", x + .5F * spread, s + .8 * spread, .65F, a);
  prop(desert ? "desert-boulder-b" : "field-rock-a", x - .45F * spread, s + .5 * spread, .55F, a + 1);
  if (!desert) {
    prop("grass-tuft", x + .25F * spread, s - .6 * spread, .8F, a);
    prop("bush-low", x - .9F * spread, s + .7 * spread, .75F, a + 2);
    prop("field-rock-b", x + .1F * spread, s - spread, .6F, a + 1);
    prop("grass-tuft", x - .5F * spread, s - .5 * spread, .7F, a + 2);
  }
}
} // namespace human_resources::presentation::scenery_detail

namespace human_resources::presentation {
namespace {
// Shape-preserving Hermite tangents keep authored bends smooth without
// overshooting narrow garden pockets. End ports have a shared zero tangent.
float road_tangent(std::span<const scenery_detail::RoadKey> keys, unsigned i, bool width) {
  if (i == 0 || i + 1 == keys.size())
    return 0;
  const auto value = [width](const auto& k) { return width ? k.half_width : k.center; };
  const float before = keys[i].station - keys[i - 1].station;
  const float after = keys[i + 1].station - keys[i].station;
  const float a = (value(keys[i]) - value(keys[i - 1])) / before;
  const float b = (value(keys[i + 1]) - value(keys[i])) / after;
  if (a * b <= 0)
    return 0;
  const float wa = 2 * after + before, wb = after + 2 * before;
  return (wa + wb) / (wa / a + wb / b);
}
} // namespace
RoadSection scenery_road(SceneryTheme theme, std::uint64_t seed, double s) {
  using namespace scenery_detail;
  if (!std::isfinite(s) || std::abs(s) > 1.e12)
    s = 0;
  const auto section = static_cast<std::int64_t>(std::floor((s + 32) / 64));
  const float relative = static_cast<float>(s - static_cast<double>(section) * 64);
  const auto keys = road_keys(theme, composition(seed, section));
  unsigned index = 0;
  while (index + 2 < keys.size() && relative > keys[index + 1].station)
    ++index;
  const auto a = keys[index], b = keys[index + 1];
  const float length = b.station - a.station;
  const float t = (relative - a.station) / length;
  const float t2 = t * t, t3 = t2 * t;
  const auto interpolate = [&](float x, float y, bool width) {
    const float ma = road_tangent(keys, index, width), mb = road_tangent(keys, index + 1, width);
    const float value = (2 * t3 - 3 * t2 + 1) * x + (t3 - 2 * t2 + t) * length * ma + (-2 * t3 + 3 * t2) * y + (t3 - t2) * length * mb;
    const float slope = ((6 * t2 - 6 * t) * x + (3 * t2 - 4 * t + 1) * length * ma + (-6 * t2 + 6 * t) * y + (3 * t2 - 2 * t) * length * mb) / length;
    return std::pair{value, slope};
  };
  const auto [center, slope] = interpolate(a.center, b.center, false);
  const auto [half, unused] = interpolate(a.half_width, b.half_width, true);
  return {center, half, slope};
}
std::span<const std::string_view> scenery_materials() {
  using namespace std::string_view_literals;
  static constexpr std::array names{"ground-grass"sv, "ground-dirt"sv, "ground-soil"sv, "ground-asphalt"sv, "ground-pavement"sv, "ground-sand"sv, "plane"sv, "cube"sv, "tree-round-large"sv, "tree-round-small"sv, "bush-low"sv, "field-rock-a"sv, "field-rock-b"sv, "grass-tuft"sv, "crop-seedling"sv, "barn"sv, "silo"sv, "hay-bale"sv, "windmill-mast"sv, "windmill-rotor"sv, "house-coral"sv, "house-teal"sv, "streetlamp"sv, "mailbox-red"sv, "mailbox-teal"sv, "wheelie-bin"sv, "hedge-module"sv, "cactus-tall"sv, "cactus-short"sv, "cliff-module-a"sv, "cliff-module-b"sv, "desert-boulder-a"sv, "desert-boulder-b"sv, "desert-scrub"sv, "service-canopy"sv, "fuel-pump-red"sv, "fuel-pump-blue"sv, "road-bollard"sv, "ring"sv};
  return names;
}
std::span<const std::string_view> scenery_materials(SceneryTheme theme) {
  using namespace std::string_view_literals;
  static constexpr std::array farm{"ground-grass"sv, "ground-dirt"sv, "ground-soil"sv, "cube"sv, "tree-round-large"sv, "tree-round-small"sv, "bush-low"sv, "field-rock-a"sv, "field-rock-b"sv, "grass-tuft"sv, "crop-seedling"sv, "barn"sv, "silo"sv, "hay-bale"sv, "windmill-mast"sv, "windmill-rotor"sv, "ring"sv};
  static constexpr std::array suburb{"ground-grass"sv, "ground-pavement"sv, "ground-asphalt"sv, "plane"sv, "tree-round-large"sv, "tree-round-small"sv, "bush-low"sv, "field-rock-a"sv, "field-rock-b"sv, "grass-tuft"sv, "house-coral"sv, "house-teal"sv, "mailbox-red"sv, "mailbox-teal"sv, "wheelie-bin"sv, "hedge-module"sv, "streetlamp"sv};
  static constexpr std::array desert{"ground-sand"sv, "ground-dirt"sv, "ground-asphalt"sv, "ground-pavement"sv, "plane"sv, "cliff-module-a"sv, "cliff-module-b"sv, "cactus-tall"sv, "cactus-short"sv, "desert-boulder-a"sv, "desert-boulder-b"sv, "desert-scrub"sv, "service-canopy"sv, "fuel-pump-red"sv, "fuel-pump-blue"sv, "road-bollard"sv};
  if (theme == SceneryTheme::farmland)
    return farm;
  if (theme == SceneryTheme::suburbia)
    return suburb;
  return desert;
}
SceneryGeneration generate_scenery(const SceneryQuery& input) {
  using namespace scenery_detail;
  auto q = input;
  if (!std::isfinite(q.distance) || std::abs(q.distance) > 1.e12)
    q.distance = 0;
  if (!std::isfinite(q.aspect) || q.aspect < .4F || q.aspect > 3)
    q.aspect = 9.F / 16;
  if (static_cast<unsigned>(q.theme) > 2)
    q.theme = SceneryTheme::farmland;
  if (!std::isfinite(q.play_clear_half_width))
    q.play_clear_half_width = 6.55F;
  q.play_clear_half_width = std::clamp(q.play_clear_half_width, 6.55F, 12.F);
  Builder b(q);
  set_overhead_camera(b.generation.scene, q.aspect, std::clamp(20.5F * q.aspect, 20.5F, 32.F));
  roads(b);
  b.generation.first_sector = static_cast<std::int64_t>(std::floor((q.distance - 52 + 32) / sector_length));
  b.generation.last_sector = static_cast<std::int64_t>(std::floor((q.distance + 52 + 32) / sector_length));
  for (auto n = b.generation.first_sector; n <= b.generation.last_sector; ++n) {
    b.sector = n;
    if (q.theme == SceneryTheme::farmland)
      farmland_sector(b);
    else if (q.theme == SceneryTheme::suburbia)
      suburb_sector(b);
    else
      desert_sector(b);
  }
  // Widen the same authored groups and their ground patches together. The
  // continuous map stretches placement through the clearing, then translates
  // outer groups rigidly; source mesh scale, UVs and identity stay unchanged.
  const float extra = q.play_clear_half_width - 6.55F;
  const auto x = [extra](float value) { return value + extra * std::clamp(value / 6.55F, -1.F, 1.F); };
  if (extra > 0) {
    for (auto& instance : b.generation.scene.instances)
      instance.position.x = x(instance.position.x);
    for (auto& surface : b.generation.scene.surfaces)
      for (auto& vertex : surface.vertices)
        vertex.position.x = x(vertex.position.x);
    for (auto& site : b.generation.sites)
      site.x = x(site.x);
  }
  return std::move(b.generation);
}
} // namespace human_resources::presentation
