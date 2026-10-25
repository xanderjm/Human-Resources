#pragma once
#include <human_resources/presentation/frame_viewport.h>

namespace human_resources::renderer {
struct PreviewExtent final {
  unsigned width{}, height{};
};
// Fit pixels without changing aspect; rounding introduces at most one pixel.
constexpr PreviewExtent fit_preview_extent(unsigned width, unsigned height, unsigned limit_width, unsigned limit_height, bool upscale = false) {
  const auto fitted = presentation::fit_extent(width, height, limit_width, limit_height, upscale);
  return {fitted.width, fitted.height};
}
} // namespace human_resources::renderer
