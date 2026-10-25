#pragma once
#include <human_resources/gameplay/gameplay.h>
#include <human_resources/presentation/scenery.h>
#include <human_resources/presentation/style_scene.h>
#include <human_resources/progression/progression.h>
#include <human_resources/ui/geometry.h>
#include <string>
#include <string_view>
#include <vector>

namespace human_resources::product {
class SceneEffects;
enum class Screen { title,
  run,
  pause,
  upgrade,
  death,
  results,
  hangar,
  settings,
  blueprints };
enum class Command { none,
  start,
  hangar,
  settings,
  resume,
  pause,
  finish,
  retry,
  home,
  revive,
  bonus,
  pick0,
  pick1,
  pick2,
  reroll,
  buy0,
  buy1,
  buy2,
  buy3,
  audio,
  haptics,
  motion,
  retry_save,
  dash,
  back_up_reset,
  blueprints,
  recipes,
  recipe_next,
  recipe_previous,
  recipe0,
  recipe1,
  recipe2,
  recipe3,
  recipe4,
  recipe5,
  location_next,
  back_up_bank,
  page_next,
  page_previous,
  current_build,
  preview0,
  preview1,
  preview2,
  privacy };
struct Button final {
  ui::Rect bounds;
  Command command{};
  std::string label;
  bool enabled{true};
};
struct Interface final {
  std::vector<presentation::ScreenQuad> quads;
  std::vector<Button> buttons;
  unsigned page{}, page_count{1};
  // Visible measured page copy also supplies the native spoken summary.
  std::string page_text;
  bool text_overflow{}; // Internal fit result; the product selects its measured layout.
};
struct View final {
  Screen screen{Screen::title};
  const gameplay::Snapshot* run{};
  const progression::AccountStore* store{};
  ui::Rect viewport;
  ui::Insets safe_area;
  bool save_error{}, guest{}, revive_available{}, bonus_available{};
  ui::Point stick_origin{}, stick_position{};
  bool stick_active{};
  std::string_view save_message{};
  bool can_reset_save{};
  const progression::RunReceipt* pending_results{};
  bool show_recipes{};
  unsigned recipe_page{};
  presentation::SceneryTheme theme{presentation::SceneryTheme::farmland};
  bool haptics_available{};
  bool restored_run{}, can_bank_checkpoint{};
  float text_scale{1.F};
  unsigned ui_page{};
  bool show_build{};
  unsigned upgrade_preview{};
  bool show_privacy{};
};
[[nodiscard]] constexpr std::string_view location_name(presentation::SceneryTheme theme) {
  switch (theme) {
  case presentation::SceneryTheme::suburbia:
    return "Suburbia";
  case presentation::SceneryTheme::desert:
    return "Desert";
  default:
    return "Farmland";
  }
}
[[nodiscard]] constexpr presentation::SceneryTheme next_location(presentation::SceneryTheme theme) {
  switch (theme) {
  case presentation::SceneryTheme::farmland:
    return presentation::SceneryTheme::suburbia;
  case presentation::SceneryTheme::suburbia:
    return presentation::SceneryTheme::desert;
  default:
    return presentation::SceneryTheme::farmland;
  }
}
[[nodiscard]] Interface interface(const View& view);
[[nodiscard]] presentation::StyleFrame scene(const gameplay::Snapshot& run, float aspect, bool reduced_motion, bool attract = false, SceneEffects* effects = nullptr, presentation::SceneryTheme theme = presentation::SceneryTheme::farmland);
[[nodiscard]] std::vector<std::string_view> content_ids();
} // namespace human_resources::product
