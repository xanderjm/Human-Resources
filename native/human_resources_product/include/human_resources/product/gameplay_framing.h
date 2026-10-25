#pragma once
#include <human_resources/presentation/frame_viewport.h>
#include <human_resources/presentation/style_scene.h>
#include <human_resources/ui/geometry.h>

namespace human_resources::product {
enum class FramingContext { flight, attract };
// Disposable display data; field rules and semantic input remain unchanged.
struct GameplayFraming final {
  GameplayFraming() = default;
  static constexpr float tactical_row_height = 44.F;
  static constexpr float tactical_bottom_margin = 12.F;
  FramingContext context{FramingContext::flight};
  ui::Rect viewport, safe, stage, run_layout, left_rail, right_rail;
  presentation::PixelExtent render_extent;
  std::array<float, 4> destination{0, 0, 1, 1};
  presentation::Matrix camera{};
  float aspect{}, camera_width{17.F / .75F}, logical_pixel{};
  bool constrained{}, compact{}, banded{};
};
[[nodiscard]] GameplayFraming gameplay_framing(ui::Rect viewport, ui::Insets safe_area = {},
    float text_scale = 1, presentation::PixelExtent output_pixels = {}, FramingContext context = FramingContext::flight);
[[nodiscard]] GameplayFraming gameplay_framing(float aspect, FramingContext context = FramingContext::flight);
} // namespace human_resources::product
