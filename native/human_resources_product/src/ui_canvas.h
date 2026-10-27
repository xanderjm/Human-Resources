#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <sstream>
#include <human_resources/product/presentation.h>
#include <product_glyphs.generated.h>

// Private renderer-neutral draw, measurement and hit-geometry owner.
namespace human_resources::product::detail {
using Color = std::array<float, 4>;
inline constexpr Color navy{.035F, .105F, .17F, .96F}, panel{.06F, .16F, .23F, .97F}, mint{.39F, .96F, .75F, 1}, white{.96F, .97F, .89F, 1}, muted{.67F, .78F, .76F, 1}, gold{1, .79F, .32F, 1}, coral{1,.43F,.36F,1};
inline std::string number(std::uint64_t n) {
  return std::to_string(n);
}
inline std::string time_text(double seconds) {
  const auto n = static_cast<unsigned>(seconds);
  return number(n / 60) + ":" + (n % 60 < 10 ? "0" : "") + number(n % 60);
}
struct Canvas final {
  Interface output;
  float w, h, scale, ox, oy, vx, vy;
  float type_scale{1};
  Command held_command{Command::none};
  ui::Rect safe;
  std::vector<ui::Rect> text_lines;
  explicit Canvas(const View& v, bool wide = false) : w(v.viewport.width), h(v.viewport.height), vx(v.viewport.x), vy(v.viewport.y), held_command(v.held_command) {
    safe = ui::inset(v.viewport, v.safe_area);
    if (v.screen == Screen::run) {
      const auto framing = v.framing ? *v.framing : gameplay_framing(v.viewport, v.safe_area, v.text_scale);
      if (framing.constrained && !framing.compact) safe = framing.run_layout;
    }
    type_scale = std::isfinite(v.text_scale) && v.text_scale > 1 ? v.text_scale : 1;
    const float base_w = wide ? 844.F : 390.F, base_h = wide ? 390.F : 844.F;
    scale = std::min(safe.width / base_w, safe.height / base_h);
    ox = safe.x + (safe.width - base_w * scale) * .5F;
    oy = safe.y + (safe.height - base_h * scale) * .5F;
  }
  ui::Rect rect(float x, float y, float width, float height) const {
    return {ox + x * scale, oy + y * scale, width * scale, height * scale};
  }
  void fill(float x, float y, float width, float height, Color color) {
    const auto r = rect(x, y, width, height);
    output.quads.push_back({{(r.x - vx) / w, (r.y - vy) / h, r.width / w, r.height / h}, {r.width, r.height, -std::max(.5F, std::min({12.F * scale, r.width * .5F, r.height * .5F})), 0}, color});
  }
  Color button_color(Color color, Command command, bool enabled = true) const {
    if (enabled && command != Command::none && command == held_command) {
      const bool dark = color[0] + color[1] + color[2] < 1;
      for (unsigned i = 0; i < 3; ++i)
        color[i] = dark ? color[i] + .14F * (1 - color[i]) : color[i] * .82F;
    }
    return color;
  }
  void button_fill(float x, float y, float width, float height, Color color, Command command, bool enabled = true) {
    fill(x, y, width, height, button_color(color, command, enabled));
  }
  float measure(std::string_view text, float size, bool title = false) const {
    size = std::max(size, 14.F / scale) * type_scale;
    float width = 0;
    for (char source : text) {
      const auto ch = static_cast<unsigned char>(source);
      const auto glyph_index = static_cast<std::size_t>((title ? 95 : 0) + (ch >= 32 && ch < 127 ? ch - 32 : 31));
      width += glyph_advances[glyph_index] * size / 48.F;
    }
    return width;
  }
  void text(float x, float y, std::string_view text, float size = 17, Color color = white, bool title = false) {
    const float measured = measure(text, size, title);
    size = std::max(size, 14.F / scale) * type_scale;
    if (type_scale > 1 && !text.empty()) {
      const auto bounds = rect(x, y, measured, size);
      if (bounds.x < safe.x || bounds.y < safe.y || bounds.right() > safe.right() || bounds.bottom() > safe.bottom()) output.text_overflow = true;
      for (const auto previous : text_lines) {
        const auto overlap = ui::intersect(bounds, previous);
        if (overlap.width > 1 && overlap.height > 1) output.text_overflow = true;
      }
      text_lines.push_back(bounds);
    }
    for (char source : text) {
      auto ch = static_cast<unsigned char>(source);
      if (ch < 32 || ch > 126)
        ch = '?';
      const unsigned index = ch - 32;
      const auto r = rect(x - 4 * size / 48.F, y - 8 * size / 48.F, 64 * size / 48.F, 80 * size / 48.F);
      output.quads.push_back({{(r.x - vx) / w, (r.y - vy) / h, r.width / w, r.height / h}, {static_cast<float>((index % 16) * 64) / 1024, static_cast<float>((title ? 512 : 0) + (index / 16) * 80) / 2048, 64.F / 1024, 80.F / 2048}, color});
      x += glyph_advances[(title ? 95 : 0) + index] * size / 48.F;
    }
  }
  void centered(float x, float y, float width, std::string_view value, float size = 17, Color color = white, bool title = false) {
    text(x + (width - measure(value, size, title)) * .5F, y, value, size, color, title);
  }
  void fitted(float x, float y, float width, std::string_view value, float size = 17, Color color = white, bool title = false, bool center = false) {
    if (type_scale > 1 && measure(value, size, title) > width) output.text_overflow = true;
    while (type_scale == 1 && size > 14.F / scale && measure(value, size, title) > width)
      size -= .5F;
    if (center)
      centered(x, y, width, value, size, color, title);
    else
      text(x, y, value, size, color, title);
  }
  void wrap(float x, float y, float width, std::string_view value, float size = 16, Color color = muted, unsigned max_lines = 5, float leading = 1.2F) {
    size = std::max(size, 14.F / scale);
    std::istringstream stream{std::string(value)};
    std::string word, line;
    unsigned lines = 0;
    while (stream >> word) {
      const auto next = line.empty() ? word : line + " " + word;
      if (measure(next, size) > width && !line.empty()) {
        text(x, y, line, size, color);
        y += size * type_scale * leading;
        line = word;
        if (++lines >= max_lines) {
          if (type_scale > 1) output.text_overflow = true;
          return;
        }
      } else
        line = next;
    }
    if (!line.empty())
      text(x, y, line, size, color);
  }
  void sprite(float x, float y, float width, float height, float sx, float sy, float sw, float sh, Color color = {1, 1, 1, 1}) {
    const auto r = rect(x, y, width, height);
    output.quads.push_back({{(r.x - vx) / w, (r.y - vy) / h, r.width / w, r.height / h}, {sx / 1024.F, sy / 2048.F, sw / 1024.F, sh / 2048.F}, color});
  }
  void icon(float x, float y, unsigned id, float size = 28) {
    sprite(x, y, size, size, static_cast<float>(id) * 128, 1024, 128, 128);
  }
  void hud(float x, float y, float width, float height) {
    sprite(x, y, width, height, 0, 1152, 768, 192);
  }
  void card(float x, float y, float width, float height) {
    // Crop transparent authoring margins; all text is inset from this visible frame.
    sprite(x, y, width, height, 536, 1432, 464, 592);
  }
  void button(float x, float y, float width, std::string label, Command command, bool enabled = true, Color color = mint, float height = 46) {
    if (type_scale > 1 && (measure(label, 17) > width - 12 || std::max(17.F, 14.F / scale) * type_scale > height - 8)) output.text_overflow = true;
    button_fill(x, y, width, height, enabled ? color : Color{.15F, .23F, .28F, 1}, command, enabled);
    centered(x, y + (height - 22) * .5F - 2, width, label, 17, enabled ? navy : muted);
    auto target = rect(x, y, width, height);
    if (target.height < 44) {
      target.y -= (44 - target.height) * .5F;
      target.height = 44;
    }
    if (target.width < 44) {
      target.x -= (44 - target.width) * .5F;
      target.width = 44;
    }
    output.buttons.push_back({target, command, std::move(label), enabled});
  }
  void header(std::string_view title, std::string_view subtitle) {
    fill(92, 26, 660, 338, navy);
    text(116, 40, title, 30, white, true);
    text(116, 81, subtitle, 16, muted);
  }
};
} // namespace human_resources::product::detail
