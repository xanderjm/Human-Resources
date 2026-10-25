#include "progression_test_backend.h"
#include "test_support.h"
#include <human_resources/product/presentation.h>
#include <human_resources/product/accessibility.h>
#include <human_resources/product/controls.h>
#include <human_resources/gameplay/tuning.h>
#include <cctype>
#include <limits>
#include <map>
#include <set>
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <human_resources/assets/preview_mesh.h>

namespace {
namespace hr = human_resources;
using namespace hr::product;
using test_support::require;
bool glyph(const hr::presentation::ScreenQuad& q) {
  return std::abs(q.uv[2] - 64.F / 1024.F) < .00001F && std::abs(q.uv[3] - 80.F / 2048.F) < .00001F;
}
hr::assets::PreviewMesh atlas;
hr::ui::Rect actual_glyph_ink(const hr::presentation::ScreenQuad& q, const hr::ui::Rect& viewport) {
  const unsigned x = static_cast<unsigned>(std::lround(q.uv[0] * static_cast<float>(atlas.width)));
  const unsigned y = static_cast<unsigned>(std::lround(q.uv[1] * static_cast<float>(atlas.height)));
  unsigned left = 64, top = 80, right = 0, bottom = 0;
  for (unsigned cy = 0; cy < 80; ++cy) for (unsigned cx = 0; cx < 64; ++cx)
    if (std::to_integer<unsigned>(atlas.rgba[(static_cast<std::size_t>(y + cy) * atlas.width + x + cx) * 4 + 3])) {
      left = std::min(left, cx); top = std::min(top, cy); right = std::max(right, cx + 1); bottom = std::max(bottom, cy + 1);
    }
  if (right == 0) return {};
  left = left ? left - 1 : 0; top = top ? top - 1 : 0;
  right = std::min(64U, right + 1); bottom = std::min(80U, bottom + 1);
  return {viewport.x + q.rect[0] * viewport.width + q.rect[2] * viewport.width * static_cast<float>(left) / 64,
      viewport.y + q.rect[1] * viewport.height + q.rect[3] * viewport.height * static_cast<float>(top) / 80,
      q.rect[2] * viewport.width * static_cast<float>(right - left) / 64,
      q.rect[3] * viewport.height * static_cast<float>(bottom - top) / 80};
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
    std::string body_copy;
    for (const auto& q : ui.quads) {
      if (!glyph(q) || q.color[0] != .035F || q.color[1] != .105F) continue;
      const float x = q.rect[0] * phone.width, y = q.rect[1] * phone.height;
      if (y >= first.y && y < first.bottom()) {
        const unsigned column = static_cast<unsigned>(std::lround(q.uv[0] * 1024.F / 64.F));
        const unsigned row = static_cast<unsigned>(std::lround(q.uv[1] * 2048.F / 80.F));
        if (row * 16 + column != 0) body_copy += static_cast<char>(32 + row * 16 + column);
        require(x >= first.x && x < first.right(), "effect glyph starts inside visible card frame");
        const float size = q.rect[2] * phone.width * 48.F / 64.F;
        require(size >= 13.99F, "effect text remains at least fourteen points");
        // Atlas glyph quads contain transparent bearings; check the actual line
        // origin plus one em against the cream panel's lower edge.
        require(y + size * (1.F + 8.F / 48.F) <= first.bottom() - 2.F, "last effect line stays within card");
      }
    }
    std::string expected_copy;
    for (const char ch : effect) if (ch != ' ') expected_copy += ch;
    require(body_copy.find(expected_copy) != std::string::npos, "long complete effect copy is rendered without truncation beside optional benefit/evolution context");
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
    if (view.screen == Screen::run) {
      const auto ink = actual_glyph_ink(q, view.viewport);
      require(ink.width == 0 || (ink.x >= safe.x - .01F && ink.y >= safe.y - .01F && ink.right() <= safe.right() + .01F && ink.bottom() <= safe.bottom() + .01F), "actual consumed-atlas RUN ink stays inside safe area, including enlarged transparent bearings");
    } else {
      require(x >= safe.x && x < safe.right() && y >= safe.y && y + size <= safe.bottom(), "measured glyph origins and line height stay in safe area");
    }
    const unsigned col = static_cast<unsigned>(std::lround(q.uv[0] * 1024.F / 64.F));
    const unsigned row = static_cast<unsigned>(std::lround(q.uv[1] * 2048.F / 80.F));
    rendered += static_cast<char>(32 + row * 16 + col);
    glyph_lines.push_back({x,y,size,size});
  }
  require(compact(rendered).find(compact(page.page_text)) != std::string::npos, "complete spoken page is also rendered through actual atlas quads");
  if (view.screen != Screen::run && !page.page_text.empty() && !page.buttons.empty()) {
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
        // Starter Dash has its own lower control. This assertion owns only the
        // tactical top HUD that constrains hostile ingress.
        if (top > pause->bounds.bottom()) continue;
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

void upgrade_board_exposes_actual_build_and_projected_benefits() {
  progression_tests::MemoryBackend backend;
  hr::progression::AccountStore store{backend};
  require(store.load().playable(), "decision board account loads");
  hr::gameplay::Game game;
  game.start();
  auto run = game.snapshot();
  run.modules[0] = 3;
  run.modules[1] = 1;
  run.choices[0] = {.hardware = true, .module = hr::gameplay::Module::phase_drive, .next_rank = 1, .before = .35F, .after = .5F, .title = "Phase Drive", .description = "Dash protection: 0.35 -> 0.5s."};
  run.choices[1] = {.hardware = true, .module = hr::gameplay::Module::beam_splitter, .next_rank = 2, .before = 2, .after = 3, .title = "Beam Splitter", .description = "Biological targets: 2 -> 3."};
  run.choices[2] = {.hardware = false, .research = hr::gameplay::Research::weapon_damage, .before = 20, .after = 25, .title = "Weapon Damage", .description = "Original laser damage: 20 -> 25 per shot."};
  for (const auto phone : std::array<hr::ui::Rect, 2>{{{0, 0, 320, 568}, {0, 0, 390, 844}}}) {
    View view{Screen::upgrade, &run, &store, phone, {0, 20, 0, 0}};
    const auto ui = interface(view);
    const auto copy = compact(atlas_copy(ui));
    require(copy.find("2/6HARDWARE") != std::string::npos, "decision board exposes real hardware occupancy before selection");
    require(copy.find("NEWHARDWARE") != std::string::npos && copy.find("RANKUP/2OF3") != std::string::npos && copy.find("FLIGHTRESEARCH") != std::string::npos, "new slots, installed ranks and research are distinct decisions");
    for (const auto& choice : run.choices)
      require(copy.find(compact(choice.description)) != std::string::npos, "complete actual effects remain present beside benefit hierarchy");
    require(copy.find("KEY1")!=std::string::npos && copy.find("KEY2")!=std::string::npos && copy.find("KEY3")!=std::string::npos,"choice shortcuts explicitly say Key and cannot masquerade as hardware ranks");
    if(phone.width==390) {
      float last_bottom=0;
      for(const auto& button:ui.buttons)last_bottom=std::max(last_bottom,button.bounds.bottom());
      require(last_bottom<700,"short real effects keep the decision board compact with paused world visible");
    }
    for(float scale:{2.F,3.2F}) {
      auto readable=view;readable.text_scale=scale;
      for(unsigned preview=0;preview<3;++preview) {
        readable.upgrade_preview=preview;
        const auto first=interface(readable);
        require(first.page_text.find("HARDWARE")==std::string::npos || first.page_text.find('>')<first.page_text.find("HARDWARE"),"benefit precedes admission metadata in the enlarged first offer page");
        require(first.page_text.find('>')!=std::string::npos,"actual before-after benefit is visible on the first enlarged preview page");
      }
    }
    if (phone.width == 390)
      require(copy.find("0.35>0.5s") != std::string::npos, "projected fractional benefit does not round away actual protection timing");
    for (unsigned i = 0; i < 3; ++i) {
      const auto command = static_cast<Command>(static_cast<unsigned>(Command::pick0) + i);
      require(std::any_of(ui.buttons.begin(), ui.buttons.end(), [&](const auto& b) { return b.command == command && b.enabled; }), "all three cards are directly selectable at ordinary mobile size");
    }
    const auto semantic = accessibility_snapshot(view, ui);
    require(std::any_of(semantic.begin(), semantic.end(), [](const auto& node) { return node.value.find("Uses one of six hardware slots") != std::string::npos; }), "new hardware slot consequence is available to assistive input");
  }
}
void grouped_build_keeps_full_loadout_research_and_hangar_in_three_pages() {
  progression_tests::MemoryBackend backend;
  hr::progression::AccountStore store{backend};
  require(store.load().playable(), "grouped build account loads");
  hr::gameplay::Game game;
  game.start();
  auto run = game.snapshot();
  for (const bool populated : {false, true}) {
    run.modules.fill(0);
    run.research.fill(0);
    if (populated) {
      for (unsigned i = 0; i < 6; ++i)
        run.modules[i] = 3;
      for (auto& amount : run.research)
        amount = 15;
    }
    run.evolution_bits = populated ? 3 : 0;
    for (const auto phone : std::array<hr::ui::Rect, 2>{{{0, 0, 320, 568}, {0, 0, 390, 844}}}) {
      View view{Screen::pause, &run, &store, phone, {0, 20, 0, 0}};
      view.show_build = true;
      std::string all_copy;
      for (unsigned page = 0; page < 3; ++page) {
        view.ui_page = page;
        const auto ui = interface(view);
        require(ui.page == page && ui.page_count == 3, "ordinary build has three grouped pages even with a complete loadout");
        readable_page_is_real_safe_and_scaled(view, ui);
        all_copy += ui.page_text;
      }
      const auto text = compact(all_copy);
      require(text.find(populated ? "6/6hardwareslots" : "0/6hardwareslots") != std::string::npos, "empty and occupied slots are explicit");
      if (populated) {
        for (unsigned i = 0; i < 6; ++i)
          require(text.find(compact(hr::gameplay::tuning::module_names[i])) != std::string::npos, "all six installed hardware names survive grouped overview");
        for (const auto name : hr::gameplay::tuning::research_names)
          require(text.find(compact(name)) != std::string::npos, "all seven actual research entries survive grouped overview");
      } else
        require(text.find("Emptyhardwareslot") != std::string::npos && text.find("Noresearchearned") != std::string::npos, "an empty build explains what remains to earn");
    }
  }
}

void harvest_feedback_explains_sweeps_boost_and_dash_without_expanding_top_hud() {
  progression_tests::MemoryBackend backend;
  hr::progression::AccountStore store{backend};
  require(store.load().playable(), "harvest feedback account loads");
  hr::gameplay::Game game;
  game.start();
  auto run = game.snapshot();
  run.stats.active_seconds = 30;
  run.modules[static_cast<unsigned>(hr::gameplay::Module::phase_drive)] = 0;
  for (const auto phone : std::array<hr::ui::Rect,2>{{{0,0,320,568},{0,0,390,844}}}) {
    for (const float scale : {1.F, 1.3F, 2.F, 3.2F}) {
      View view{Screen::run, &run, &store, phone, {0,20,0,0}};
      view.text_scale = scale;
      run.harvest_chain = 0;
      run.harvest_overdrive_seconds = 0;
      const auto idle = interface(view);
      const auto original_pause = std::find_if(idle.buttons.begin(), idle.buttons.end(), [](const auto& b){return b.command == Command::pause;})->bounds;
      for (const bool boosted : {false,true}) {
        run.harvest_chain = boosted ? 0 : 2;
        run.harvest_chain_seconds = 3.2F;
        run.harvest_overdrive_seconds = boosted ? 4.2F : 0;
        const auto ui = interface(view);
        const auto copy = compact(atlas_copy(ui));
        require(copy.find(boosted ? (scale == 1 ? "OVERDRIVE5s" : scale >= 3.2F ? "BOOST" : "BOOST5s") : scale >= 3.2F ? "2/3" : "SWEEP2/3") != std::string::npos, "actual timed sweep/Overdrive state is visible without inferring reward from animation");
        const auto pause = std::find_if(ui.buttons.begin(), ui.buttons.end(), [](const auto& b){return b.command == Command::pause;})->bounds;
        require(pause.y == original_pause.y && pause.bottom() == original_pause.bottom(), "harvest payoff leaves tactical top ingress and Pause bounds unchanged");
        if (boosted && scale == 1) {
          const auto safe = hr::ui::inset(view.viewport, view.safe_area);
          const float fit = std::min(safe.width / 390.F, safe.height / 844.F);
          const float ox = safe.x + (safe.width - 390.F * fit) * .5F;
          const float oy = safe.y + (safe.height - 844.F * fit) * .5F;
          const hr::ui::Rect panel{ox + 20 * fit, oy + 752 * fit, 190 * fit, 60 * fit};
          unsigned caption_glyphs = 0;
          for (const auto& q : ui.quads) {
            if (!glyph(q)) continue;
            const float size = q.rect[2] * phone.width * 48.F / 64.F;
            const float origin_y = q.rect[1] * phone.height + size * 8.F / 48.F;
            if (std::abs(origin_y - (oy + 785 * fit)) > .1F) continue;
            ++caption_glyphs;
            const hr::ui::Rect ink_guard{q.rect[0] * phone.width, origin_y, q.rect[2] * phone.width, size * 1.25F};
            require(size >= 13.99F, "Overdrive caption retains the readable physical text floor");
            require(ink_guard.x >= panel.x && ink_guard.right() <= panel.right() &&
                ink_guard.y >= panel.y && ink_guard.bottom() <= panel.bottom() + .1F,
                "Overdrive caption including horizontal atlas bearings stays inside its existing mint panel, clear of Dash");
          }
          require(caption_glyphs > 0, "ordinary Overdrive keeps a visible benefit caption");
        }
        if (scale > 1) readable_page_is_real_safe_and_scaled(view, ui);
        auto dash = std::find_if(ui.buttons.begin(), ui.buttons.end(), [](const auto& b){return b.command == Command::dash;});
        require(dash != ui.buttons.end() && dash->enabled && dash->label.find("DASH") != std::string::npos, "fresh run exposes immediately usable starter Dash action");
        const auto semantics = accessibility_snapshot(view, ui);
        require(std::any_of(semantics.begin(), semantics.end(), [&](const auto& n){return n.value.find(boosted ? "Overdrive active" : "Sweep chain 2 of 3") != std::string::npos;}), "native spoken tactical summary describes the actual reward and time");
        if (boosted) require(std::any_of(semantics.begin(), semantics.end(), [](const auto& n){return n.value.find("Capture and firing are boosted.") != std::string::npos;}), "concise visual Overdrive caption retains the full accessible benefit description at every text size");
      }
      run.dash_cooldown_seconds = 1.2F;
      const auto charging = interface(view);
      const auto dash = std::find_if(charging.buttons.begin(), charging.buttons.end(), [](const auto& b){return b.command == Command::dash;});
      require(dash != charging.buttons.end() && !dash->enabled && dash->label == "DASH 2s", "Dash cooldown is named, rounded up and genuinely disabled until ready");
      run.dash_cooldown_seconds = 0;
    }
  }
}

void compact_choice_titles_stay_on_the_dark_frame() {
  progression_tests::MemoryBackend backend;
  hr::progression::AccountStore store{backend};
  require(store.load().playable(), "compact header account loads");
  hr::gameplay::Game game; game.start();
  auto run=game.snapshot();
  const std::array<std::string,3> names{"Tractor Lens","Repair Swarm","Shield Generator"};
  for(unsigned i=0;i<3;++i) run.choices[i]={.hardware=true,.module=hr::gameplay::Module::tractor_lens,.next_rank=1,.title=names[i],.description="Capture radius: 4 -> 4.4m. Beams hold0.3s out of range."};
  for(const auto phone:std::array<hr::ui::Rect,2>{{{0,0,320,568},{0,0,390,844}}}) {
    View view{Screen::upgrade,&run,&store,phone,{0,20,0,0}};
    const auto ui=interface(view);
    std::string copy;
    std::vector<hr::ui::Rect> lines;
    std::vector<float> body_tops;
    for(const auto& q:ui.quads) {
      if(std::abs(q.uv[1]-1612.F/2048.F)<.00001F && std::abs(q.uv[2]-464.F/1024.F)<.00001F)
        body_tops.push_back(q.rect[1]*phone.height);
      if(!glyph(q)) continue;
      const float size=q.rect[2]*phone.width*48.F/64.F;
      const unsigned col=static_cast<unsigned>(std::lround(q.uv[0]*16));
      const float bank=q.uv[1]*2048>=512?512.F:0.F;
      const unsigned row=static_cast<unsigned>(std::lround((q.uv[1]*2048-bank)/80));
      copy+=static_cast<char>(32+row*16+col);
      lines.push_back({q.rect[0]*phone.width+size*4.F/48.F,q.rect[1]*phone.height+size*8.F/48.F,size,size});
    }
    require(body_tops.size()==3,"each choice has a complete independent dark header and cream effect body");
    for(unsigned i=0;i<3;++i) {
      const auto title=copy.find(names[i]);
      require(title!=std::string::npos,"actual complete upgrade title remains rendered");
      for(std::size_t j=title;j<title+names[i].size();++j)
        require(lines[j].y+lines[j].height*1.25F<body_tops[i],"all title glyph lines and descender room remain on the dark header at the physical text floor");
    }
    const auto summary=copy.find("0/6 HARDWARE");
    require(summary!=std::string::npos,"hardware admission summary remains visible");
    const auto first=std::find_if(ui.buttons.begin(),ui.buttons.end(),[](const auto& b){return b.command==Command::pick0;})->bounds;
    require(lines[summary].y+lines[summary].height*1.25F+3<first.y,"first choice starts below the full hardware summary with visible spacing");
  }
}

void promotion_preserves_choice_safety_and_collected_return_controls() {
  progression_tests::MemoryBackend backend;
  hr::progression::AccountStore store{backend};
  require(store.load().playable(), "promotion account loads");
  hr::gameplay::Game game;
  game.start({.seed=514});
  for (unsigned step=0;step<20000 && game.snapshot().phase==hr::gameplay::Phase::active;++step) game.advance(1.F/60,{});
  require(game.snapshot().phase==hr::gameplay::Phase::choosing_upgrade,"public Game reaches a real earned promotion");
  const auto before=game.snapshot().stats.active_seconds;
  const auto chosen=game.snapshot().choices[0];
  PromotionFlow flow;
  flow.enter();
  View view{Screen::upgrade,&game.snapshot(),&store,{0,0,320,568},{0,20,0,0}};
  const auto settled=interface(view);
  for (float age : {0.F,.04F,.12F,.24F}) {
    view.promotion={age,nullptr,false};
    const auto entry=interface(view);
    require(entry.buttons.size()==settled.buttons.size(),"entry never hides or replaces any choice action");
    for (unsigned i=0;i<entry.buttons.size();++i) {
      const auto& a=entry.buttons[i]; const auto& b=settled.buttons[i];
      require(a.command==b.command && a.enabled==b.enabled && a.bounds.x==b.bounds.x && a.bounds.y==b.bounds.y && a.bounds.width==b.bounds.width && a.bounds.height==b.bounds.height,"all promotion touch targets remain stationary through card entry");
    }
    require(atlas_copy(entry)==atlas_copy(settled),"every complete mechanic remains present from first entry frame");
  }
  view.promotion={0,nullptr,true};
  const auto reduced=interface(view);
  require(reduced.quads.size()==settled.quads.size(),"reduced motion uses the settled board without moving progress accents");
  for (unsigned i=0;i<reduced.quads.size();++i) require(reduced.quads[i].rect==settled.quads[i].rect && reduced.quads[i].color==settled.quads[i].color,"reduced motion retains a completely stationary full-opacity draft");
  require(game.choose_upgrade(0),"earned selection applies immediately");
  require(hr::gameplay::inspect_saved_state(game.save_state()).valid(),"selection can be saved before cosmetic confirmation");
  flow.confirm(chosen);
  hr::product::Controls controls;
  hr::input::Action choice_contact{.id=static_cast<hr::input::ActionId>(ControlAction::contact),.device=1,.phase=hr::input::ActionPhase::started,.value=1,.position={150,200},.has_position=true,.contact=17,.source_kind=hr::input::DeviceKind::touch};
  controls.apply(choice_contact,false,true);
  choice_contact.phase=hr::input::ActionPhase::changed;
  choice_contact.position.x=194;
  controls.apply(choice_contact,true,false);
  require(!controls.dragging(),"the menu selection contact cannot become a flight drag without a new press");
  hr::input::Action right{.id=static_cast<hr::input::ActionId>(ControlAction::right),.device=2,.phase=hr::input::ActionPhase::started,.value=1,.source_control=7};
  hr::input::Action dash=right;
  dash.id=static_cast<hr::input::ActionId>(ControlAction::dash);
  controls.apply(right,true,false);
  controls.apply(dash,true,false);
  flow.advance(.119F);
  require(flow.holds_combat() && game.snapshot().stats.active_seconds==before,"confirmation cannot advance enemies, projectiles or the run clock");
  view.screen=Screen::run;
  view.promotion=flow.frame();
  const auto confirming=interface(view);
  require(accessibility_command({100+static_cast<hr::ui::NodeId>(Command::pick0),hr::accessibility::ActionKind::activate},confirming)==Command::none,"old choice action cannot leak into confirmation/next draft");
  for (float scale : {1.F,1.3F,2.F,3.2F}) {
    view.text_scale=scale;
    view.promotion=flow.frame(true);
    const auto receipt=interface(view);
    require(receipt.page_text.find(chosen.title)!=std::string::npos,"confirmation names the actual installed choice");
    require(!receipt.text_overflow,"confirmation preserves complete actual title at requested text size");
    require(compact(atlas_copy(receipt)).find("Installed")!=std::string::npos || (scale>1 && compact(atlas_copy(receipt)).find("ADDED")!=std::string::npos),"compact receipt visibly acknowledges the saved choice at every text size");
    const auto semantics=accessibility_snapshot(view,receipt);
    require(semantics[1].value.find("Installed: "+chosen.title)!=std::string::npos,"compact receipt retains exact installed title in native spoken context");
    const auto pause=std::find_if(receipt.buttons.begin(),receipt.buttons.end(),[](const auto& b){return b.command==Command::pause;})->bounds;
    auto ordinary_view=view; ordinary_view.promotion={};
    for(std::size_t i=interface(ordinary_view).quads.size();i<receipt.quads.size();++i) {
      const auto& q=receipt.quads[i];
      if(q.uv[2]>=0)continue;
      require(q.rect[1]*view.viewport.height>=pause.bottom(),"nonblocking receipt sits below HUD rather than over the ship at screen centre");
      require(q.rect[3]*view.viewport.height<=14*scale+16+.01F || q.rect[3]*view.viewport.height<=36.01F,"receipt is one measured line at every text preference");
    }
  }
  flow.advance(.002F);
  require(!flow.holds_combat(),"confirmation releases after120ms without countdown");
  require(flow.frame().installed,"exact installed choice remains available after combat resumes");
  flow.advance(.68F);
  require(!flow.frame().installed,"nonblocking installed receipt expires at0.8seconds");
  const auto returned=controls.sample();
  require(returned.movement.x>0 && returned.dash,"movement and Dash pressed during confirmation reach first resumed sample");
  require(!controls.sample().dash,"queued Dash is consumed exactly once");
  game.advance(1.F/60,returned);
  require(game.snapshot().stats.active_seconds>before,"first returned control advances actual Game");
  flow.enter();
  require(!flow.holds_combat() && !flow.frame().installed,"a queued earned draft clears old selected receipt");
  flow.confirm(chosen);
  flow.clear(); controls.clear();
  require(!flow.holds_combat() && !flow.frame().installed && controls.sample().movement.x==0,"pause, focus loss or protected-save interruption cancels cosmetics and controls");
}

void actual_damage_feedback_stays_inside_the_tactical_hud() {
  progression_tests::MemoryBackend backend;
  hr::progression::AccountStore store{backend};
  require(store.load().playable(),"damage account loads");
  hr::gameplay::Game game;game.start();
  auto run=game.snapshot();run.stats.active_seconds=30;
  for (const auto viewport : {hr::ui::Rect{0,0,320,568}, hr::ui::Rect{0,0,390,844}, hr::ui::Rect{0,0,844,390}})
  for (float scale : {1.F,1.3F,1.6F,2.F,3.2F}) for (bool hull : {false,true})
  for (const auto cause : {hr::gameplay::DamageCause::unknown, hr::gameplay::DamageCause::contact, hr::gameplay::DamageCause::projectile}) {
    View view{Screen::run,&run,&store,viewport,{0,20,0,0}};
    view.text_scale=scale;
    const auto ordinary=interface(view);
    const auto pause=std::find_if(ordinary.buttons.begin(),ordinary.buttons.end(),[](const auto& b){return b.command==Command::pause;})->bounds;
    view.damage_feedback={.1F,hull,cause};
    const auto hit=interface(view);
    const auto copy=compact(atlas_copy(hit));
    const auto framing=gameplay_framing(viewport,view.safe_area,scale);
    const bool short_stage=framing.compact || scale>1;
    const std::string expected=short_stage?std::string(hull?"HP":"SH")+(framing.compact?"":":")+(cause==hr::gameplay::DamageCause::contact?"BUMP":cause==hr::gameplay::DamageCause::projectile?"SHOT":"HIT"):std::string(damage_label(view.damage_feedback));
    require(copy.find(compact(expected))!=std::string::npos,"committed hit identifies agreed full or compact actual cause and affected defence at every text size");
    const auto after=std::find_if(hit.buttons.begin(),hit.buttons.end(),[](const auto& b){return b.command==Command::pause;})->bounds;
    require(after.x==pause.x && after.y==pause.y && after.height==pause.height,"hit feedback preserves Pause and tactical field height");
    if(scale>1) readable_page_is_real_safe_and_scaled(view,hit);
    const auto spoken=accessibility_snapshot(view,hit);
    require(std::any_of(spoken.begin(),spoken.end(),[&](const auto& n){return n.value.find(damage_announcement(view.damage_feedback))!=std::string::npos;}),"native context describes committed damage without repeating an age countdown");
    view.damage_feedback.age=.4F;
    require(accessibility_snapshot(view,interface(view))[1].value==spoken[1].value,"damage age does not create per-frame spoken announcements");
  }
}

void bounded_damage_hud_keeps_ingress_vitality_clear() {
  progression_tests::MemoryBackend backend;
  hr::progression::AccountStore store{backend};
  require(store.load().playable(), "bounded-hit ingress account loads");
  hr::gameplay::Game game; game.start();
  auto run = game.snapshot(); run.stats.active_seconds = 30;
  unsigned visible_meters = 0;
  for (const auto viewport : {hr::ui::Rect{0,0,320,568}, hr::ui::Rect{0,0,390,844}, hr::ui::Rect{0,0,768,1024}})
    for (float scale : {1.3F, 2.F, 3.2F}) for (float z : {5.F, 9.5F, 13.F, 16.5F}) {
      View view{Screen::run,&run,&store,viewport,{0,20,0,0}};
      view.text_scale = scale;
      view.damage_feedback = {0,false,hr::gameplay::DamageCause::contact};
      run.entities.clear();
      const auto base = interface(view);
      run.entities = {{.id=7, .kind=hr::gameplay::EntityKind::boss, .position={0,z}, .hull=100, .maximum_hull=160},
          {.id=8, .kind=hr::gameplay::EntityKind::tank, .position={4,z}, .hull=100, .maximum_hull=300}};
      run.presentation_events[0] = {1,30,hr::gameplay::PresentationEventKind::impact,8,{4,z}};
      const auto result = interface(view);
      require(result.damage_hud_bottom == 0, "actual hit cause never expands the bounded tactical row");
      const auto pause=std::find_if(result.buttons.begin(),result.buttons.end(),[](const auto& b){return b.command==Command::pause;})->bounds;
      const auto dash=std::find_if(result.buttons.begin(),result.buttons.end(),[](const auto& b){return b.command==Command::dash;})->bounds;
      for (std::size_t i=base.quads.size(); i<result.quads.size(); ++i) {
        const auto& q=result.quads[i];
        if (q.uv[2]>=0 || q.color!=std::array<float,4>{.035F,.105F,.17F,.96F}) continue;
        const float y=q.rect[1]*viewport.height, bottom=y+q.rect[3]*viewport.height;
        require(y>=pause.bottom()+11.99F && bottom<=dash.y-11.99F, "actual appended vitality panels clear both fixed controls during damage");
        ++visible_meters;
      }
    }
  require(visible_meters>0,"bounded damage row retains actual combat progress meters when they fit");
}

void enlarged_run_preserves_two_bands_and_complete_semantics() {
  progression_tests::MemoryBackend backend;
  hr::progression::AccountStore store{backend};
  require(store.load().playable(),"enlarged tactical account loads");
  hr::gameplay::Game game; game.start();
  const auto original=game.snapshot();
  hr::gameplay::UpgradeChoice installed; installed.title="Shield Generator";
  unsigned percentage_cases=0, fraction_cases=0, icon_cases=0;
  for (const auto viewport : {hr::ui::Rect{0,0,320,568},hr::ui::Rect{0,0,390,844},hr::ui::Rect{0,0,768,1024},hr::ui::Rect{0,0,1366,1024},hr::ui::Rect{0,0,320,320},hr::ui::Rect{0,0,320,500},hr::ui::Rect{17,29,320,500}})
    for (float scale : {1.3F,2.F,3.2F,3.4F}) for (unsigned inset_case=0;inset_case<3;++inset_case) for (unsigned state=0; state<18; ++state) {
      const bool small=viewport.width==320 && viewport.height<=500;
      if(inset_case && !small)continue;
      auto run=original; run.stats.active_seconds=30; run.stats.level=24;
      run.hull=112; run.maximum_hull=130; run.shield=80; run.maximum_shield=100;
      run.xp=50; run.next_level_xp=100;
      const hr::ui::Insets insets{inset_case==1?8.F:inset_case==2?12.F:0.F,viewport.width==390?59.F:20.F,inset_case==1?10.F:inset_case==2?12.F:0.F,viewport.width==390?34.F:inset_case==2?16.F:0.F};
      const auto framing=gameplay_framing(viewport,insets,scale,{static_cast<unsigned>(viewport.width*2),static_cast<unsigned>(viewport.height*2)});
      if((framing.compact && !framing.banded) || (scale>3.2F && !framing.banded))continue;
      const auto safe=framing.constrained?framing.run_layout:framing.safe;
      View view{Screen::run,&run,&store,viewport,insets}; view.text_scale=scale; view.framing=&framing;
      if(state==1){run.hull=run.maximum_hull=130;run.shield=run.maximum_shield=40;}
      if(state==2){run.hull=run.maximum_hull=100000000;run.shield=run.maximum_shield=100000000;}
      if(state>=3 && state<9)view.damage_feedback={.1F,state%2==0,static_cast<hr::gameplay::DamageCause>((state-3)/2)};
      if(state==9){run.harvest_chain=2;run.harvest_chain_seconds=3.2F;}
      if(state==10 || state==11)run.harvest_overdrive_seconds=5.5F;
      if(state==11){view.promotion={.1F,&installed,false};view.damage_feedback={.1F,true,hr::gameplay::DamageCause::projectile};}
      if(state==12)run.stats.active_seconds=2;
      if(state==13)run.stats.active_seconds=8;
      if(state==14)run.dash_cooldown_seconds=4;
      if(state==15)run.stats.level=1000000;
      if(state==16){run.hull=99000000;run.maximum_hull=100000000;run.harvest_overdrive_seconds=5.5F;}
      if(state==17){run.dash_cooldown_seconds=4;view.promotion={.1F,&installed,false};}
      const auto ui=interface(view);
      require(!ui.text_overflow,"every bounded tactical state fits at actual requested font size");
      const auto pause=std::find_if(ui.buttons.begin(),ui.buttons.end(),[](const auto& b){return b.command==Command::pause;})->bounds;
      const auto dash=std::find_if(ui.buttons.begin(),ui.buttons.end(),[](const auto& b){return b.command==Command::dash;})->bounds;
      test_support::require_near(pause.y,safe.y,.01F,"Pause starts at actual safe stage top");
      test_support::require_near(pause.height,44,.01F,"Pause never grows into flight field");
      test_support::require_near(dash.bottom(),safe.bottom()-12,.01F,"Dash retains safe bottom margin");
      require(dash.width>=44 && dash.height==44,"actual Dash target remains complete forty-four points");
      for(const auto& q:ui.quads) {
        hr::ui::Rect bounds{viewport.x+q.rect[0]*viewport.width,viewport.y+q.rect[1]*viewport.height,q.rect[2]*viewport.width,q.rect[3]*viewport.height};
        if(glyph(q)) {
          require(q.rect[2]*viewport.width*48.F/64.F>=14*scale-.01F,"bounded rows never shrink requested readable type");
          bounds=actual_glyph_ink(q,viewport);
          if(bounds.width==0)continue;
        }
        require(bounds.x>=safe.x-.01F && bounds.right()<=safe.right()+.01F,"entire actual ink/control stays within safe stage sides");
        require((bounds.y>=safe.y-.01F && bounds.bottom()<=safe.y+44.01F) || (bounds.y>=dash.y-.01F && bounds.bottom()<=dash.bottom()+.01F),"all actual glyph alpha, panels and controls remain in the two bounded strips");
      }
      const auto copy=compact(atlas_copy(ui));
      if(state==2){
        const bool percentage=copy.find("HP100%")!=std::string::npos || (framing.banded && copy.find("HPMAX")!=std::string::npos);
        percentage_cases+=percentage?1U:0U;
        const bool dual_fraction=framing.banded && copy.find("HP")!=std::string::npos && copy.find("SH")!=std::string::npos;
        require((percentage || dual_fraction || copy.find("HP100000000")!=std::string::npos) && copy.find("SH")!=std::string::npos,"uncapped health retains full numeric value, honest labelled percentage/full state, or explicit HP/SH fractions");
      }
      if(framing.banded) {
        if(copy.find(">>")!=std::string::npos)++icon_cases;
        const auto overlap=hr::ui::intersect(dash,framing.stage);
        require(overlap.width==0 || overlap.height==0,"banded Dash target remains outside the exact world destination");
        auto changed=run;changed.dash_cooldown_seconds=run.dash_cooldown_seconds>0?0:4;
        auto other=view;other.run=&changed;
        const auto other_ui=interface(other);
        const auto other_dash=std::find_if(other_ui.buttons.begin(),other_ui.buttons.end(),[](const auto& b){return b.command==Command::dash;})->bounds;
        require(other_dash.x==dash.x && other_dash.y==dash.y && other_dash.width==dash.width && other_dash.height==dash.height,"measured Dash icon/cooldown target is stationary across ready state changes");
        if(state==10 || state==11 || state==16 || state==17)
          require(copy.find(state==11 || state==17?"ADDED":"BOOST")!=std::string::npos,"narrow-safe banded layout retains complete primary status words");
        if(state==2 || state==16) {
          const auto hp_background=std::find_if(ui.quads.begin(),ui.quads.end(),[&](const auto& q){return q.uv[2]<0 && q.color==std::array<float,4>{.06F,.16F,.23F,.97F} && std::abs(viewport.x+q.rect[0]*viewport.width-(safe.x+4))<.01F && std::abs(viewport.y+q.rect[1]*viewport.height-(safe.y+42))<.01F;});
          if(copy.starts_with("HPSH"))require(hp_background!=ui.quads.end(),"qualitative HP/SH labels cannot silently lose the actual HP fraction bar");
          if(hp_background!=ui.quads.end()) {
            ++fraction_cases;
            const auto hp_fill=std::next(hp_background);
            require(hp_fill!=ui.quads.end() && hp_fill->color==std::array<float,4>{.39F,.96F,.75F,1},"qualitative HP fallback has an actual labelled fraction fill");
            test_support::require_near(hp_fill->rect[2]/hp_background->rect[2],run.hull/run.maximum_hull,.0001F,"HP fraction retains authoritative full or partial health");
          }
        }
      }
      if(state==11)require(copy.find("ADDED")!=std::string::npos && copy.find("HP:SHOT")!=std::string::npos && copy.find("BOOST")==std::string::npos,"saved installation and actual hit retain priority without an extra strip");
      const auto spoken=accessibility_snapshot(view,ui);
      require(spoken[1].value.find("Hull "+std::to_string(static_cast<unsigned>(run.hull)))!=std::string::npos && spoken[1].value.find("Shield "+std::to_string(static_cast<unsigned>(run.shield)))!=std::string::npos,"full absolute defences survive qualitative visual fallback");
      if(state==11)require(spoken[1].value.find("Installed: Shield Generator")!=std::string::npos && spoken[1].value.find(damage_announcement(view.damage_feedback))!=std::string::npos,"full installed title and exact damage cause/layer remain spoken");
    }
  require(percentage_cases>0,"extreme size/value matrix genuinely exercises bounded percentage fallback");
  require(fraction_cases>0 && icon_cases>0,"narrow-safe matrix exercises real labelled HP fractions and the measured ready Dash icon");
}
void banded_framing_keeps_pixel_fit_and_global_contacts() {
  progression_tests::MemoryBackend backend;
  hr::progression::AccountStore store{backend};
  (void)store.load();
  hr::gameplay::Game game;game.start();
  const auto run=game.snapshot();
  {
    const hr::ui::Rect viewport{0,0,320,500};
    const hr::ui::Insets insets{60,20,60,16};
    const auto framing=gameplay_framing(viewport,insets,1,{640,1000});
    require(framing.banded,"safe-area budget alone may require the explicit strip mode at ordinary type");
    View view{Screen::run,&run,&store,viewport,insets};view.framing=&framing;
    const auto ui=interface(view);
    require(!ui.text_overflow,"explicit strip mode retains the ordinary physical font floor");
    const auto pause=std::find_if(ui.buttons.begin(),ui.buttons.end(),[](const auto& b){return b.command==Command::pause;})->bounds;
    require(pause.height==44 && pause.y==framing.safe.y,"explicit band layout dispatch is authoritative even without enlarged text");
  }
  for(const auto viewport:{hr::ui::Rect{0,0,320,320},hr::ui::Rect{17,29,320,500}})
    for(float scale:{3.2F,3.4F})for(unsigned dpi:{1U,2U,3U}) {
      const hr::presentation::PixelExtent pixels{static_cast<unsigned>(viewport.width)*dpi+(dpi==3?1U:0U),static_cast<unsigned>(viewport.height)*dpi-(dpi==3?1U:0U)};
      const auto framing=gameplay_framing(viewport,{8,12,10,8},scale,pixels);
      require(framing.banded && framing.compact && framing.run_layout.x==framing.safe.x && framing.run_layout.width==framing.safe.width,"starved rails select one full-safe strip layout value");
      const auto actual=hr::presentation::fit_world_destination(framing.render_extent.width,framing.render_extent.height,pixels.width,pixels.height,framing.destination);
      test_support::require_near(framing.stage.x,viewport.x+static_cast<float>(actual.x)*viewport.width/static_cast<float>(pixels.width),.001F,"DPI and odd-pixel fit share actual absolute stage x");
      test_support::require_near(framing.stage.y,viewport.y+static_cast<float>(actual.y)*viewport.height/static_cast<float>(pixels.height),.001F,"DPI and odd-pixel fit share actual absolute stage y");
      View view{Screen::run,&run,&store,viewport,{8,12,10,8}};view.text_scale=scale;view.framing=&framing;
      view.stick_active=true;view.stick_origin={viewport.x+24,viewport.y+100};view.stick_position={view.stick_origin.x+44,view.stick_origin.y};
      const auto ui=interface(view);
      require(!ui.text_overflow,"odd-DPI translated strip layout retains complete readable text");
      const auto& knob=ui.quads.back();
      test_support::require_near(viewport.x+knob.rect[0]*viewport.width,view.stick_origin.x+38-8,.001F,"floating stick remains in full-window points in clear flanks");
      test_support::require_near(viewport.y+knob.rect[1]*viewport.height,view.stick_origin.y-8,.001F,"floating stick receives no stage-local offset");
      hr::product::Controls controls;
      hr::input::Action contact{.id=static_cast<hr::input::ActionId>(ControlAction::contact),.device=1,.phase=hr::input::ActionPhase::started,.value=1,.position={view.stick_origin.x,view.stick_origin.y},.has_position=true,.contact=7,.source_kind=hr::input::DeviceKind::touch};
      controls.apply(contact,true,false);contact.phase=hr::input::ActionPhase::changed;contact.position={view.stick_position.x,view.stick_position.y};controls.apply(contact,true,false);
      test_support::require_near(controls.sample().movement.x,1,.0001F,"banded presentation retains ordinary forty-four-point semantic stick throw");
    }
}

void late_value_hud_keeps_level_at_small_portrait() {
  progression_tests::MemoryBackend backend;
  hr::progression::AccountStore store{backend};
  require(store.load().playable(), "late HUD fixture account");
  hr::gameplay::Game game;
  game.start();
  auto run = game.snapshot();
  run.hull = 112; run.maximum_hull = 130;
  run.shield = 80; run.maximum_shield = 100;
  run.stats.level = 24; run.stats.score = 2126;
  run.stats.active_seconds = 400;
  for (const auto viewport : std::array<hr::ui::Rect,2>{{{0,0,320,568},{0,0,390,844}}}) {
    View view{Screen::run,&run,&store,viewport,{0,20,0,0}};
    const auto ui = interface(view);
    const auto copy = compact(atlas_copy(ui));
    for (const auto value : {"HP112","SH80","LV24","SCORE2126","XP"})
      require(copy.find(value) != std::string::npos, "late normal HUD retains every actual value and experience label");
    for (const auto& q : ui.quads) if (glyph(q)) {
      const float size = q.rect[2] * viewport.width * 48.F / 64.F;
      require(size >= 13.99F, "late HUD does not shrink below readable floor");
    }
  }
}
} // namespace
int main(int argc, char** argv) {
  require(argc == 2, "actual cooked atlas directory is supplied");
  std::ifstream file(std::filesystem::path(argv[1]) / "ui-atlas.hrm", std::ios::binary | std::ios::ate);
  require(file.good(), "actual cooked UI atlas opens");
  const auto count = file.tellg();
  require(count > 0 && count < 16 * 1024 * 1024, "atlas fixture is bounded");
  std::vector<std::byte> bytes(static_cast<std::size_t>(count));
  file.seekg(0); file.read(reinterpret_cast<char*>(bytes.data()), count);
  require(hr::assets::decode_preview_mesh(bytes, atlas), "actual production atlas decodes");
  bounded_damage_hud_keeps_ingress_vitality_clear();
  enlarged_run_preserves_two_bands_and_complete_semantics();
  banded_framing_keeps_pixel_fit_and_global_contacts();
  late_value_hud_keeps_level_at_small_portrait();
  compact_choice_titles_stay_on_the_dark_frame();
  promotion_preserves_choice_safety_and_collected_return_controls();
  actual_damage_feedback_stays_inside_the_tactical_hud();
  harvest_feedback_explains_sweeps_boost_and_dash_without_expanding_top_hud();
  compact_phone_cards_preserve_effects_and_scene_stage();
  upgrade_board_exposes_actual_build_and_projected_benefits();
  grouped_build_keeps_full_loadout_research_and_hangar_in_three_pages();
  larger_text_preserves_whole_offers_and_all_menu_actions();
  current_build_separates_temporary_and_permanent_identity();
  small_preferences_preserve_type_growth_and_useful_menus();
  compact_hud_maximum_values_preserve_spacing_and_ingress_bound();
  std::cout << "Portrait cards preserve full effect copy, scene exposure and safe touch targets\n";
}
