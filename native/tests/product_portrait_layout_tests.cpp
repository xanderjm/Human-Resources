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
    require(x >= safe.x && x < safe.right() && y >= safe.y && y + size <= safe.bottom(), "measured glyph origins and line height stay in safe area");
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
        require(copy.find(boosted ? (scale == 1 ? "OVERDRIVE5s" : "BOOST5s") : "SWEEP2/3") != std::string::npos, "actual timed sweep/Overdrive state is visible without inferring reward from animation");
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
    require(compact(atlas_copy(receipt)).find("Installed")!=std::string::npos,"compact receipt visibly acknowledges the saved choice at every text size");
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
  for (float scale : {1.F,1.3F,2.F,3.2F}) for (bool hull : {false,true}) {
    View view{Screen::run,&run,&store,{0,0,320,568},{0,20,0,0}};
    view.text_scale=scale;
    const auto ordinary=interface(view);
    const auto pause=std::find_if(ordinary.buttons.begin(),ordinary.buttons.end(),[](const auto& b){return b.command==Command::pause;})->bounds;
    view.damage_feedback={.1F,hull};
    const auto hit=interface(view);
    const auto copy=compact(atlas_copy(hit));
    require(copy.find(hull ? "HULL" : "SHIELD")!=std::string::npos,"committed hit identifies the affected defence at every text size");
    const auto after=std::find_if(hit.buttons.begin(),hit.buttons.end(),[](const auto& b){return b.command==Command::pause;})->bounds;
    require(after.x==pause.x && after.y==pause.y && after.height==pause.height,"hit feedback preserves Pause and tactical field height");
    if(scale>1) readable_page_is_real_safe_and_scaled(view,hit);
    const auto spoken=accessibility_snapshot(view,hit);
    require(std::any_of(spoken.begin(),spoken.end(),[&](const auto& n){return n.value.find(hull?"Hull hit.":"Shield hit.")!=std::string::npos;}),"native context describes committed damage without repeating an age countdown");
    view.damage_feedback.age=.4F;
    require(accessibility_snapshot(view,interface(view))[1].value==spoken[1].value,"damage age does not create per-frame spoken announcements");
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
int main() {
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
