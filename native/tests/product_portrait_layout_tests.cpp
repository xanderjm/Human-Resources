#include "progression_test_backend.h"
#include "test_support.h"
#include <human_resources/product/presentation.h>
#include <human_resources/product/accessibility.h>
#include <human_resources/gameplay/tuning.h>
#include <cctype>
#include <limits>
#include <map>
#include <set>
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>

namespace {
namespace hr = human_resources;
using namespace hr::product;
using test_support::require;
bool glyph(const hr::presentation::ScreenQuad& q) {
  return std::abs(q.uv[2] - 64.F / 1024.F) < .00001F && std::abs(q.uv[3] - 80.F / 2048.F) < .00001F;
}
void compact_phone_cards_preserve_effects_and_scene_stage() {
  progression_tests::MemoryBackend backend;
  hr::progression::AccountStore store{backend};
  require(store.load().playable(), "fresh account loads");
  hr::gameplay::Game game;
  game.start();
  auto run = game.snapshot();
  run.rerolls_remaining = 0;
  // Real rank-III hardware text includes the threshold, slot/cooldown and kill buff.
  const std::string effect = "Jeep capture hull threshold: 60 -> 75%. Uses 2 slots; 8s vehicle cooldown. Tanks <=35% hull. Vehicle kills: +40% bio speed, 5s.";
  for (auto& pick : run.choices) {
    pick.hardware = true;
    pick.title = "Overtime Reactor 3";
    pick.next_rank = 3;
    pick.description = effect;
    pick.evolution_preview = "Hostile Acquisition";
  }
  constexpr std::array<hr::ui::Rect, 5> phones{{{0,0,320,568},{0,0,320,694},{0,0,375,667},{0,0,390,844},{0,0,430,932}}};
  for (const auto phone : phones) {
    View view{Screen::upgrade, &run, &store, phone, {0,20,0,0}};
    const auto ui = interface(view);
    const auto safe = hr::ui::inset(view.viewport, view.safe_area);
    require(ui.buttons.size() == 4, "three choice cards and one evolution library target");
    for (std::size_t i = 0; i < ui.buttons.size(); ++i) {
      const auto& b = ui.buttons[i];
      require(b.bounds.height >= 44 && b.bounds.width >= 44, "portrait target remains touch sized");
      require(b.bounds.x >= safe.x && b.bounds.y >= safe.y && b.bounds.right() <= safe.right() + .01F && b.bounds.bottom() <= safe.bottom() + .01F, "portrait targets stay within safe area");
      for (std::size_t j = i+1; j < ui.buttons.size(); ++j) {
        const auto overlap = hr::ui::intersect(b.bounds, ui.buttons[j].bounds);
        require(overlap.width < .01F || overlap.height < .01F, "card and library hit targets do not overlap");
      }
    }
    const auto& first = ui.buttons[0].bounds;
    unsigned letters = 0;
    for (const auto& q : ui.quads) {
      if (!glyph(q) || q.color[0] != .035F || q.color[1] != .105F) continue;
      const float x = q.rect[0] * phone.width, y = q.rect[1] * phone.height;
      if (y >= first.y && y < first.bottom()) {
        const unsigned column = static_cast<unsigned>(std::lround(q.uv[0] * 1024.F / 64.F));
        const unsigned row = static_cast<unsigned>(std::lround(q.uv[1] * 2048.F / 80.F));
        if (row * 16 + column != 0) ++letters;
        require(x >= first.x && x < first.right(), "effect glyph starts inside visible card frame");
        const float size = q.rect[2] * phone.width * 48.F / 64.F;
        require(size >= 13.99F, "effect text remains at least fourteen points");
        // Atlas glyph quads contain transparent bearings; check the actual line
        // origin plus one em against the cream panel's lower edge.
        require(y + size * (1.F + 8.F / 48.F) <= first.bottom() - 2.F, "last effect line stays within card");
      }
    }
    const auto expected = static_cast<unsigned>(std::count_if(effect.begin(), effect.end(), [](char ch) { return ch != ' '; }));
    require(letters == expected, "long complete effect copy is rendered without truncation");
    view.screen = Screen::title;
    const auto title = interface(view);
    // Central stage is reserved for the actual saucer/capture scene.
    const float scale = std::min(safe.width / 390.F, safe.height / 844.F);
    const float oy = safe.y + (safe.height - 844.F * scale) * .5F;
    const hr::ui::Rect stage{safe.x + safe.width*.2F, oy+210.F*scale, safe.width*.6F, 330.F*scale};
    for (const auto& q : title.quads) {
      if (q.uv[2] >= 0) continue;
      const hr::ui::Rect bounds{q.rect[0]*phone.width,q.rect[1]*phone.height,q.rect[2]*phone.width,q.rect[3]*phone.height};
      const auto overlap = hr::ui::intersect(bounds,stage);
      require(q.color[3] < .5F || overlap.width < .01F || overlap.height < .01F, "title has no opaque central panel hiding its hero");
    }
  }
}
std::string compact(std::string_view text) {
  std::string result;
  for (const char ch : text)
    if (!std::isspace(static_cast<unsigned char>(ch))) result += ch;
  return result;
}
std::string atlas_copy(const Interface& ui) {
  std::string text;
  for (const auto& q : ui.quads) {
    if (!glyph(q)) continue;
    const unsigned col = static_cast<unsigned>(std::lround(q.uv[0] * 1024.F / 64.F));
    const float offset = q.uv[1] * 2048.F >= 512 ? 512.F : 0.F;
    const unsigned row = static_cast<unsigned>(std::lround((q.uv[1] * 2048.F - offset) / 80.F));
    text += static_cast<char>(32 + row * 16 + col);
  }
  return text;
}

void readable_page_is_real_safe_and_scaled(const View& view, const Interface& page) {
  const auto safe = hr::ui::inset(view.viewport, view.safe_area);
  std::string rendered;
  std::vector<hr::ui::Rect> glyph_lines;
  for (const auto& q : page.quads) {
    if (!glyph(q)) continue;
    const float size = q.rect[2] * view.viewport.width * 48.F / 64.F;
    require(size >= 14.F * view.text_scale - .01F, "readable body AND action glyphs honor requested text scale");
    const float x = q.rect[0] * view.viewport.width + size * 4.F / 48.F;
    const float y = q.rect[1] * view.viewport.height + size * 8.F / 48.F;
    require(x >= safe.x && x < safe.right() && y >= safe.y && y + size <= safe.bottom(), "measured glyph origins and line height stay in safe area");
    const unsigned col = static_cast<unsigned>(std::lround(q.uv[0] * 1024.F / 64.F));
    const unsigned row = static_cast<unsigned>(std::lround(q.uv[1] * 2048.F / 80.F));
    rendered += static_cast<char>(32 + row * 16 + col);
    glyph_lines.push_back({x,y,size,size});
  }
  require(compact(rendered).find(compact(page.page_text)) != std::string::npos, "complete spoken page is also rendered through actual atlas quads");
  if (!page.page_text.empty() && !page.buttons.empty()) {
    auto body = page.page_text;
    body.erase(std::remove(body.begin(),body.end(),'\n'),body.end());
    const auto begin = rendered.find(body);
    require(begin != std::string::npos, "actual typed body is present in atlas order");
    float first_action = safe.bottom();
    for (const auto& button : page.buttons)
      if (button.command < Command::preview0 || button.command > Command::preview2) first_action = std::min(first_action,button.bounds.y);
    for (std::size_t i = begin; i < begin + body.size(); ++i)
      require(glyph_lines[i].bottom() <= first_action + .01F, "typed heading/body never overlap fixed primary or page actions");
  }
  hr::accessibility::Session session;
  require(session.publish(accessibility_snapshot(view, page)), "paged semantic snapshot has unique controls");
  for (std::size_t i = 0; i < page.buttons.size(); ++i) {
    const auto& a = page.buttons[i].bounds;
    require(a.width >= 44 && a.height >= 44, "readable actions remain at least forty-four points");
    require(a.x >= safe.x && a.y >= safe.y && a.right() <= safe.right() + .01F && a.bottom() <= safe.bottom() + .01F, "all readable actions remain in safe area");
    for (std::size_t j = i + 1; j < page.buttons.size(); ++j) {
      const auto overlap = hr::ui::intersect(a, page.buttons[j].bounds);
      require(overlap.width < .01F || overlap.height < .01F, "readable navigation/action targets cannot overlap");
    }
  }
}
void larger_text_preserves_whole_offers_and_all_menu_actions() {
  progression_tests::MemoryBackend backend;
  hr::progression::AccountStore store{backend};
  require(store.load().playable(), "readable fresh account loads");
  hr::gameplay::Game game;
  game.start();
  auto run = game.snapshot();
  run.rerolls_remaining = 2;
  const std::string effect = "Jeep capture hull threshold: 60 -> 75%. Uses 2 slots; 8s vehicle cooldown. Tanks <=35% hull. Vehicle kills: +40% bio speed, 5s.";
  for (unsigned i = 0; i < 3; ++i) {
    run.choices[i].title = "Overtime Reactor " + std::to_string(i + 1);
    run.choices[i].description = effect;
    run.choices[i].next_rank = 3;
    run.choices[i].hardware = true;
    run.choices[i].evolution_preview = "Hostile Acquisition";
  }
  constexpr std::array<float, 4> scales{1.3F, 1.6F, 2.F, 3.2F};
  constexpr std::array<hr::ui::Rect, 3> phones{{{0,0,320,568},{0,0,390,844},{0,0,844,390}}};
  for (const auto phone : phones)
    for (const auto scale : scales) {
      View view{Screen::upgrade, &run, &store, phone, {0,20,0,0}};
      view.text_scale = scale;
      const auto first = interface(view);
      require(first.page_count > 1 && first.page_count < 128, "real offers use finite local detail pages");
      std::map<Command, std::string> offered_text;
      for (unsigned preview = 0; preview < 3; ++preview) {
        view.upgrade_preview = preview;
        view.ui_page = 0;
        const auto selected = interface(view);
        require(compact(selected.page_text).find(compact(run.choices[preview].title)) != std::string::npos, "the complete actual upgrade name is visible before Choose on every first preview page");
        require(!compact(selected.page_text).starts_with("PROMOTION"), "redundant promotion prose never displaces the actual upgrade name");
        for (unsigned i = 0; i < selected.page_count; ++i) {
          view.ui_page = i;
          const auto page = interface(view);
          require(page.page == i && page.page_count == selected.page_count, "selected offer detail pagination is stable");
          readable_page_is_real_safe_and_scaled(view, page);
          for (unsigned target = 0; target < 3; ++target)
            require(std::any_of(page.buttons.begin(), page.buttons.end(), [&](const auto& button) { return button.command == static_cast<Command>(static_cast<unsigned>(Command::preview0) + target); }), "all three direct preview tabs remain available on every detail page");
          for (const auto& button : page.buttons)
            if (button.command >= Command::pick0 && button.command <= Command::pick2) {
              require(button.command == static_cast<Command>(static_cast<unsigned>(Command::pick0) + preview), "preview never reindexes the actual earned choice");
              offered_text[button.command] += page.page_text;
            }
        }
      }
      view.upgrade_preview = 0;
      for (unsigned i = 0; i < 3; ++i) {
        const auto command = static_cast<Command>(static_cast<unsigned>(Command::pick0) + i);
        require(compact(offered_text[command]).find(compact(effect)) != std::string::npos, "every actual pick retains its full longest description across continuation pages");
        require(compact(offered_text[command]).find(compact(run.choices[i].title)) != std::string::npos, "page navigation never reindexes a choice");
      }
      view.ui_page = std::numeric_limits<unsigned>::max();
      require(interface(view).page == first.page_count - 1, "stale page safely clamps after content refresh");
      for (unsigned screen = 0; screen < 9; ++screen) {
        view.screen = static_cast<Screen>(screen);
        view.ui_page = 0;
        for (const bool recipes : {false, true}) {
          view.show_recipes = recipes;
          std::set<Command> reachable;
          std::string all_text;
          const auto count = interface(view).page_count;
          for (unsigned i = 0; i < count; ++i) {
            view.ui_page = i;
            const auto page = interface(view);
            readable_page_is_real_safe_and_scaled(view, page);
            all_text += page.page_text.empty() ? atlas_copy(page) : page.page_text;
            for (const auto& button : page.buttons) reachable.insert(button.command);
          }
          if (view.screen == Screen::hangar)
            for (unsigned i = 0; i < 4; ++i)
              require(reachable.contains(static_cast<Command>(static_cast<unsigned>(Command::buy0) + i)), "all permanent purchases remain reachable");
          if (view.screen == Screen::settings)
            require(reachable.contains(Command::motion) && reachable.contains(Command::audio) && reachable.contains(Command::haptics) && reachable.contains(Command::home), "all settings and fixed Back are reachable");
          if (view.screen == Screen::blueprints && recipes && count == 1 && interface(view).page_text.empty()) {
            // When the scaled original two-recipe layout still fits, retain
            // its existing three-page recipe navigation rather than force it.
            for (unsigned recipe_page = 1; recipe_page < 3; ++recipe_page) {
              view.recipe_page = recipe_page;
              all_text += atlas_copy(interface(view));
            }
            view.recipe_page = 0;
          }
          if (view.screen == Screen::blueprints && recipes)
            for (const auto name : hr::gameplay::tuning::evolution_names)
              require(compact(all_text).find(compact(name)) != std::string::npos, "all six recipe identities survive large type");
          if (view.screen == Screen::blueprints && recipes) {

            require(compact(all_text).find(compact("Every 8s release stored reflected damage, capped at 40 base damage scaled by weapon-damage research, divided across up to four enemies. No additional reflection roll or damage procs.")) != std::string::npos, "longest real recipe effect survives in full");
          }
          if (view.screen == Screen::results)
            require(reachable.contains(Command::retry) && reachable.contains(Command::hangar), "results retain retry and permanent progression access");
        }
      }
      view.screen = Screen::pause;
      view.show_build = true;
      for (const bool bank : {false, true}) {
        view.save_error = true;
        view.can_bank_checkpoint = bank;
        view.can_reset_save = !bank;
        view.save_message = "This run cannot resume on this build. Preserve a verified backup and bank its saved credits, or retry with a compatible build.";
        std::string protected_copy;
        std::set<Command> reachable;
        const auto count = interface(view).page_count;
        for (unsigned i = 0; i < count; ++i) {
          view.ui_page = i;
          const auto page = interface(view);
          readable_page_is_real_safe_and_scaled(view, page);
          protected_copy += page.page_text;
          for (const auto& button : page.buttons) {
            require(button.command == Command::page_next || button.command == Command::page_previous || button.command == Command::retry_save || button.command == (bank ? Command::back_up_bank : Command::back_up_reset), "protection pages exclusively expose recovery actions");
            reachable.insert(button.command);
          }
        }
        require(reachable.contains(Command::retry_save) && reachable.contains(bank ? Command::back_up_bank : Command::back_up_reset), "every protected recovery action stays reachable");
        require(compact(protected_copy).find(compact(view.save_message)) != std::string::npos, "complete recovery reason is visible without truncation");
      }
    }
}
float word_size(const Interface& ui, float width, std::string_view word) {
  std::string text;
  std::vector<float> sizes;
  for (const auto& q : ui.quads) {
    if (!glyph(q)) continue;
    const unsigned col = static_cast<unsigned>(std::lround(q.uv[0] * 1024.F / 64.F));
    const unsigned font_y = q.uv[1] * 2048.F >= 512 ? 512U : 0U;
    const unsigned row = static_cast<unsigned>(std::lround((q.uv[1] * 2048.F - static_cast<float>(font_y)) / 80.F));
    text += static_cast<char>(32 + row * 16 + col);
    sizes.push_back(q.rect[2] * width * 48.F / 64.F);
  }
  const auto index = text.find(word);
  require(index != std::string::npos, "measured reference word remains rendered");
  return *std::min_element(sizes.begin() + static_cast<std::ptrdiff_t>(index), sizes.begin() + static_cast<std::ptrdiff_t>(index + word.size()));
}
void small_preferences_preserve_type_growth_and_useful_menus() {
  progression_tests::MemoryBackend backend;
  hr::progression::AccountStore store{backend};
  require(store.load().playable(), "small-preference account loads");
  hr::gameplay::Game game;
  game.start();
  for (const auto phone : std::array<hr::ui::Rect,2>{{{0,0,320,568},{0,0,390,844}}}) {
    View view{Screen::title, &game.snapshot(), &store, phone, {0,20,0,0}};
    float previous_title = 0, previous_body = 0;
    for (const auto scale : {1.F,1.01F,1.1F,1.3F}) {
      view.text_scale = scale;
      view.screen = Screen::title;
      auto ui = interface(view);
      const float title = word_size(ui, phone.width, "RESOURCES");
      require(title >= previous_title - .01F, "small system preference never makes the real title smaller");
      previous_title = title;
      for (const auto expected : {Command::start,Command::hangar,Command::settings,Command::location_next})
        require(std::any_of(ui.buttons.begin(),ui.buttons.end(),[&](const auto& b){return b.command==expected;}), "small text preferences keep all title primary actions immediately available");
      view.screen = Screen::pause;
      ui = interface(view);
      for (const auto expected : {Command::resume,Command::settings,Command::finish,Command::current_build})
        require(std::any_of(ui.buttons.begin(),ui.buttons.end(),[&](const auto& b){return b.command==expected;}), "small text preferences keep all Pause primary actions immediately available");
      view.screen = Screen::settings;
      ui = interface(view);
      const float body = word_size(ui, phone.width, "Capture");
      require(body >= previous_body - .01F, "small system preference never makes actual instruction body smaller");
      previous_body = body;
      for (const auto expected : {Command::motion,Command::audio,Command::haptics,Command::home})
        require(std::any_of(ui.buttons.begin(),ui.buttons.end(),[&](const auto& b){return b.command==expected;}), "small text preferences keep all settings immediately available");
      view.screen = Screen::results;
      ui = interface(view);
      for (const auto expected : {Command::retry,Command::hangar})
        require(std::any_of(ui.buttons.begin(),ui.buttons.end(),[&](const auto& b){return b.command==expected;}), "small text preferences keep both results primary actions immediately available");
    }
    view.screen = Screen::run;
    for (const auto scale : {1.3F,1.6F,2.F,3.2F}) {
      view.text_scale = scale;
      const auto ui = interface(view);
      const auto safe = hr::ui::inset(view.viewport,view.safe_area);
      for (const auto& q : ui.quads) {
        if (q.uv[2] >= 0 || q.rect[1] * phone.height > safe.y + 100) continue;
        require(q.rect[3] * phone.height < 100, "large-text top HUD remains under one hundred points instead of hiding tactical field");
      }
      require(word_size(ui,phone.width,"HP") >= 14 * scale - .01F, "large tactical hull honors requested text size");
      require(std::any_of(ui.buttons.begin(),ui.buttons.end(),[](const auto& b){return b.command==Command::pause;}), "compact tactical Pause remains reachable");
    }
  }
}

void compact_hud_maximum_values_preserve_spacing_and_ingress_bound() {
  progression_tests::MemoryBackend backend;
  hr::progression::AccountStore store{backend};
  require(store.load().playable(), "HUD account loads");
  hr::gameplay::Game game;
  game.start();
  auto run = game.snapshot();
  run.hull = run.maximum_hull = run.shield = run.maximum_shield = 9999;
  run.stats.level = std::numeric_limits<std::uint32_t>::max();
  run.stats.active_seconds = 30;
  for (const auto phone : std::array<hr::ui::Rect,2>{{{0,0,320,568},{0,0,390,844}}}) {
    View view{Screen::run,&run,&store,phone,{0,20,0,0}};
    for (const auto score : {std::uint64_t{0},std::uint64_t{9999},std::uint64_t{999999},std::numeric_limits<std::uint64_t>::max()}) {
      run.stats.score = score;
      const auto ui = interface(view);
      const auto pause = std::find_if(ui.buttons.begin(),ui.buttons.end(),[](const auto& b){return b.command==Command::pause;});
      require(pause != ui.buttons.end(), "maximum HUD retains Pause");
      std::string text;
      std::vector<hr::ui::Rect> origins;
      float occupied_bottom = 0;
      for (const auto& q : ui.quads) {
        const float top = q.rect[1] * phone.height, bottom = (q.rect[1] + q.rect[3]) * phone.height;
        occupied_bottom = std::max(occupied_bottom,bottom);
        require(bottom <= pause->bounds.bottom() + .01F, "actual HUD panels and full glyph quads do not extend below existing Pause ingress boundary");
        if (!glyph(q)) continue;
        const float size = q.rect[2] * phone.width * 48.F / 64.F;
        const unsigned col = static_cast<unsigned>(std::lround(q.uv[0]*1024.F/64.F));
        const unsigned row = static_cast<unsigned>(std::lround(q.uv[1]*2048.F/80.F));
        text += static_cast<char>(32+row*16+col);
        origins.push_back({q.rect[0]*phone.width+size*4.F/48.F,top+size*8.F/48.F,size,size});
      }
      const auto hp=text.find("HP 9999"), sh=text.find("SH 9999"), points=text.find("SCORE ");
      require(hp!=std::string::npos && sh!=std::string::npos && points!=std::string::npos, "maximum hull/shield and distinct SCORE labels remain visible");
      const auto advance_end = [&](std::size_t start) {
        const auto last=start+6;
        return origins[last].x + origins[last].x-origins[last-1].x;
      };
      require(origins[sh].x >= advance_end(hp)+6, "measured HP and shield keep a visible gap at maximum values");
      require(origins[points].x >= advance_end(sh)+6, "score cannot collide with maximum shield or optional level");
      if (score==9999) require(text.find("SCORE 9999")!=std::string::npos, "four-digit score remains fully visible inside one row");
      if (phone.width==320) require(std::abs(pause->bounds.bottom()-64.0711F)<.02F, "compact correction preserves exact existing Pause/source-fit boundary");
      std::cout << "HUD phone=" << phone.width << 'x' << phone.height << " score=" << score << " occupied_bottom=" << occupied_bottom << " ingress_guard=" << pause->bounds.bottom() << '\n';
    }
  }
}

void current_build_separates_temporary_and_permanent_identity() {
  progression_tests::MemoryBackend backend;
  hr::progression::AccountStore store{backend};
  require(store.load().playable(), "build fresh account loads");
  hr::gameplay::Game game;
  game.start();
  auto run = game.snapshot();
  run.modules[0] = 3;
  run.modules[1] = 2;
  run.research[0] = 15;
  run.research[1] = 40;
  run.evolution_bits = 1;
  View view{Screen::pause, &run, &store, {0,0,320,568}, {0,20,0,0}};
  view.show_build = true;
  for (const auto scale : {1.F, 1.3F, 1.6F, 2.F, 3.2F}) {
    view.text_scale = scale;
    view.ui_page = 0;
    const auto count = interface(view).page_count;
    std::string copy;
    for (unsigned i = 0; i < count; ++i) {
      view.ui_page = i;
      const auto page = interface(view);
      readable_page_is_real_safe_and_scaled(view, page);
      copy += page.page_text;
      require(std::any_of(page.buttons.begin(), page.buttons.end(), [](const auto& b) { return b.command == Command::current_build; }), "build always has fixed Back to paused flight");
    }
    const auto text = compact(copy);
    require(text.find("2/6hardwareslots") != std::string::npos && text.find("rank3of3") != std::string::npos, "build exposes actual installed slot occupancy and ranks");
    require(text.find("TEMPORARYFLIGHTRESEARCH") != std::string::npos && text.find("PERMANENTHANGAR") != std::string::npos, "temporary and permanent progression are explicitly distinct");
    require(text.find("WeaponDamage:+15%ofbase") != std::string::npos && text.find("HullReinforcement:+40points") != std::string::npos, "research values keep their correct units");
    require(text.find("GreatVacuum") != std::string::npos && text.find("GreatVacuum", text.find("GreatVacuum") + 1) == std::string::npos, "active evolution is named exactly once");
  }
}

}
int main() {
  compact_phone_cards_preserve_effects_and_scene_stage();
  larger_text_preserves_whole_offers_and_all_menu_actions();
  current_build_separates_temporary_and_permanent_identity();
  small_preferences_preserve_type_growth_and_useful_menus();
  compact_hud_maximum_values_preserve_spacing_and_ingress_bound();
  std::cout << "Portrait cards preserve full effect copy, scene exposure and safe touch targets\n";
}
