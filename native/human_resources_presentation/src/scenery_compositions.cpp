#include "scenery_compositions.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace human_resources::presentation::scenery_detail {
unsigned composition(std::uint64_t seed, std::int64_t section) {
  // Each three-section group visits all compositions. Its last choice is
  // independent, so the next group can avoid it without replaying any history.
  const auto group = static_cast<std::int64_t>(std::floor(static_cast<double>(section) / 3));
  const auto slot = static_cast<unsigned>(section - group * 3);
  const auto last = [seed](std::int64_t n) {
    if (n == 0)
      return 2U;
    const auto value = key(seed, n, 502);
    return n == -1 ? 1U + static_cast<unsigned>(value % 2) : static_cast<unsigned>(value % 3);
  };
  if (group == 0)
    return slot; // Fixed comparison fixture and its two distinct neighbours.
  const unsigned end = last(group), previous = last(group - 1);
  unsigned first = (end + 1U + static_cast<unsigned>(key(seed, group, 503) % 2)) % 3;
  if (first == previous)
    first = 3U - end - first;
  return slot == 0 ? first : slot == 1 ? 3U - end - first
                                       : end;
}
std::span<const RoadKey> road_keys(SceneryTheme theme, unsigned variant) {
  // Stations are authored in metres around a reference composition's centre.
  // Street edges describe rooms/coves, rather than a constant-width race track.
  static constexpr std::array<RoadKey, 11> farm0{{{-32, 10, 1.15F}, {-25, 8.1F, 1.15F}, {-18, 10.7F, 1.15F}, {-10, 11.9F, 1.15F}, {-3, 10.4F, 1.15F}, {4, 11.8F, 1.15F}, {12, 12.5F, 1.15F}, {19, 12.8F, 1.15F}, {24, 13, 1.15F}, {28, 11.5F, 1.15F}, {32, 10, 1.15F}}};
  static constexpr std::array<RoadKey, 9> farm1{{{-32, 10, 1.15F}, {-24, 11.2F, 1.15F}, {-16, 10.9F, 1.15F}, {-8, 9.2F, 1.15F}, {0, 9.4F, 1.15F}, {8, 10.7F, 1.15F}, {16, 11.4F, 1.15F}, {24, 10.6F, 1.15F}, {32, 10, 1.15F}}};
  static constexpr std::array<RoadKey, 9> farm2{{{-32, 10, 1.15F}, {-24, 9.2F, 1.15F}, {-16, 9.8F, 1.15F}, {-8, 11.4F, 1.15F}, {0, 11.1F, 1.15F}, {8, 10.2F, 1.15F}, {16, 9.1F, 1.15F}, {24, 9.4F, 1.15F}, {32, 10, 1.15F}}};
  // Reference road edges traced at the fixed 20.5m portrait camera. The
  // narrower fore/background reveal roofs; the central bay reaches offscreen.
  static constexpr std::array<RoadKey, 17> street0{{{-32, 0, 7}, {-26, .2F, 3.5F}, {-22, .25F, 3.6F}, {-18, .1F, 4.8F}, {-14, -.4F, 7.8F}, {-11, -.2F, 6.7F}, {-8, -.05F, 5.4F}, {-5, -.1F, 5.6F}, {0, -.3F, 7.6F}, {5, -.2F, 8.7F}, {9, 0, 10.6F}, {12, .7F, 8.6F}, {16, .5F, 6.6F}, {20, .45F, 5.25F}, {24, 1.2F, 6.5F}, {28, .7F, 7.4F}, {32, 0, 7}}};
  static constexpr std::array<RoadKey, 9> street1{{{-32, 0, 7}, {-24, -1.2F, 5.4F}, {-16, -.5F, 4.9F}, {-8, .4F, 6.9F}, {0, 1.1F, 9.4F}, {8, 1.6F, 6.3F}, {16, .2F, 4.9F}, {24, -.7F, 6.3F}, {32, 0, 7}}};
  static constexpr std::array<RoadKey, 9> street2{{{-32, 0, 7}, {-24, 1.2F, 4.8F}, {-16, .1F, 5.6F}, {-8, -1.4F, 8.4F}, {0, -1.2F, 5.8F}, {8, -.2F, 7.6F}, {16, .9F, 6.5F}, {24, .1F, 4.9F}, {32, 0, 7}}};
  static constexpr std::array<RoadKey, 16> desert0{{{-32, -7.5F, 1.6F}, {-27, -4.1F, 1.6F}, {-24, -3.2F, 1.6F}, {-20, -3.F, 1.6F}, {-16, -4.15F, 1.6F}, {-12, -6.1F, 1.6F}, {-8, -8.4F, 1.6F}, {-4, -10.5F, 1.6F}, {0, -11.9F, 1.6F}, {4, -11.8F, 1.6F}, {8, -10.7F, 1.6F}, {12, -8.4F, 1.6F}, {16, -6.6F, 1.6F}, {20, -6.3F, 1.6F}, {25, -8.1F, 1.6F}, {32, -7.5F, 1.6F}}};
  static constexpr std::array<RoadKey, 9> desert1{{{-32, -7.5F, 1.6F}, {-24, -8.3F, 1.6F}, {-16, -9.1F, 1.6F}, {-8, -7.7F, 1.6F}, {0, -5.6F, 1.6F}, {8, -6.8F, 1.6F}, {16, -8.8F, 1.6F}, {24, -8.1F, 1.6F}, {32, -7.5F, 1.6F}}};
  static constexpr std::array<RoadKey, 9> desert2{{{-32, -7.5F, 1.6F}, {-24, -6.F, 1.6F}, {-16, -7.6F, 1.6F}, {-8, -8.9F, 1.6F}, {0, -8.4F, 1.6F}, {8, -6.6F, 1.6F}, {16, -5.5F, 1.6F}, {24, -6.5F, 1.6F}, {32, -7.5F, 1.6F}}};
  if (theme == SceneryTheme::farmland)
    return variant == 0 ? std::span<const RoadKey>(farm0) : variant == 1 ? std::span<const RoadKey>(farm1)
                                                                         : std::span<const RoadKey>(farm2);
  if (theme == SceneryTheme::suburbia)
    return variant == 0 ? std::span<const RoadKey>(street0) : variant == 1 ? std::span<const RoadKey>(street1)
                                                                           : std::span<const RoadKey>(street2);
  return variant == 0 ? std::span<const RoadKey>(desert0) : variant == 1 ? std::span<const RoadKey>(desert1)
                                                                         : std::span<const RoadKey>(desert2);
}
std::span<const FrontageAnchor> homes(unsigned variant) {
  static constexpr std::array<FrontageAnchor, 6> a{{{-1, 22}, {-1, -9}, {-1, -25}, {1, 18}, {1, -10}, {1, -27}}};
  static constexpr std::array<FrontageAnchor, 5> b{{{-1, 16}, {-1, -17}, {1, 18}, {1, 3}, {1, -23}}};
  static constexpr std::array<FrontageAnchor, 6> c{{{-1, 23}, {-1, 3}, {-1, -19}, {1, 24}, {1, -3}, {1, -24}}};
  return variant == 0 ? std::span<const FrontageAnchor>(a) : variant == 1 ? std::span<const FrontageAnchor>(b)
                                                                          : std::span<const FrontageAnchor>(c);
}
float farm_edge(float side, float s) {
  static constexpr std::array<float, 9> left{-9.3F, -7.6F, -8.8F, -9.5F, -10.5F, -9.3F, -8.4F, -9.2F, -9.3F};
  static constexpr std::array<float, 9> right{9.0F, 8.5F, 9.0F, 9.4F, 8.5F, 8.9F, 10.2F, 10.1F, 9.0F};
  const float u = std::clamp((s + 32) / 8, 0.F, 7.9999F);
  const auto i = static_cast<unsigned>(u);
  const float t = u - static_cast<float>(i);
  const auto& values = side < 0 ? left : right;
  return std::lerp(values[i], values[i + 1], t * t * (3 - 2 * t));
}
} // namespace human_resources::presentation::scenery_detail
