#pragma once

#include <human_resources/ui/geometry.h>

#include <array>
#include <cstdint>

namespace human_resources::ui {

struct TypographyTokens final {
  float caption{12.0F};
  float body{16.0F};
  float action{17.0F};
  float title{24.0F};
  float display{34.0F};
};

struct MotionTokens final {
  std::uint32_t immediate_ms{};
  std::uint32_t fast_ms{120};
  std::uint32_t standard_ms{220};
  std::uint32_t deliberate_ms{360};
};

struct DesignTokens final {
  std::array<float, 7> spacing{0.0F, 4.0F, 8.0F, 12.0F, 16.0F, 24.0F, 32.0F};
  TypographyTokens typography;
  MotionTokens motion;
  Size minimum_interaction_target{44.0F, 44.0F};
  float corner_radius{8.0F};
  Color canvas{0.025F, 0.035F, 0.075F, 1.0F};
  Color surface{0.09F, 0.12F, 0.20F, 1.0F};
  Color action{0.20F, 0.72F, 0.64F, 1.0F};
  Color action_pressed{0.14F, 0.54F, 0.49F, 1.0F};
  Color text{0.96F, 0.97F, 1.0F, 1.0F};
  Color danger{0.88F, 0.25F, 0.28F, 1.0F};
};

[[nodiscard]] constexpr std::uint32_t effective_motion_duration(const std::uint32_t duration_ms,
    const bool reduced_motion) noexcept {
  return reduced_motion ? 0U : duration_ms;
}

} // namespace human_resources::ui
