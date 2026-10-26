#include "progression_test_backend.h"
#include "test_support.h"
#include <human_resources/product/presentation.h>
#include <human_resources/product/accessibility.h>
#include <human_resources/product/controls.h>
#include "../human_resources_product/src/flight_report.h"
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
void pause_bank_disclosure_keeps_existing_actions_and_complete_copy() {
  progression_tests::MemoryBackend backend;
  hr::progression::AccountStore store{backend};
  require(store.load().playable(), "pause display fixture loads");
  hr::gameplay::Game game; game.start();
  hr::progression::RunReceipt receipt{.run_id=1, .active_milliseconds=75000, .score=85, .jeep_kills=4};
  for (const auto viewport : {hr::ui::Rect{0,0,320,568}, hr::ui::Rect{0,0,390,844}, hr::ui::Rect{0,0,844,390}}) {
    for (const float scale : {1.F,2.F}) {
      View view{Screen::pause, &game.snapshot(), &store, viewport, {0,20,0,0}};
      view.text_scale = scale; view.current_receipt = &receipt;
      for (const bool guest : {false,true}) {
        view.guest = guest;
        const auto bank = pause_bank_preview(view);
        auto ui = interface(view);
        std::string copy;
        for (unsigned page=0; page<ui.page_count; ++page) {
          view.ui_page=page; ui=interface(view);
          require(!ui.text_overflow, "pause reward uses complete measured pages at enlarged text");
          copy += compact(atlas_copy(ui));
          for (const auto& button : ui.buttons)
            require(button.bounds.width>=44 && button.bounds.height>=44, "pause action touch sizes remain unchanged");
        }
        require(copy.find(compact(bank.heading)) != std::string::npos && copy.find(compact(bank.detail)) != std::string::npos, "pause renders actual bank amount and complete status on phone wide and enlarged layouts");
      }
    }
  }
}
void hangar_shows_owned_and_next_power_with_visible_basis() {
  progression_tests::MemoryBackend backend;
  hr::progression::AccountStore store{backend};
  require(store.load().playable(), "Hangar display fixture loads");
  for (unsigned i = 0; i < 10; ++i) {
    require(store.begin_run().succeeded(), "fund display fixture through actual progression");
    require(store.settle({.run_id = store.account().checkpoint.run_id, .active_milliseconds = 3600000,
        .score = 1500, .captures = 710, .soldier_kills = 60, .jeep_kills = 4,
        .tank_kills = 3, .vehicle_captures = 2, .boss_kills = 2}).succeeded(), "bank fixture reward");
  }
  hr::gameplay::Game game; game.start();
  for (const unsigned rank : {0U, 1U, 4U, 5U}) {
    for (unsigned track = 0; track < 4; ++track)
      while (store.hangar()[track].rank < rank)
        require(store.purchase(static_cast<hr::progression::Track>(track)).succeeded(), "purchase actual displayed rank");
    for (const auto phone : {hr::ui::Rect{0,0,320,568}, hr::ui::Rect{0,0,390,844}}) {
      View view{Screen::hangar, &game.snapshot(), &store, phone, {0,20,0,0}};
      const auto ui = interface(view);
      const auto copy = compact(atlas_copy(ui));
      require(copy.find("Permanentbonusesforyournextflight.") != std::string::npos,
          "ordinary Hangar visibly states when permanent power applies");
      for (const auto& track : store.hangar()) {
        require(copy.find(compact(track.owned_bonus + (track.next_bonus.empty() ? "" : " -> " + track.next_bonus))) != std::string::npos,
            "ordinary cards show actual owned to next totals, including maximum owned power");
        require(copy.find(compact(track.bonus_basis)) != std::string::npos,
            "ordinary cards retain complete original damage, starting/max pools and base capture basis");
      }
      require(ui.buttons.size() == 7, "four comparisons, Next Flight and stable Back/Blueprints remain together");
      for (unsigned i = 0; i < 4; ++i)
        require(ui.buttons[i].enabled == (rank < 5), "capped cards retain owned power but no purchase action");
      const auto safe = hr::ui::inset(view.viewport, view.safe_area);
      const float scale = std::min(safe.width / 390.F, safe.height / 844.F);
      const float ox = safe.x + (safe.width - 390.F * scale) * .5F;
      const float oy = safe.y + (safe.height - 844.F * scale) * .5F;
      bool price_label = false;
      for (const auto& q : ui.quads) {
        if (!glyph(q)) {
          price_label = false;
          for (unsigned i = 0; i < 4; ++i)
            if (std::abs(q.rect[0] * phone.width - ui.buttons[i].bounds.x) < .01F &&
                std::abs(q.rect[2] * phone.width - ui.buttons[i].bounds.width) < .01F) price_label = true;
          continue;
        }
        const auto ink = actual_glyph_ink(q, view.viewport);
        if (ink.width == 0) continue;
        require(ink.x >= ox + 8 * scale && ink.right() <= ox + 382 * scale,
            "actual Hangar caption and heading ink stays inside its visible panel on small phones");
        // Purchase label ink belongs inside its button; comparison/caption ink
        // must not cross that price column or the following track's panel.
        if (price_label) continue;
        for (unsigned i = 0; i < 4; ++i) {
          const float top = oy + (162 + static_cast<float>(i) * 138) * scale;
          if (ink.y >= top + 56 * scale && ink.y < top + 126 * scale) {
            if (!(ink.right() < ui.buttons[i].bounds.x && ink.bottom() <= top + 126 * scale))
              std::cerr << "Hangar ink width=" << phone.width << " rank=" << rank << " track=" << i <<
                  " ink=" << ink.x << ',' << ink.y << ',' << ink.width << ',' << ink.height <<
                  " price-x=" << ui.buttons[i].bounds.x << " card-bottom=" << top + 126 * scale << '\n';
            require(ink.right() < ui.buttons[i].bounds.x && ink.bottom() <= top + 126 * scale,
                "consumed comparison and full stat-basis ink clears price targets and stays inside its card");
          }
        }
      }
    }
  }
  View enlarged{Screen::hangar, &game.snapshot(), &store, {0,0,390,844}, {0,47,0,34}};
  enlarged.text_scale = 2;
  const auto first_enlarged = interface(enlarged);
  require(first_enlarged.page_text.find("Weapon") != std::string::npos && first_enlarged.page_text.find("Calibration") != std::string::npos,
      "shared flight footer leaves the full first track name readable on page one");
  const auto back = std::find_if(first_enlarged.buttons.begin(), first_enlarged.buttons.end(), [](const auto& b){return b.command == Command::home;});
  require(back != first_enlarged.buttons.end() && back->label == "BACK" && back->bounds.width >= 44, "compact visible Back arrow keeps its complete semantic name and target");
  std::string all;
  const auto count = interface(enlarged).page_count;
  for (unsigned page = 0; page < count; ++page) {
    enlarged.ui_page = page;
    all += interface(enlarged).page_text;
  }
  for (const auto& track : store.hangar())
    require(compact(all).find(compact("Owned: " + track.owned_bonus + " " + track.bonus_basis)) != std::string::npos,
        "enlarged fallback retains complete maximum owned power and basis");
}

