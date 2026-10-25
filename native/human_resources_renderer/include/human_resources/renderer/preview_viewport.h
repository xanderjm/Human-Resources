#pragma once
#include <algorithm>
#include <cstdint>

namespace human_resources::renderer {
struct PreviewExtent final {
  unsigned width{}, height{};
};
// Fit pixels without changing aspect; rounding introduces at most one pixel.
constexpr PreviewExtent fit_preview_extent(unsigned width, unsigned height, unsigned limit_width, unsigned limit_height, bool upscale = false) {
  if (!width || !height || !limit_width || !limit_height)
    return {};
  if (!upscale && width <= limit_width && height <= limit_height)
    return {width, height};
  if (static_cast<std::uint64_t>(width) * limit_height > static_cast<std::uint64_t>(height) * limit_width)
    return {limit_width, std::max(1U, static_cast<unsigned>((static_cast<std::uint64_t>(height) * limit_width + width / 2) / width))};
  return {std::max(1U, static_cast<unsigned>((static_cast<std::uint64_t>(width) * limit_height + height / 2) / height)), limit_height};
}
} // namespace human_resources::renderer
