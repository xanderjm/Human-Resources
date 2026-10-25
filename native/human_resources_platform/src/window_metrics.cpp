#include <human_resources/platform/sdl_platform.h>

#include <algorithm>
#include <cmath>

namespace human_resources::platform {

WindowMetrics normalize_window_metrics(const human_resources::ui::Size window_size,
    const human_resources::ui::Insets window_safe_area, const float pixel_density,
    const float display_scale) noexcept {
  WindowMetrics metrics;
  if (!std::isfinite(window_size.width) || !std::isfinite(window_size.height) ||
      window_size.width <= 0.0F || window_size.height <= 0.0F ||
      !std::isfinite(pixel_density) || pixel_density <= 0.0F ||
      !std::isfinite(display_scale) || display_scale <= 0.0F)
    return metrics;
  const float scale = display_scale / pixel_density;
  if (!std::isfinite(scale) || scale <= 0.0F)
    return metrics;
  metrics.window_units_per_point = scale;
  metrics.display_scale = display_scale;
  metrics.viewport = {.width = window_size.width / scale,
      .height = window_size.height / scale};
  metrics.safe_area = {window_safe_area.left / scale, window_safe_area.top / scale,
      window_safe_area.right / scale, window_safe_area.bottom / scale};
  metrics.valid = metrics.viewport.finite() &&
                  std::isfinite(metrics.safe_area.left) &&
                  std::isfinite(metrics.safe_area.top) &&
                  std::isfinite(metrics.safe_area.right) &&
                  std::isfinite(metrics.safe_area.bottom) &&
                  metrics.safe_area.left >= 0.0F && metrics.safe_area.top >= 0.0F &&
                  metrics.safe_area.right >= 0.0F && metrics.safe_area.bottom >= 0.0F &&
                  metrics.safe_area.left + metrics.safe_area.right <= metrics.viewport.width &&
                  metrics.safe_area.top + metrics.safe_area.bottom <= metrics.viewport.height;
  return metrics;
}

human_resources::ui::Rect window_coordinates(const human_resources::ui::Rect rect,
    const WindowMetrics& metrics) noexcept {
  const float scale = metrics.window_units_per_point;
  return {rect.x * scale, rect.y * scale, rect.width * scale, rect.height * scale};
}

WindowMetrics query_window_metrics(SDL_Window* window) noexcept {
  if (window == nullptr)
    return {};
  int width = 0;
  int height = 0;
  if (!SDL_GetWindowSize(window, &width, &height) || width <= 0 || height <= 0)
    return {};
  SDL_Rect safe{0, 0, width, height};
  if (!SDL_GetWindowSafeArea(window, &safe))
    safe = {0, 0, width, height};
  const int safe_right = std::clamp(safe.x + safe.w, 0, width);
  const int safe_bottom = std::clamp(safe.y + safe.h, 0, height);
  return normalize_window_metrics(
      {static_cast<float>(width), static_cast<float>(height)},
      {static_cast<float>(std::clamp(safe.x, 0, width)),
          static_cast<float>(std::clamp(safe.y, 0, height)),
          static_cast<float>(width - safe_right), static_cast<float>(height - safe_bottom)},
      SDL_GetWindowPixelDensity(window), SDL_GetWindowDisplayScale(window));
}

} // namespace human_resources::platform
