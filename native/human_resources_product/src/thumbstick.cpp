#include "thumbstick.h"
#include <human_resources/product/controls.h>
#include <algorithm>
#include <array>
#include <cmath>
namespace human_resources::product::detail {
using Color = std::array<float, 4>;
constexpr Color mint{.39F,.96F,.75F,1};
void append_thumbstick(Interface& output, const View& view) {
  if (!view.stick_active) return;
  const auto safe = ui::inset(view.viewport, view.safe_area);
  const auto circle = [&](ui::Point center, float radius, Color color) {
    const auto clipped = ui::intersect({center.x - radius, center.y - radius, radius * 2, radius * 2}, safe);
    if (clipped.width <= 0 || clipped.height <= 0) return;
    const ui::Rect r{center.x - radius, center.y - radius, radius * 2, radius * 2};
    output.quads.push_back({{(r.x - view.viewport.x) / view.viewport.width,
        (r.y - view.viewport.y) / view.viewport.height, r.width / view.viewport.width, r.height / view.viewport.height},
        {r.width, r.height, -radius, 0}, color});
  };
  const auto origin = view.stick_origin;
  const float dx = view.stick_position.x - origin.x, dy = view.stick_position.y - origin.y;
  const float scale = Controls::stick_radius / std::max(Controls::stick_radius, std::hypot(dx, dy));
  circle(origin, Controls::stick_radius + 18, {1, 1, 1, .32F});
  circle(origin, Controls::stick_radius + 16, {.035F, .105F, .17F, .8F});
  circle(origin, Controls::stick_radius + 14, {1, 1, 1, .08F});
  circle({origin.x + dx * scale, origin.y + dy * scale}, 7, mint);
}
} // namespace human_resources::product::detail
