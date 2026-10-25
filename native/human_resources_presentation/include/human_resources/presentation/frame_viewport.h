#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace human_resources::presentation {
struct PixelExtent final { unsigned width{}, height{}; };
struct PixelRect final { unsigned x{}, y{}, width{}, height{}; };
// The same integer fit owns the rendered destination and screen attachments.
constexpr PixelExtent fit_extent(unsigned width, unsigned height, unsigned limit_width, unsigned limit_height, bool upscale = false) {
  if (!width || !height || !limit_width || !limit_height) return {};
  if (!upscale && width <= limit_width && height <= limit_height) return {width, height};
  if (static_cast<std::uint64_t>(width) * limit_height > static_cast<std::uint64_t>(height) * limit_width)
    return {limit_width, std::max(1U, static_cast<unsigned>((static_cast<std::uint64_t>(height) * limit_width + width / 2) / width))};
  return {std::max(1U, static_cast<unsigned>((static_cast<std::uint64_t>(width) * limit_height + height / 2) / height)), limit_height};
}
inline PixelRect fit_world_destination(unsigned source_width, unsigned source_height, unsigned target_width, unsigned target_height,
    std::array<float, 4> available = {0, 0, 1, 1}) {
  if (!target_width || !target_height) return {};
  for (float value : available) if (!std::isfinite(value)) return {};
  // Clip available edges once, then round to the nearest target pixel. Default
  // full-frame fitting is exactly the previous renderer's integer centering.
  const auto edge = [](double value, unsigned extent) { return static_cast<unsigned>(std::round(std::clamp(value, 0., 1.) * static_cast<double>(extent))); };
  const unsigned left = edge(available[0], target_width), top = edge(available[1], target_height);
  const unsigned right = edge(static_cast<double>(available[0]) + available[2], target_width), bottom = edge(static_cast<double>(available[1]) + available[3], target_height);
  if (right <= left || bottom <= top) return {};
  const auto fitted = fit_extent(source_width, source_height, right - left, bottom - top, true);
  return {left + (right - left - fitted.width) / 2, top + (bottom - top - fitted.height) / 2, fitted.width, fitted.height};
}
} // namespace human_resources::presentation
