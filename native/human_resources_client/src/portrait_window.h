#pragma once
#include <human_resources/presentation/frame_viewport.h>
#include <algorithm>

namespace human_resources::client {
// Native phone orientation is locked by the platform shells. Desktop previews
// remain resizable within portrait phone/tablet proportions.
inline constexpr float maximum_portrait_window_aspect = 3.F / 4.F;
[[nodiscard]] constexpr presentation::PixelExtent portrait_window_extent(presentation::PixelExtent requested) {
  if (requested.width > requested.height) std::swap(requested.width, requested.height);
  requested.width = std::max(320U, requested.width);
  requested.height = std::max(568U, requested.height);
  requested.width = std::min(requested.width, requested.height * 3U / 4U);
  return requested;
}
} // namespace human_resources::client