void hangar_next_flight_is_reachable_without_hiding_purchases() {
  progression_tests::MemoryBackend backend;
  hr::progression::AccountStore store{backend}; require(store.load().playable(), "Hangar fixture loads");
  hr::gameplay::Game game; game.start();
  for (const auto viewport : {hr::ui::Rect{0,0,320,568}, hr::ui::Rect{0,0,390,844}, hr::ui::Rect{0,0,844,390}, hr::ui::Rect{0,0,320,320}}) {
    for (const float scale : {1.F, 2.F}) {
      View view{Screen::hangar, &game.snapshot(), &store, viewport, {}}; view.text_scale = scale;
      std::set<Command> reachable;
      const auto pages = interface(view).page_count;
      for (unsigned page = 0; page < pages; ++page) {
        view.ui_page = page; const auto ui = interface(view);
        for (std::size_t i = 0; i < ui.buttons.size(); ++i) {
          const auto& b = ui.buttons[i]; reachable.insert(b.command);
          require(b.bounds.width >= 44 && b.bounds.height >= 44, "every Hangar control remains touch sized");
          require(b.bounds.x >= 0 && b.bounds.y >= 0 && b.bounds.right() <= viewport.width + .01F && b.bounds.bottom() <= viewport.height + .01F, "Hangar controls stay within the viewport");
          for (std::size_t j = i + 1; j < ui.buttons.size(); ++j) {
            const auto overlap = hr::ui::intersect(b.bounds, ui.buttons[j].bounds);
            require(overlap.width < .01F || overlap.height < .01F, "Hangar direct-flight and existing targets never overlap");
          }
        }
      }
      for (const auto command : {Command::start, Command::home, Command::blueprints, Command::buy0, Command::buy1, Command::buy2, Command::buy3})
        require(reachable.contains(command), "complete Hangar navigation and every purchase remain reachable");
    }
  }
}
void held_button_feedback_preserves_targets_glyphs_and_geometry() {
  progression_tests::MemoryBackend backend;
  hr::progression::AccountStore store{backend}; require(store.load().playable(), "button fixture loads");
  hr::gameplay::Game game; game.start();
  for (const auto screen : {Screen::title, Screen::hangar, Screen::run, Screen::settings}) {
    for (const float scale : {1.F, 2.F}) {
      View view{screen, &game.snapshot(), &store, {0,0,390,844}, {}}; view.text_scale = scale;
      const auto idle = interface(view);
      for (const auto& target : idle.buttons) {
        view.held_command = target.command;
        const auto held = interface(view);
        require(held.buttons.size() == idle.buttons.size(), "pressure creates no new control");
        for (std::size_t i = 0; i < held.buttons.size(); ++i)
          require(held.buttons[i].bounds.x == idle.buttons[i].bounds.x && held.buttons[i].bounds.y == idle.buttons[i].bounds.y && held.buttons[i].bounds.width == idle.buttons[i].bounds.width && held.buttons[i].bounds.height == idle.buttons[i].bounds.height && held.buttons[i].command == idle.buttons[i].command && held.buttons[i].enabled == idle.buttons[i].enabled, "pressure never moves targets or changes activation");
        require(held.quads.size() == idle.quads.size(), "pressure adds no geometry or new draw commands");
        unsigned changed = 0;
        for (std::size_t i = 0; i < idle.quads.size(); ++i) {
          const auto& q = held.quads[i]; const auto& original = idle.quads[i];
          require(q.rect == original.rect && q.uv == original.uv, "pressure preserves original rounded surfaces and layout geometry");
          if (glyph(q) || q.uv[2] >= 0) require(q.color == original.color, "pressure preserves glyph and icon colors");
          if (q.color != original.color) {
            ++changed;
            const hr::ui::Rect r{q.rect[0]*view.viewport.width, q.rect[1]*view.viewport.height, q.rect[2]*view.viewport.width, q.rect[3]*view.viewport.height};
            require(r.x >= target.bounds.x - .01F && r.y >= target.bounds.y - .01F && r.right() <= target.bounds.right() + .01F && r.bottom() <= target.bounds.bottom() + .01F, "only the explicitly owned button surface responds to pressure");
          }
        }
        require((changed > 0) == target.enabled, "enabled pressure changes its surface and disabled pressure does nothing");
      }
      view.held_command = Command::none;
      require(interface(view).quads.size() == idle.quads.size(), "released or cancelled pressure disappears completely");
      view.held_command = Command::back_up_bank;
      require(interface(view).quads.size() == idle.quads.size(), "a command absent from this page cannot light a control");
    }
  }
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
          if (view.screen == Screen::hangar) {
            for (unsigned i = 0; i < 4; ++i)
              require(reachable.contains(static_cast<Command>(static_cast<unsigned>(Command::buy0) + i)), "all permanent purchases remain reachable");
            require(reachable.contains(Command::start) && reachable.contains(Command::blueprints) && reachable.contains(Command::home), "readable Hangar preserves direct flight, library and return actions");
          }
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
  progression_tests::MemoryBackend backend;hr::progression::AccountStore store{backend};
  require(store.load().playable(), "HUD account loads");
  hr::gameplay::Game game;game.start();auto run=game.snapshot();
  run.hull=run.maximum_hull=run.shield=run.maximum_shield=9999;
  run.stats.level=std::numeric_limits<std::uint32_t>::max();run.stats.active_seconds=30;
  for(const auto phone:std::array<hr::ui::Rect,2>{{{0,0,320,568},{0,0,390,844}}})
    for(const auto score:{std::uint64_t{0},std::uint64_t{9999},std::uint64_t{999999},std::numeric_limits<std::uint64_t>::max()}) {
      run.stats.score=score;View view{Screen::run,&run,&store,phone,{0,20,0,0}};
      const auto ui=interface(view);
      const auto pause=std::find_if(ui.buttons.begin(),ui.buttons.end(),[](const auto& b){return b.command==Command::pause;});
      require(pause!=ui.buttons.end(),"maximum HUD retains Pause");
      std::vector<hr::ui::Rect> pods;
      for(const auto& q:ui.quads) if(q.uv[2]<0 && q.color==std::array<float,4>{.035F,.105F,.17F,.96F} && q.rect[1]*phone.height<pause->bounds.bottom()) {
        const hr::ui::Rect box{q.rect[0]*phone.width,q.rect[1]*phone.height,q.rect[2]*phone.width,q.rect[3]*phone.height};
        require(box.bottom()<=pause->bounds.bottom()+.01F,"grouped HUD keeps original hostile ingress boundary");pods.push_back(box);
      }
      require(pods.size()==2 && pods[0].right()+7.99F<=pods[1].x,"survival and progress have separate stable groups");
      std::string copy;std::vector<hr::ui::Rect> inks;
      for(const auto& q:ui.quads) if(glyph(q)) {
        const auto ink=actual_glyph_ink(q,phone);if(ink.width==0 || ink.y>pause->bounds.bottom() || ink.x>=pause->bounds.x)continue;
        const bool inside=std::any_of(pods.begin(),pods.end(),[&](const auto& box){return ink.x>=box.x+1.99F && ink.right()<=box.right()-1.99F && ink.y>=box.y+1.99F && ink.bottom()<=box.bottom()-1.99F;});
        require(inside,"actual body/title glyph alpha plus filtering stays inside the correct tactical pod with physical padding");
        const unsigned col=static_cast<unsigned>(std::lround(q.uv[0]*16));const float bank=q.uv[1]*2048>=512?512.F:0.F;
        const unsigned row=static_cast<unsigned>(std::lround((q.uv[1]*2048-bank)/80));copy+=static_cast<char>(32+row*16+col);inks.push_back(ink);
      }
      for(const auto value:{"HP9999","SHIELD9999","LV4B","XP","SCORE"}) require(copy.find(value)!=std::string::npos,"grouped HUD retains absolute defences and distinct level/XP/score values");
      const auto gap_after=[&](std::string_view label){const auto start=copy.find(label);const auto last=start+label.size()-1;require(last+1<inks.size(),"numeric anchor follows label");require(inks[last].right()+1.99F<=inks[last+1].x,"label and heavy numeric anchor have visible actual-ink separation");};
      gap_after("HP");gap_after("SHIELD");gap_after("LV");gap_after("SCORE");
      if(score==9999) require(copy.find("SCORE9999")!=std::string::npos,"four-digit score remains fully visible");
      require(!ui.text_overflow,"ordinary grouped HUD honestly fits maximum supported visual values");
      if(phone.width==320)require(std::abs(pause->bounds.bottom()-64.0711F)<.02F,"grouping preserves exact existing Pause/source-fit boundary");
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

void run_onboarding_names_the_actual_input_without_moving_controls() {
  progression_tests::MemoryBackend backend;
  hr::progression::AccountStore store{backend};
  require(store.load().playable(), "onboarding account loads");
  hr::gameplay::Game game;
  game.start();
  auto run = game.snapshot();
  for (const auto phone : std::array<hr::ui::Rect, 2>{{{0, 0, 320, 568}, {0, 0, 390, 844}}}) {
    View view{Screen::run, &run, &store, phone, {0, 20, 0, 0}};
    const auto touch = interface(view);
    const auto touch_copy = compact(atlas_copy(touch));
    require(touch_copy.find("DRAGTOFLY") != std::string::npos && touch_copy.find("TAPDASHTODODGE") != std::string::npos, "initial mobile hints explain drag and the actual Dash target");
    require(touch_copy.find("ARROWS") == std::string::npos && touch_copy.find("SPACE") == std::string::npos, "default hints do not advertise a keyboard");
    for (const auto device : {hr::input::DeviceKind::pointer, hr::input::DeviceKind::keyboard, hr::input::DeviceKind::gamepad}) {
      view.hint_device = device;
      const auto ui = interface(view);
      const auto copy = compact(atlas_copy(ui));
      if (device == hr::input::DeviceKind::keyboard)
        require(copy.find("WASD/ARROWSTOFLY") != std::string::npos && copy.find("SPACETODODGE") != std::string::npos, "keyboard hints name the actual movement and dash bindings");
      else if (device == hr::input::DeviceKind::gamepad)
        require(copy.find("LEFTSTICKTOFLY") != std::string::npos && copy.find("RIGHTFACEBUTTONTODODGE") != std::string::npos, "controller hints use the bound stick and physical east-face button without a family-specific label");
      else
        require(copy == touch_copy, "pointer drag uses the same direct manipulation hints");
      require(ui.buttons.size() == touch.buttons.size(), "onboarding modality preserves controls");
      for (std::size_t i = 0; i < ui.buttons.size(); ++i) {
        const auto& a = ui.buttons[i]; const auto& b = touch.buttons[i];
        require(a.command == b.command && a.enabled == b.enabled && a.bounds.x == b.bounds.x && a.bounds.y == b.bounds.y && a.bounds.width == b.bounds.width && a.bounds.height == b.bounds.height, "onboarding copy never moves or changes a hit target");
      }
    }
    run.stats.active_seconds = 4;
    const auto later_touch = interface(view);
    const auto coaching = compact(atlas_copy(later_touch));
    require((coaching.find("KEEPMOVINGTHROUGHTARGETS") != std::string::npos || coaching.find("MOVETHROUGHTARGETS") != std::string::npos) && coaching.find("3SWEEPS=OVERDRIVE") != std::string::npos, "sweep coaching teaches movement through targets and the three-sweep goal");
    view.hint_device = hr::input::DeviceKind::keyboard;
    require(compact(atlas_copy(interface(view))) == compact(atlas_copy(later_touch)), "capture and Overdrive coaching retains the established four-second transition independent of input");
    run.stats.active_seconds = 12;
    require(compact(atlas_copy(interface(view))).find("KEEPMOVINGTHROUGHTARGETS") == std::string::npos, "coaching retains its established twelve-second cutoff");
    run.stats.active_seconds = 0;
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
    require(copy.find("KEY1")==std::string::npos && copy.find("KEY2")==std::string::npos && copy.find("KEY3")==std::string::npos,"touch-first offers do not advertise unavailable keyboard input");
    auto keyboard = view; keyboard.hint_device = hr::input::DeviceKind::keyboard;
    const auto keyboard_ui = interface(keyboard);
    const auto keyboard_copy = compact(atlas_copy(keyboard_ui));
    require(keyboard_copy.find("KEY1")!=std::string::npos && keyboard_copy.find("KEY2")!=std::string::npos && keyboard_copy.find("KEY3")!=std::string::npos,"actual keyboard mode names the three available shortcuts");
    require(keyboard_ui.buttons.size()==ui.buttons.size(),"input hints do not change available offers");
    for(std::size_t i=0;i<ui.buttons.size();++i) {
      const auto& a=ui.buttons[i]; const auto& b=keyboard_ui.buttons[i];
      require(a.command==b.command && a.enabled==b.enabled && a.bounds.x==b.bounds.x && a.bounds.y==b.bounds.y && a.bounds.width==b.bounds.width && a.bounds.height==b.bounds.height,"switching input hints keeps every choice target and command stable");
    }
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
void earned_offers_show_one_projected_benefit_and_all_conditions() {
  progression_tests::MemoryBackend backend;
  hr::progression::AccountStore store{backend};
  require(store.load().playable(), "reward-choice account loads");
  hr::gameplay::Game game; game.start({.seed=514});
  for(unsigned step=0;step<1800 && game.snapshot().phase==hr::gameplay::Phase::active;++step) game.advance(1.F/60);
  require(game.snapshot().phase==hr::gameplay::Phase::choosing_upgrade,"reward layout consumes a naturally earned draft");
  const auto& run=game.snapshot();
  // Ordinary system text still needs the measured selected-offer layout when a
  // short window cannot hold all three cards above its footer.
  for(unsigned preview=0;preview<3;++preview) {
    View short_view{Screen::upgrade,&run,&store,{0,0,320,320},{0,20,0,0}};
    short_view.upgrade_preview=preview;
    const auto first=interface(short_view);
    require(!first.page_text.empty(),"short ordinary-text draft uses the readable offer layout");
    std::string all;
    for(unsigned page=0;page<first.page_count;++page) {
      short_view.ui_page=page;
      const auto ui=interface(short_view);readable_page_is_real_safe_and_scaled(short_view,ui);all+=ui.page_text;
      if(page==0)for(unsigned i=0;i<3;++i)require(std::any_of(ui.buttons.begin(),ui.buttons.end(),[&](const auto& b){return b.command==static_cast<Command>(static_cast<unsigned>(Command::preview0)+i) && b.enabled;}),"short ordinary text keeps direct access to every offer preview");
    }
    require(compact(all).find(compact(run.choices[preview].conditions))!=std::string::npos,"short ordinary-text pagination preserves the complete selected mechanics");
  }
  for(const auto phone:std::array<hr::ui::Rect,2>{{{0,0,320,568},{0,0,390,844}}}) {
    View view{Screen::upgrade,&run,&store,phone,{0,20,0,0}};
    const auto board=interface(view);
    const auto copy=compact(atlas_copy(board));
    for(unsigned i=0;i<3;++i) {
      const auto& pick=run.choices[i];
      const auto label=compact(pick.effect_label);
      const auto start=copy.find(label);
      require(start!=std::string::npos && copy.find(label,start+label.size())==std::string::npos,"each projected metric is visible exactly once on the ordinary board");
      require(copy.find(compact(pick.conditions))!=std::string::npos,"secondary mechanics remain visible once beside the benefit");
      require(copy.find(compact(pick.description))==std::string::npos,"structured offers no longer repeat the full numeric description");
      const auto command=static_cast<Command>(static_cast<unsigned>(Command::pick0)+i);
      require(std::any_of(board.buttons.begin(),board.buttons.end(),[&](const auto& button){return button.command==command && button.enabled;}),"all three earned offers retain immediate ordinary-size selection");
      const auto nodes=accessibility_snapshot(view,board);
      require(std::any_of(nodes.begin(),nodes.end(),[&](const auto& node){return node.label=="Choose "+pick.title && node.value.find(pick.description)!=std::string::npos;}),"assistive choice retains the exact complete original mechanics");
    }
    for(float scale:{2.F,3.2F})for(unsigned preview=0;preview<3;++preview) {
      auto readable=view;readable.text_scale=scale;readable.upgrade_preview=preview;
      std::string all;
      const auto first=interface(readable);
      for(unsigned page=0;page<first.page_count;++page) {
        readable.ui_page=page;
        const auto ui=interface(readable);readable_page_is_real_safe_and_scaled(readable,ui);
        all+=ui.page_text;
      }
      const auto label=compact(run.choices[preview].effect_label);const auto text=compact(all);const auto start=text.find(label);
      require(start!=std::string::npos && text.find(label,start+label.size())==std::string::npos,"enlarged offer keeps one labelled benefit across pages");
      require(text.find(compact(run.choices[preview].conditions))!=std::string::npos,"all actual conditions survive enlarged pagination");
    }
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
        if (scale == 1) {
          const auto safe=hr::ui::inset(view.viewport,view.safe_area);const float fit=std::min(safe.width/390.F,safe.height/844.F);
          const float ox=safe.x+(safe.width-390*fit)*.5F,oy=safe.y+(safe.height-844*fit)*.5F;
          const hr::ui::Rect status{ox+20*fit,oy+752*fit,190*fit,60*fit};
          require(std::any_of(ui.quads.begin(),ui.quads.end(),[&](const auto& q){return q.uv[2]<0 && q.color==std::array<float,4>{.035F,.105F,.17F,.96F} && std::abs(q.rect[0]*phone.width-status.x)<.01F && std::abs(q.rect[1]*phone.height-status.y)<.01F;}),"passive sweep/Overdrive has a navy status backing distinct from the mint Dash action");
          require(ui.buttons.size()==2,"passive sweep/Overdrive does not become an input target");
          unsigned pips=0;
          for(const auto& q:ui.quads) {
            if(glyph(q)) {const auto ink=actual_glyph_ink(q,phone);if(ink.width>0 && ink.y>status.y && ink.x<status.right() && ink.x>status.x) require(ink.right()<=status.right()-2 && ink.bottom()<=status.bottom()-2,"complete passive status title uses consumed atlas ink inside its existing footprint");}
            if(q.uv[2]<0 && q.color==std::array<float,4>{.06F,.16F,.23F,.97F} && std::abs(q.rect[1]*phone.height-(oy+789*fit))<.01F)++pips;
          }
          require(pips==hr::gameplay::tuning::harvest_chain_target,"counted empty/filled sweep pips retain non-color chain meaning");
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
    const std::string expected=short_stage?std::string(hull?"HP":"SH")+(framing.compact?"":":")+(cause==hr::gameplay::DamageCause::contact?"BUMP":cause==hr::gameplay::DamageCause::projectile?"SHOT":"HIT"):std::string(hull?"HP ":"SHIELD ")+(cause==hr::gameplay::DamageCause::contact?"BUMP":cause==hr::gameplay::DamageCause::projectile?"SHOT":"HIT");
    require(copy.find(compact(expected))!=std::string::npos,"committed hit identifies agreed full or compact actual cause and affected defence at every text size");
    if(!short_stage)for(const auto value:{"HP100","SHIELD20","LV1","XP","SCORE0"}) require(copy.find(value)!=std::string::npos,"ordinary damage keeps every persistent survival/progress value visible");
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
    for (const auto value : {"HP112","SHIELD80","LV24","SCORE2126","XP"})
      require(copy.find(value) != std::string::npos, "late normal HUD retains every actual value and experience label");
    for (const auto& q : ui.quads) if (glyph(q)) {
      const float size = q.rect[2] * viewport.width * 48.F / 64.F;
      require(size >= 13.99F, "late HUD does not shrink below readable floor");
    }
  }
}
void report_header_contains_complete_consumed_glyph_ink() {
  progression_tests::MemoryBackend backend;
  hr::progression::AccountStore store{backend};
  require(store.load().playable() && store.begin_run().succeeded(), "report ink fixture account begins");
  hr::progression::RunReceipt receipt{.run_id=store.account().checkpoint.run_id,.active_milliseconds=75000,.score=85,.captures=23};
  require(store.settle(receipt).succeeded(), "report ink fixture has a saved receipt");
  hr::gameplay::Game game; game.start(); auto run=game.snapshot();run.phase=hr::gameplay::Phase::dead;
  run.stats.level=124;for(unsigned i=0;i<6;++i)run.modules[i]=3;
  for(const auto phone:std::array<hr::ui::Rect,2>{{{0,0,320,568},{0,0,390,844}}})
    for(const auto theme:{hr::presentation::SceneryTheme::farmland,hr::presentation::SceneryTheme::suburbia,hr::presentation::SceneryTheme::desert})
      for(unsigned scenario=0;scenario<4;++scenario) {
        View view{Screen::results,&run,&store,phone,{0,20,0,0}};view.theme=theme;
        if(scenario==1)view.guest=true;
        // A genuine interrupted-checkpoint recovery supplies the longer status.
        progression_tests::MemoryBackend recovered_backend;
        hr::progression::AccountStore interrupted{recovered_backend};
        hr::progression::AccountStore recovered{recovered_backend};
        progression_tests::MemoryBackend short_backend;
        hr::progression::AccountStore short_store{short_backend};
        if(scenario==2) {
          require(short_store.load().playable() && short_store.begin_run().succeeded(), "short report fixture begins");
          auto short_receipt=receipt;short_receipt.run_id=short_store.account().checkpoint.run_id;short_receipt.active_milliseconds=10000;
          require(short_store.settle(short_receipt).succeeded(), "actual earlyzero settlement");
          view.store=&short_store;
        }
        if(scenario==3) {
          require(interrupted.load().playable() && interrupted.begin_run().succeeded(), "report ink recovery begins");
          auto pending=receipt;pending.run_id=interrupted.account().checkpoint.run_id;pending.active_milliseconds=75000;
          require(interrupted.checkpoint(pending,true).succeeded(), "report ink recovery checkpoint persists");
          require(recovered.load().issue==hr::progression::LoadIssue::recovered_checkpoint,"actual recovery report fixture");
          view.store=&recovered;
        }
        const auto report=flight_report(view);const auto ui=interface(view);
        if(!ui.page_text.empty()) {
          std::string copy;
          for(unsigned page=0;page<ui.page_count;++page){view.ui_page=page;const auto readable=interface(view);readable_page_is_real_safe_and_scaled(view,readable);copy+=readable.page_text;}
          require(compact(copy).find(compact(report.status))!=std::string::npos,"measured fallback preserves complete longer report status");
          continue;
        }
        // The standard interface's translucent scene dimmer precedes the report
        // panel. Own the navy header itself, never that full-viewport overlay.
        const auto first=std::find_if(ui.quads.begin(),ui.quads.end(),[](const auto& q){return q.uv[2]<0 && q.color[0]==.035F && q.color[1]==.105F && q.color[2]==.17F;});
        require(first!=ui.quads.end(),"ordinary report has its own navy header");
        const auto header_index=static_cast<std::size_t>(first-ui.quads.begin());
        const hr::ui::Rect header{phone.x+first->rect[0]*phone.width,phone.y+first->rect[1]*phone.height,first->rect[2]*phone.width,first->rect[3]*phone.height};
        std::string copy;bool left_header=false;float ink_bottom=header.y;
        for(std::size_t i=header_index+1;i<ui.quads.size();++i) {
          const auto& q=ui.quads[i];
          if(!glyph(q)){left_header=true;break;}
          const auto ink=actual_glyph_ink(q,phone);if(ink.width==0)continue;
          require(ink.x>=header.x+3.99F && ink.right()<=header.right()-3.99F && ink.y>=header.y+3.99F && ink.bottom()<=header.bottom()-3.99F,"actual consumed report header alpha and filtering fringe stay inside navy with physical padding");
          ink_bottom=std::max(ink_bottom,ink.bottom());
        }
        require(left_header && ink_bottom>header.y,"report header has real consumed glyph ink before reward content");
        for(std::size_t i=header_index+1;i<ui.quads.size() && glyph(ui.quads[i]);++i) {
          const auto& q=ui.quads[i];const unsigned col=static_cast<unsigned>(std::lround(q.uv[0]*16));const float bank=q.uv[1]*2048>=512?512.F:0.F;const unsigned row=static_cast<unsigned>(std::lround((q.uv[1]*2048-bank)/80));copy+=static_cast<char>(32+row*16+col);
        }
        require(compact(copy)==compact(report.heading+report.status),"all status words are drawn before reward panel, without max-lines truncation");
        require(!ui.text_overflow,"bounded ordinary report header allocation does not falsely declare a fit");
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
  hangar_shows_owned_and_next_power_with_visible_basis();
  hangar_next_flight_is_reachable_without_hiding_purchases();
  held_button_feedback_preserves_targets_glyphs_and_geometry();
  enlarged_run_preserves_two_bands_and_complete_semantics();
  banded_framing_keeps_pixel_fit_and_global_contacts();
  late_value_hud_keeps_level_at_small_portrait();
  report_header_contains_complete_consumed_glyph_ink();
  compact_choice_titles_stay_on_the_dark_frame();
  promotion_preserves_choice_safety_and_collected_return_controls();
  actual_damage_feedback_stays_inside_the_tactical_hud();
  harvest_feedback_explains_sweeps_boost_and_dash_without_expanding_top_hud();
  pause_bank_disclosure_keeps_existing_actions_and_complete_copy();
  compact_phone_cards_preserve_effects_and_scene_stage();
  run_onboarding_names_the_actual_input_without_moving_controls();
  upgrade_board_exposes_actual_build_and_projected_benefits();
  earned_offers_show_one_projected_benefit_and_all_conditions();
  grouped_build_keeps_full_loadout_research_and_hangar_in_three_pages();
  larger_text_preserves_whole_offers_and_all_menu_actions();
  current_build_separates_temporary_and_permanent_identity();
  small_preferences_preserve_type_growth_and_useful_menus();
  compact_hud_maximum_values_preserve_spacing_and_ingress_bound();
  std::cout << "Portrait cards preserve full effect copy, scene exposure and safe touch targets\n";
}
