#pragma once
#include "scenery_internal.h"
#include <span>
namespace human_resources::presentation::scenery_detail {
struct RoadKey final {
  float station, center, half_width;
};
struct FrontageAnchor final {
  float side, station;
};
[[nodiscard]] unsigned composition(std::uint64_t seed, std::int64_t section);
[[nodiscard]] std::span<const RoadKey> road_keys(SceneryTheme theme, unsigned composition);
[[nodiscard]] std::span<const FrontageAnchor> homes(unsigned composition);
[[nodiscard]] float farm_edge(float side, float relative);
} // namespace human_resources::presentation::scenery_detail
