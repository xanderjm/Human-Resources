#pragma once
#include <algorithm>
#include <charconv>
#include <cstdint>
#include <optional>
#include <string_view>
#include <system_error>

namespace human_resources::renderer {
// Explicit RGBA capture pixels are independent of logical window points and
// the capped world target. At most 32 MiB each for composition and readback.
struct CaptureExtent final {
  unsigned width{}, height{};
  [[nodiscard]] constexpr bool empty() const { return width == 0 && height == 0; }
  [[nodiscard]] constexpr bool valid() const {
    return width > 0 && height > 0 && width <= 4096 && height <= 4096 &&
        static_cast<std::uint64_t>(width) * height <= 8U * 1024U * 1024U;
  }
  // One output pixel of rounding is allowed; never stretch a different layout.
  [[nodiscard]] constexpr bool matches_aspect(unsigned window_width, unsigned window_height) const {
    if (!valid() || !window_width || !window_height)
      return false;
    const auto a = static_cast<std::uint64_t>(width) * window_height;
    const auto b = static_cast<std::uint64_t>(height) * window_width;
    return (a > b ? a - b : b - a) <= std::max(window_width, window_height);
  }
};

[[nodiscard]] inline std::optional<CaptureExtent> parse_capture_extent(std::string_view value) {
  const auto separator = value.find('x');
  if (separator == std::string_view::npos)
    return std::nullopt;
  CaptureExtent extent;
  const auto width = std::from_chars(value.data(), value.data() + separator, extent.width);
  const auto height = std::from_chars(value.data() + separator + 1, value.data() + value.size(), extent.height);
  if (width.ec != std::errc{} || width.ptr != value.data() + separator ||
      height.ec != std::errc{} || height.ptr != value.data() + value.size() || !extent.valid())
    return std::nullopt;
  return extent;
}
} // namespace human_resources::renderer
