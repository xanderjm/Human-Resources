#pragma once

#include "ui_canvas.h"

namespace human_resources::product::enamel {
// Short portraits reflow on the same width grid instead of shrinking the whole
// tall-phone composition. Tactical HUD framing has its own physical contract.
inline void configure_portrait(detail::Canvas& c) {
  const float width_scale=c.safe.width/390.F;
  if(c.safe.height/width_scale<760) {
    c.scale=width_scale;
    c.ox=c.safe.x;
    c.oy=c.safe.y;
  }
}
[[nodiscard]] inline float height(const detail::Canvas& c) {
  return std::min(844.F,c.safe.height/c.scale);
}
[[nodiscard]] inline bool compact(const detail::Canvas& c) {
  return height(c)<760;
}
} // namespace human_resources::product::enamel
