#include <human_resources/product/gameplay_framing.h>
#include <product_glyphs.generated.h>
#include <algorithm>
#include <cmath>

namespace human_resources::product {
namespace {
constexpr float portrait = 320.F / 568.F;
float word_width(std::string_view word, float size) {
  float width = 0;
  for (char ch : word) width += glyph_advances[static_cast<unsigned>(ch - 32)] * size / 48.F;
  return width;
}
} // namespace
GameplayFraming gameplay_framing(ui::Rect viewport, ui::Insets safe_area, float text_scale, presentation::PixelExtent output_pixels, FramingContext context) {
  GameplayFraming result;
  result.context = context;
  result.viewport = viewport;
  result.safe = ui::inset(viewport, safe_area);
  if (!viewport.finite() || !result.safe.finite() || viewport.width <= 0 || viewport.height <= 0 || result.safe.width <= 0 || result.safe.height <= 0) return result;
  result.constrained = context == FramingContext::flight && viewport.width / viewport.height > portrait + .00001F;
  result.compact = result.constrained && result.safe.height < 568.F;
  ui::Rect available = viewport;
  if (result.constrained) {
    const float type = std::isfinite(text_scale) ? std::max(1.F, text_scale) : 1.F;
    // Measure button copy in points; status copy wraps vertically in the rail.
    const float left = std::max(52.F, std::max(word_width("BUMP",14*type),word_width("SHOT",14*type)) + 12);
    const float right = std::max(52.F, std::max({word_width("DASH",14*type),word_width("BOOST",14*type),word_width("SWEEP",14*type),word_width("ADDED",14*type)}) + 12);
    // Short displays use top/bottom strips: opaque full-height side rails
    // would hide a ship that can now fly to the actual left and right edges.
    result.banded = result.compact;
    if (result.banded) {
      const float top = result.safe.y + GameplayFraming::tactical_row_height + 12;
      const float bottom = result.safe.bottom() - GameplayFraming::tactical_bottom_margin - GameplayFraming::tactical_row_height - 10;
      const float room_height = std::max(1.F, bottom - top);
      const float width = std::min(result.safe.width, room_height * portrait);
      const float height = width / portrait;
      available = {result.safe.x + (result.safe.width - width) * .5F, top + (room_height - height) * .5F, width, height};
    } else {
      const float room = result.compact ? result.safe.width-left-right : result.safe.width;
      const float width = std::min(result.safe.height * portrait, room);
      const float height = width / portrait;
      available = {result.safe.x + (result.compact ? left : 0) + (room - width) * .5F, result.safe.y + (result.safe.height - height) * .5F, width, height};
    }
    result.destination = {(available.x - viewport.x) / viewport.width, (available.y - viewport.y) / viewport.height, available.width / viewport.width, available.height / viewport.height};
  }
  const auto ui_stage = available; // Keep measured HUD/rail placement independent of flight reach.
  if (context == FramingContext::flight) {
    available = viewport;
    result.destination = {0, 0, 1, 1};
  }
  result.stage = available;
  result.aspect = available.width / available.height;
  if (output_pixels.width && output_pixels.height) {
    const float width = available.width / viewport.width * static_cast<float>(output_pixels.width);
    const float height = available.height / viewport.height * static_cast<float>(output_pixels.height);
    const float factor = std::min(1.F, 1280.F / std::max(width, height));
    result.render_extent = {static_cast<unsigned>(std::max(4, static_cast<int>(width * factor) / 4 * 4)),
        static_cast<unsigned>(std::max(4, static_cast<int>(height * factor) / 4 * 4))};
    result.aspect = static_cast<float>(result.render_extent.width) / static_cast<float>(result.render_extent.height);
    const auto actual = presentation::fit_world_destination(result.render_extent.width, result.render_extent.height,
        output_pixels.width, output_pixels.height, result.destination);
    const float sx = viewport.width / static_cast<float>(output_pixels.width), sy = viewport.height / static_cast<float>(output_pixels.height);
    result.stage = {viewport.x + static_cast<float>(actual.x) * sx, viewport.y + static_cast<float>(actual.y) * sy,
        static_cast<float>(actual.width) * sx, static_cast<float>(actual.height) * sy};
  }
  result.run_layout = result.banded ? result.safe : result.constrained ? ui_stage : result.safe;
  if (result.compact && !result.banded) {
    result.left_rail = {result.safe.x, result.safe.y, ui_stage.x - result.safe.x, result.safe.height};
    result.right_rail = {ui_stage.right(), result.safe.y, result.safe.right() - ui_stage.right(), result.safe.height};
  }
  presentation::StyleFrame projection;
  if(context==FramingContext::attract) result.camera_width=(result.aspect<1?17.F:31.F)/.75F;
  if (context == FramingContext::flight)
    result.camera_width *= std::max(1.F, result.aspect / portrait);
  presentation::set_overhead_camera(projection, result.aspect, result.camera_width, {0, 0, -.15F}, 27.F);
  result.camera = projection.camera;
  result.logical_pixel = (context == FramingContext::flight ? 17.F / .75F : result.camera_width) / std::min(320.F, ui_stage.width);
  // Invert the actual orthographic camera on the ship's Y=2.5m plane. A centre
  // at a clip edge leaves roughly half the saucer outside, on every aspect/DPI.
  const auto& m = result.camera;
  const float determinant = m[0] * m[6] - m[2] * m[4];
  if (std::abs(determinant) > .000001F) {
    auto& area = result.flight_bounds;
    area = {256, -256, 256, -256};
    for (float x : {-1.F, 1.F}) for (float y : {-1.F, 1.F}) {
      const float cx = x - m[1] * 2.5F - m[3], cy = y - m[5] * 2.5F - m[7];
      const float world_x = (cx * m[6] - cy * m[2]) / determinant;
      const float station = 2.3F - (cy * m[0] - cx * m[4]) / determinant;
      area.left = std::min(area.left, world_x); area.right = std::max(area.right, world_x);
      area.backward = std::min(area.backward, station); area.forward = std::max(area.forward, station);
    }
  }
  return result;
}
GameplayFraming gameplay_framing(float aspect, FramingContext context) {
  if (!std::isfinite(aspect) || aspect <= 0) return {};
  return gameplay_framing({0, 0, 844.F * aspect, 844.F},{},1,{},context);
}
} // namespace human_resources::product
