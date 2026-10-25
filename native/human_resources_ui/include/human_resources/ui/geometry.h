#pragma once

#include <algorithm>
#include <cmath>

namespace human_resources::ui {

struct Point final {
  float x{};
  float y{};
};

struct Size final {
  float width{};
  float height{};
};

struct Insets final {
  float left{};
  float top{};
  float right{};
  float bottom{};
};

struct Rect final {
  float x{};
  float y{};
  float width{};
  float height{};

  [[nodiscard]] constexpr float right() const noexcept {
    return x + width;
  }
  [[nodiscard]] constexpr float bottom() const noexcept {
    return y + height;
  }
  [[nodiscard]] constexpr bool contains(const Point point) const noexcept {
    return point.x >= x && point.y >= y && point.x <= right() && point.y <= bottom();
  }
  [[nodiscard]] bool finite() const noexcept {
    return std::isfinite(x) && std::isfinite(y) && std::isfinite(width) && std::isfinite(height);
  }
};

struct Color final {
  float red{};
  float green{};
  float blue{};
  float alpha{1.0F};
};

[[nodiscard]] constexpr Rect intersect(const Rect first, const Rect second) noexcept {
  const float left = std::max(first.x, second.x);
  const float top = std::max(first.y, second.y);
  const float right = std::min(first.right(), second.right());
  const float bottom = std::min(first.bottom(), second.bottom());
  return {.x = left, .y = top, .width = std::max(0.0F, right - left), .height = std::max(0.0F, bottom - top)};
}

[[nodiscard]] constexpr Rect inset(const Rect rect, const Insets insets) noexcept {
  const float width = std::max(0.0F, rect.width - insets.left - insets.right);
  const float height = std::max(0.0F, rect.height - insets.top - insets.bottom);
  return {.x = rect.x + insets.left, .y = rect.y + insets.top, .width = width, .height = height};
}

} // namespace human_resources::ui
