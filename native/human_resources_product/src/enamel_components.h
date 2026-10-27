#pragma once

#include "ui_canvas.h"
#include <ui_components.generated.h>
#include <span>

// Product-owned presentation components. No SDL, renderer resource or game
// mutation is reachable here; text and hit geometry remain live value data.
namespace human_resources::product::enamel {
using detail::Canvas;
using detail::Color;
inline constexpr Color ink{.0314F,.1412F,.2392F,1};
inline constexpr Color cream{1,.9451F,.8157F,1};
inline constexpr Color accent{1,.3843F,.2980F,1};
inline constexpr Color active{.3451F,.9294F,.7804F,1};
inline constexpr Color transparent_white{1,1,1,1};
enum class Font { body, display, compact, prose };
enum class ButtonStyle { secondary, primary, navy };
struct Label final {
  float size{18};
  Color color{cream};
  Font font{Font::body};
  bool centered{}, shadow{};
  float rise{}; // Rise in local points per horizontal local point.
  bool shrink_to_fit{true}; // Measured accessibility pages keep their exact requested size.
};
void picture(Canvas&, Sprite, ui::Rect, Color tint = transparent_white);
void panel(Canvas&, Sprite, ui::Rect, float corner = 14, Color tint = transparent_white);
[[nodiscard]] float measure(const Canvas&, std::string_view, float, Font = Font::body);
[[nodiscard]] std::string short_number(std::uint64_t);
void label(Canvas&, ui::Rect, std::string_view, Label = {});
void tactical_label(Canvas&, ui::Rect, std::string_view, float size, Color, bool centered = false);
// Essential copy is measured in full. Overflow selects the paginated layout,
// never an ellipsis or silent truncation of a requirement or save reason.
void paragraph(Canvas&, ui::Rect, std::string_view, float size = 15, Color = ink, Font = Font::prose);
[[nodiscard]] float paragraph_height(const Canvas&, std::string_view, float width, float size = 15, Font = Font::prose);
void heading(Canvas&, ui::Rect, std::string_view, bool compact = false);
void button(Canvas&, ui::Rect, std::string, Command, ButtonStyle = ButtonStyle::secondary,
            bool enabled = true);
void action_panel(Canvas&, ui::Rect, std::string, Command, Sprite = Sprite::cream_panel, bool enabled = true);
struct Tab final {std::string_view label; Sprite icon; Command command;};
void tabs(Canvas&, ui::Rect, std::span<const Tab>, unsigned selected);
[[nodiscard]] std::vector<std::string> lines(const Canvas&, std::string_view, float width, float size, Font = Font::prose);
void icon(Canvas&, ui::Rect, Sprite);
void progress(Canvas&, ui::Rect, float, Color = active);
void pips(Canvas&, ui::Rect, unsigned owned, unsigned maximum);
void toggle(Canvas&, ui::Rect, std::string, Sprite, bool on, Command, bool available = true);
void wallet(Canvas&, ui::Rect, std::uint64_t credits);
void blueprint_progress(Canvas&, ui::Rect, const progression::AccountStore&);
[[nodiscard]] Sprite hardware_icon(gameplay::Module);
[[nodiscard]] Sprite reward_icon(const gameplay::UpgradeChoice&);
} // namespace human_resources::product::enamel
