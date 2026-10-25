#pragma once
#include <cstdint>
#include <human_resources/presentation/style_scene.h>
#include <span>

namespace human_resources::presentation {
enum class SceneryTheme : std::uint8_t { farmland,
  suburbia,
  desert };
enum class SiteKind : std::uint8_t { farmyard,
  crops,
  grove,
  home,
  garden,
  outcrop,
  cactus,
  service_stop };
struct SceneryQuery final {
  SceneryTheme theme{SceneryTheme::farmland};
  std::uint64_t seed{514};
  double distance{}; // metres travelled; absolute world coordinates stay double
  float aspect{9.F / 16};
};
struct RoadSection final {
  float center{}, half_width{}, slope{};
};
struct ScenerySite final {
  std::uint64_t id{};
  std::int64_t sector{};
  SiteKind kind{};
  float x{};
  double along{};
  float radius{}; // core footprint; understory may intentionally overlap its host
};
struct SceneryGeneration final {
  StyleFrame scene;
  std::vector<ScenerySite> sites;
  std::int64_t first_sector{}, last_sector{};
};
// Pure, bounded window generation. Regenerating/visiting in another order does
// not change a site's identity or placement. No finite film clock or wrap.
[[nodiscard]] SceneryGeneration generate_scenery(const SceneryQuery& query);
[[nodiscard]] RoadSection scenery_road(SceneryTheme theme, std::uint64_t seed, double along);
[[nodiscard]] std::span<const std::string_view> scenery_materials();
[[nodiscard]] std::span<const std::string_view> scenery_materials(SceneryTheme theme);
} // namespace human_resources::presentation
