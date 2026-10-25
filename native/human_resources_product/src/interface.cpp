#include <algorithm>
#include <array>
#include <cmath>
#include <human_resources/gameplay/tuning.h>
#include <human_resources/product/presentation.h>
#include <human_resources/product/combat_feedback.h>
#include <product_glyphs.generated.h>
#include <sstream>
#include "privacy_policy.h"
#include "upgrade_presentation.h"
#include "flight_report.h"
namespace human_resources::product {
namespace {
using Color = std::array<float, 4>;
constexpr Color navy{.035F, .105F, .17F, .96F}, panel{.06F, .16F, .23F, .97F}, mint{.39F, .96F, .75F, 1}, white{.96F, .97F, .89F, 1}, muted{.67F, .78F, .76F, 1}, gold{1, .79F, .32F, 1}, coral{1,.43F,.36F,1};
std::string number(std::uint64_t n) {
  return std::to_string(n);
}
std::string time_text(double seconds) {
  const auto n = static_cast<unsigned>(seconds);
  return number(n / 60) + ":" + (n % 60 < 10 ? "0" : "") + number(n % 60);
}
struct Canvas final {
  Interface output;
  float w, h, scale, ox, oy, vx, vy;
  float type_scale{1};
  ui::Rect safe;
  std::vector<ui::Rect> text_lines;
  explicit Canvas(const View& v, bool wide = false) : w(v.viewport.width), h(v.viewport.height), vx(v.viewport.x), vy(v.viewport.y) {
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
  void sprite(float x, float y, float width, float height, float sx, float sy, float sw, float sh) {
    const auto r = rect(x, y, width, height);
    output.quads.push_back({{(r.x - vx) / w, (r.y - vy) / h, r.width / w, r.height / h}, {sx / 1024.F, sy / 2048.F, sw / 1024.F, sh / 2048.F}, {1, 1, 1, 1}});
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
    fill(x, y, width, height, enabled ? color : Color{.15F, .23F, .28F, 1});
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
} // namespace
void save_modal(Canvas& c, const View& v, bool wide) {
  if (!v.save_error)
    return;
  c.output.buttons.clear();
  c.fill(-1000, -1000, 3000, 3000, {.015F, .045F, .09F, .72F});
  if (wide) {
    c.fill(80, 30, 684, 330, navy);
    c.text(104, 48, "PROGRESS PROTECTED", 28, white, true);
    c.wrap(104, 102, 636, v.save_message.empty() ? "Saving failed. Retry before continuing." : v.save_message, 18, white, 5);
    c.button(104, 268, v.can_reset_save || v.can_bank_checkpoint ? 300.F : 636.F, "RETRY SAVE", Command::retry_save, true, gold, 64);
    if (v.can_reset_save)
      c.button(440, 268, 300, "BACK UP & RESET", Command::back_up_reset, true, white, 64);
    else if (v.can_bank_checkpoint)
      c.button(440, 268, 300, "BACK UP & BANK", Command::back_up_bank, true, white, 64);
  } else {
    c.fill(8, 236, 374, 386, navy);
    c.wrap(26, 254, 338, "PROGRESS PROTECTED", 28, white, 2);
    c.wrap(26, 332, 338, v.save_message.empty() ? "Saving failed. Retry before continuing." : v.save_message, 18, white, 7);
    c.button(26, 492, 338, "RETRY SAVE", Command::retry_save, true, gold, 56);
    if (v.can_reset_save)
      c.button(26, 566, 338, "BACK UP & RESET", Command::back_up_reset, true, white, 44);
    else if (v.can_bank_checkpoint)
      c.button(26, 566, 338, "BACK UP & BANK", Command::back_up_bank, true, white, 44);
  }
}
Interface wide_interface(const View& v) {
  Canvas c(v, true);
  const auto& run = *v.run;
  const auto& account = v.store->account();
  const auto title = [&](std::string_view heading, std::string_view sub) {c.fill(12,12,820,366,navy);c.text(32,28,heading,30,white,true);c.wrap(32,78,780,sub,17,muted,2); };
  const auto back = [&]() { c.button(658, 298, 154, "BACK", Command::home, true, white, 64); };
  switch (v.screen) {
  case Screen::run:
    c.hud(12, 10, 280, 56);
    c.text(28, 20, v.damage_feedback.age >= 0 && !v.damage_feedback.hull ? std::string(damage_label(v.damage_feedback)) + " " + number(static_cast<unsigned>(run.shield)) : std::string(v.damage_feedback.age >= 0 ? damage_label(v.damage_feedback) : "HULL") + " " + number(static_cast<unsigned>(run.hull)) + " / " + number(static_cast<unsigned>(run.maximum_hull)), 18, v.damage_feedback.age >= 0 ? coral : white);
    c.hud(304, 10, 218, 56);
    c.text(322, 20, "LV " + number(run.stats.level) + " / " + time_text(run.stats.active_seconds), 18);
    c.hud(534, 10, 218, 56);
    c.text(550, 20, "SCORE " + number(run.stats.score), 18);
    c.button(764, 10, 64, "II", Command::pause, true, white, 64);
    c.button(646, 298, 182, run.dash_cooldown_seconds > 0 ? "DASH " + number(static_cast<unsigned>(std::ceil(run.dash_cooldown_seconds))) + "s" : "DASH", Command::dash, run.dash_cooldown_seconds <= 0, mint, 64);
    break;
  case Screen::title:
    title("HUMAN RESOURCES", "Fly the saucer. Gather specimens. Outrun the resistance.");
    c.button(32, 278, 244, "START RUN", Command::start, true, mint, 64);
    c.button(300, 278, 244, "HANGAR", Command::hangar, true, white, 64);
    c.button(568, 278, 244, "SETTINGS", Command::settings, true, white, 64);
    c.button(32, 186, 310, "LOCATION: " + std::string(location_name(v.theme)) + " >", Command::location_next, true, white, 48);
    break;
  case Screen::upgrade:
    title("PROMOTION / LV " + number(run.stats.level), "Choose one upgrade. Your run is paused.");
    for (unsigned i = 0; i < 3; ++i) {
      const float x = 32 + static_cast<float>(i) * 264;
      const auto& pick = run.choices[i];
      c.fill(x, 116, 252, 156, panel);
      c.wrap(x + 12, 126, 228, pick.title, 19, white, 2);
      c.wrap(x + 12, 174, 228, pick.description, 14, muted, 4);
      c.button(x, 292, 252, "CHOOSE " + number(i + 1), static_cast<Command>(static_cast<unsigned>(Command::pick0) + i), true, mint, 64);
    }
    break;
  case Screen::pause:
    title(v.restored_run ? "RUN SAVED" : "ON HOLD", std::string(location_name(v.theme)) + (v.restored_run ? " / Level " + number(run.stats.level) + " / " + time_text(run.stats.active_seconds) + " saved. Resume or end and bank credits." : " / Your run clock is paused. Stay near specimens and avoid coral warnings."));
    c.button(32, 190, 244, "CURRENT BUILD", Command::current_build, true, white, 64);
    c.button(32, 278, 244, "RESUME", Command::resume, true, mint, 64);
    c.button(300, 278, 244, "SETTINGS", Command::settings, true, white, 64);
    c.button(568, 278, 244, "END & BANK", Command::finish, true, white, 64);
    break;
  case Screen::death:
  case Screen::results: {
    const auto report = flight_report(v);
    title(report.heading, report.status);
    c.text(32, 120, report.reward_label + "  +" + number(report.credits), 26, mint);
    c.text(32, 163, "SCORE " + number(report.score) + " / CAPTURES " + number(report.captures) + " / " + time_text(report.seconds), 19);
    if (!report.preview && !report.guest && !report.pending) {
      c.wrap(32, 208, 780, report.blueprint_heading + ": " + report.blueprint_name + " / " + report.blueprint_detail + ". " + report.hangar_name + ": " + report.hangar_detail, 17, muted, 2);
    }
    if (report.preview) c.button(32, 278, 780, "BANK CREDITS & RESULTS", Command::finish, true, mint, 64);
    else {
      c.button(32, 278, 378, "FLY AGAIN", Command::retry, true, mint, 64);
      c.button(434, 278, 378, report.hangar_action, Command::hangar, true, white, 64);
    }
    break;
  }
  case Screen::hangar: {
    title("HANGAR", number(account.credits) + " Research Credits / next-flight bonuses");
    const auto tracks = v.store->hangar();
    for (unsigned i = 0; i < 4; ++i) {
      const auto& track = tracks[i];
      const float x = 32 + static_cast<float>(i) * 196;
      c.fill(x, 120, 184, 146, panel);
      c.wrap(x + 10, 130, 164, track.title, 18, white, 2);
      c.text(x + 10, 174, track.next_bonus.empty() ? "OWNED / MAX" : "OWNED / NEXT", 14, mint);
      c.fitted(x + 10, 196, 164, track.owned_bonus + (track.next_bonus.empty() ? "" : " -> " + track.next_bonus), 22);
      c.wrap(x + 10, 226, 164, track.bonus_basis, 14, muted, 2);
      c.button(x, 282, 184, track.rank >= track.maximum_rank ? "MAXED" : number(track.cost) + " CR", static_cast<Command>(static_cast<unsigned>(Command::buy0) + i), track.affordable, mint, 64);
    }
    c.button(688, 28, 124, "BACK", Command::home, true, white, 64);
    c.button(514, 28, 160, "BLUEPRINTS", Command::blueprints, true, mint, 64);
    break;
  }
  case Screen::blueprints: {
    if (v.show_recipes) {
      title("EARNED EVOLUTIONS", "Both ingredients require rank III. Your first two completed recipes evolve automatically.");
      for (unsigned slot = 0; slot < 2; ++slot) {
        const unsigned i = (v.recipe_page % 3) * 2 + slot;
        const auto ingredients = gameplay::tuning::evolution_ingredients[i];
        const float x = 32 + static_cast<float>(slot) * 400;
        c.fill(x, 116, 380, 238, panel);
        c.wrap(x + 12, 126, 356, gameplay::tuning::evolution_names[i], 20, white, 1);
        c.wrap(x + 12, 162, 356, std::string(gameplay::tuning::module_names[ingredients[0]]) + " " + number(run.modules[ingredients[0]]) + "/3 + " + std::string(gameplay::tuning::module_names[ingredients[1]]) + " " + number(run.modules[ingredients[1]]) + "/3", 14, mint, 2);
        c.wrap(x + 12, 208, 356, recipe_effects[i], 14, muted, 6);
      }
      c.button(350, 28, 150, "NEXT " + number(v.recipe_page + 1) + "/3", Command::recipe_next, true, white, 64);
      c.button(514, 28, 160, "BLUEPRINTS", Command::recipes, true, mint, 64);
    } else {
      title("BLUEPRINT LIBRARY", "Free milestones unlock hardware in future upgrade draws.");
      const auto blueprints = v.store->blueprints();
      for (unsigned i = 0; i < 6; ++i) {
        const float x = 32 + static_cast<float>(i % 3) * 264, y = 120 + static_cast<float>(i / 3) * 118;
        const auto& blueprint = blueprints[i];
        c.fill(x, y, 252, 112, panel);
        c.wrap(x + 10, y + 7, 232, blueprint.title, 17, white, 1);
        c.wrap(x + 10, y + 35, 232, blueprint.milestone, 14, blueprint.unlocked ? mint : gold, 2);
        c.wrap(x + 10, y + 83, 232, "Run: " + blueprint.requirement, 14, muted, 1);
      }
      c.button(514, 28, 160, "EVOLUTIONS", Command::recipes, true, mint, 64);
    }
    c.button(688, 28, 124, "BACK", Command::home, true, white, 64);
    break;
  }
  case Screen::settings:
    title("FLIGHT SETTINGS", "Drag / WASD / arrows / controller left stick. Dash: Space or its control.");
    c.button(32, 154, 378, account.settings.audio ? "SOUND EFFECTS: ON" : "SOUND EFFECTS: OFF", Command::audio, true, white, 64);
    c.button(434, 154, 378, account.settings.reduced_motion ? "REDUCED MOTION: ON" : "REDUCED MOTION: OFF", Command::motion, true, white, 64);
    c.button(32, 236, 378, v.haptics_available ? (account.settings.haptics ? "HAPTICS: ON" : "HAPTICS: OFF") : "HAPTICS: UNAVAILABLE", Command::haptics, v.haptics_available, white, 64);
    c.button(434, 228, 378, "PRIVACY POLICY", Command::privacy, true, white, 56);
    back();
    break;
  }
  save_modal(c, v, true);
  return std::move(c.output);
}

namespace {
struct ReadableSection final {
  std::string text;
  Command command{Command::none};
  std::string label;
  bool enabled{true};
};
std::vector<ReadableSection> readable_sections(const View& v) {
  const auto& run = *v.run;
  const auto& account = v.store->account();
  std::vector<ReadableSection> sections;
  const auto add = [&](std::string text, Command command = Command::none, std::string label = {}, bool enabled = true) {
    sections.push_back({std::move(text), command, std::move(label), enabled});
  };
  if (v.save_error) {
    const std::string message = "PROGRESS PROTECTED\n" + std::string(v.save_message.empty() ? "Saving failed. Retry before continuing." : v.save_message);
    add(message, Command::retry_save, "RETRY SAVE");
    if (v.can_reset_save)
      add(message + "\nPreserve a verified backup before creating a fresh account.", Command::back_up_reset, "BACK UP & RESET");
    else if (v.can_bank_checkpoint)
      add(message + "\nPreserve a verified backup before banking the saved checkpoint.", Command::back_up_bank, "BACK UP & BANK");
    return sections;
  }
  if (v.screen == Screen::settings && v.show_privacy) {
    for (const auto section : privacy_policy_sections) add(std::string(section));
    return sections;
  }
  if (v.screen == Screen::pause && v.show_build) {
    unsigned installed = 0;
    for (const auto rank : run.modules) installed += rank != 0;
    add("CURRENT BUILD\n" + number(installed) + " / 6 hardware slots. Hardware, flight research and evolutions last for this run only. Permanent Hangar ranks apply to future runs.");
    for (unsigned i = 0; i < run.modules.size(); ++i)
      if (run.modules[i])
        add("FLIGHT HARDWARE\n" + std::string(gameplay::tuning::module_names[i]) + " / rank " + number(run.modules[i]) + " of 3.");
    bool research = false;
    for (unsigned i = 0; i < run.research.size(); ++i)
      if (run.research[i] > 0) {
        research = true;
        std::ostringstream amount;
        amount << run.research[i];
        const bool points = i == static_cast<unsigned>(gameplay::Research::hull_reinforcement) || i == static_cast<unsigned>(gameplay::Research::shield_capacity);
        add("TEMPORARY FLIGHT RESEARCH\n" + std::string(gameplay::tuning::research_names[i]) + ": +" + amount.str() + (points ? " points." : i == static_cast<unsigned>(gameplay::Research::emergency_repairs) ? "% immediate repair earned." : "% of base."));
      }
    if (!research) add("TEMPORARY FLIGHT RESEARCH\nNo research upgrades earned this run.");
    std::string evolutions = "ACTIVE EVOLUTIONS\n";
    for (unsigned i = 0; i < gameplay::tuning::evolution_names.size(); ++i)
      if (run.evolution_bits & (1U << i)) evolutions += std::string(gameplay::tuning::evolution_names[i]) + ". ";
    if (!run.evolution_bits) evolutions += "None yet. Both ingredients need rank III; your first two completed recipes evolve automatically.";
    add(evolutions);
    const auto tracks = v.store->hangar();
    std::string permanent = "PERMANENT HANGAR\n";
    for (const auto& track : tracks)
      permanent += std::string(track.title) + ": " + number(track.rank) + "/" + number(track.maximum_rank) + ". ";
    add(permanent);
    return sections;
  }
  switch (v.screen) {
  case Screen::title:
    add("HUMAN RESOURCES\nEvery abduction counts. Drag to fly. Capture and fire are automatic.", Command::start, "START RUN");
    add("LOCATION\n" + std::string(location_name(v.theme)) + ". All locations are available.", Command::location_next, "CHANGE LOCATION");
    add("HANGAR\nSpend saved Research Credits on permanent flight upgrades.", Command::hangar, "HANGAR");
    add("FLIGHT SETTINGS\nSound, haptics, reduced motion and flight controls.", Command::settings, "SETTINGS");
    break;
  case Screen::upgrade: {
    const unsigned i = std::min(v.upgrade_preview, 2U);
    const auto& pick = run.choices[i];
    add(pick.title + "\n" + std::string(choice_effect_label(pick)) + "\n" + choice_change(pick) + (choice_conditions(pick).empty() ? "" : "\n" + std::string(choice_conditions(pick))) + "\n" + choice_status(pick) + "\n" + number(hardware_count(run)) + " of 6 hardware slots occupied." + (pick.evolution_preview.empty() ? "" : "\nEvolves: " + pick.evolution_preview), static_cast<Command>(static_cast<unsigned>(Command::pick0) + i), "CHOOSE " + number(i + 1));
    if (run.rerolls_remaining) add("REROLL\nReplace all three offers. " + number(run.rerolls_remaining) + " remaining.", Command::reroll, "REROLL");
    add("EVOLUTIONS\nRead all six recipes and your ingredient ranks.", Command::recipe0, "EVOLUTIONS");
    break;
  }
  case Screen::pause:
    add(std::string(v.restored_run ? "RUN SAVED\n" : "ON HOLD\n") + std::string(location_name(v.theme)) + " / Level " + number(run.stats.level) + " / " + time_text(run.stats.active_seconds) + ". Score " + number(run.stats.score) + ". Hull " + number(static_cast<unsigned>(std::max(0.F, run.hull))) + ", shield " + number(static_cast<unsigned>(std::max(0.F, run.shield))) + ". The run clock is paused. Stay near specimens and avoid coral shots and warning circles." + (run.phase == gameplay::Phase::choosing_upgrade ? " Your earned upgrade is waiting." : ""), Command::resume, "RESUME");
    add("CURRENT BUILD\nInspect installed hardware, temporary flight research, evolutions and permanent Hangar ranks.", Command::current_build, "CURRENT BUILD");
    add("FLIGHT SETTINGS\nThe run stays paused while you change settings.", Command::settings, "SETTINGS");
    add("END RUN\nBank this flight's earned Research Credits exactly once. Flights shorter than 30 seconds earn no credits.", Command::finish, "END & BANK");
    break;
  case Screen::death:
  case Screen::results: {
    const auto report = flight_report(v);
    const auto body = report.heading + "\n" + report.summary.substr(report.heading.size() + 2);
    add(body, report.preview ? Command::finish : Command::retry, report.preview ? "BANK CREDITS & RESULTS" : "FLY AGAIN");
    if (!report.preview) add("HANGAR\nPermanent upgrades apply to future flights.", Command::hangar, report.hangar_action);
    break;
  }
  case Screen::hangar: {
    const auto tracks = v.store->hangar();
    for (unsigned i = 0; i < tracks.size(); ++i) {
      const auto& track = tracks[i];
      add("HANGAR / " + number(account.credits) + " RESEARCH CREDITS\nPermanent bonuses apply to your next flight.\n" + std::string(track.title) + " / rank " + number(track.rank) + " of " + number(track.maximum_rank) + "\nOwned: " + track.owned_bonus + " " + track.bonus_basis + "." + (track.next_bonus.empty() ? " Maximum rank." : "\nNext: " + track.next_bonus + " " + track.bonus_basis + "."), static_cast<Command>(static_cast<unsigned>(Command::buy0) + i), track.rank >= track.maximum_rank ? "MAXED" : "BUY / " + number(track.cost) + " CR", track.affordable);
    }
    add("BLUEPRINT LIBRARY\nFree milestones unlock hardware in future upgrade draws.", Command::blueprints, "BLUEPRINTS");
    break;
  }
  case Screen::blueprints:
    if (v.show_recipes) {
      for (unsigned i = 0; i < gameplay::tuning::evolution_names.size(); ++i) {
        const auto pair = gameplay::tuning::evolution_ingredients[i];
        add("EARNED EVOLUTIONS\n" + std::string(gameplay::tuning::evolution_names[i]) + "\n" + std::string(gameplay::tuning::module_names[pair[0]]) + " " + number(run.modules[pair[0]]) + "/3 + " + std::string(gameplay::tuning::module_names[pair[1]]) + " " + number(run.modules[pair[1]]) + "/3\n" + std::string(recipe_effects[i]) + "\nBoth ingredients need rank III. The first two completed recipes evolve automatically.");
      }
      add("BLUEPRINT LIBRARY\nRead free account milestones and this-run requirements.", Command::recipes, "BLUEPRINTS");
    } else {
      for (const auto& blueprint : v.store->blueprints())
        add("BLUEPRINT LIBRARY\n" + std::string(blueprint.title) + "\nAccount milestone: " + blueprint.milestone + "\nRequired in this run: " + blueprint.requirement + "\nUnlocked hardware joins future upgrade draws automatically.");
      add("EARNED EVOLUTIONS\nRead all six recipes and your ingredient ranks.", Command::recipes, "EVOLUTIONS");
    }
    break;
  case Screen::settings:
    add("FLIGHT SETTINGS\nDrag to fly / WASD or arrows / controller left stick. Capture and fire stay active while you move. Fly close for faster captures. Chain three quick sweeps for Overdrive. Dash through danger: press Space or tap Dash. Phase Drive improves it.");
    add("REDUCED MOTION\n" + std::string(account.settings.reduced_motion ? "On" : "Off") + ". System reduced motion is also respected.", Command::motion, account.settings.reduced_motion ? "MOTION: ON" : "MOTION: OFF");
    add("SOUND EFFECTS\n" + std::string(account.settings.audio ? "On" : "Off"), Command::audio, account.settings.audio ? "SOUND: ON" : "SOUND: OFF");
    add("HAPTICS\n" + std::string(v.haptics_available ? account.settings.haptics ? "On" : "Off" : "Unavailable on this device"), Command::haptics, v.haptics_available ? account.settings.haptics ? "HAPTICS: ON" : "HAPTICS: OFF" : "UNAVAILABLE", v.haptics_available);
    add("PRIVACY POLICY\nRead how this offline game handles local progress, settings and device backups.", Command::privacy, "PRIVACY POLICY");
    break;
  case Screen::run:
    break;
  }
  return sections;
}
// English atlas layout, measured at the requested system size. A word wider
// than the content column breaks between glyphs rather than losing its suffix.
std::vector<std::string> readable_lines(const Canvas& c, std::string_view value, float width, float size) {
  std::vector<std::string> lines;
  std::string line, word;
  const auto flush_word = [&]() {
    if (word.empty()) return;
    if (!line.empty() && c.measure(line + " " + word, size) > width) {
      lines.push_back(line);
      line.clear();
    }
    for (const char ch : word) {
      if (!line.empty() && c.measure(line + ch, size) > width) {
        lines.push_back(line);
        line.clear();
      }
      line += ch;
    }
    word.clear();
  };
  for (const char ch : value) {
    if (ch == ' ' || ch == '\n') {
      flush_word();
      if (ch == '\n') {
        lines.push_back(line);
        line.clear();
      } else if (!line.empty()) line += ' ';
    } else word += ch;
  }
  flush_word();
  if (!line.empty()) lines.push_back(line);
  return lines;
}
Interface readable_interface(const View& v) {
  Canvas c(v);
  const float base = c.scale;
  const float preference = std::isfinite(v.text_scale) && v.text_scale > 0 ? std::max(1.F, v.text_scale) : 1.F;
  const auto font = [&](float baseline) { return std::max(baseline * base, 14.F) * preference; };
  const auto safe = ui::inset(v.viewport, v.safe_area);
  c.scale = c.type_scale = 1;
  c.ox = safe.x; c.oy = safe.y;
  const float width = safe.width - 32, gap = 8;
  const float button_size = font(17), count_size = font(14);
  const float row = std::max(44.F, button_size + 12);
  const float heading_size = font(v.save_error ? 28.F : v.screen == Screen::title ? 43.F : v.screen == Screen::hangar ? 32.F : v.screen == Screen::upgrade ? 19.F : 29.F);
  const float body_size = font(v.screen == Screen::blueprints ? 22.F : v.screen == Screen::death ? 30.F : v.screen == Screen::hangar ? 20.F : v.screen == Screen::upgrade ? 14.F : 19.F);
  const bool upgrade_tabs = v.screen == Screen::upgrade && !v.save_error;
  const bool visible_counter = safe.height >= 500 && !upgrade_tabs;
  const float content_top = upgrade_tabs ? row + 20 : visible_counter ? count_size * 1.25F + 20 : 12.F;
  const Command back = v.save_error ? Command::none : v.show_privacy && v.screen == Screen::settings ? Command::privacy : v.show_build ? Command::current_build : v.screen == Screen::hangar || v.screen == Screen::settings || v.screen == Screen::blueprints ? Command::home : Command::none;
  auto sections = readable_sections(v);
  const auto button_height = [&](std::string_view label) {
    const auto lines = readable_lines(c, label, width - 24, button_size);
    return std::max(44.F, button_size + static_cast<float>(lines.empty() ? 0 : lines.size() - 1) * button_size * 1.25F + 12);
  };
  // Keep useful primary menu actions together. Only genuinely overflowing
  // content/items page; a small text preference does not turn menus into tours.
  std::vector<ReadableSection> primary;
  const bool primary_screen = !v.save_error && !v.show_build && !v.show_privacy && (v.screen == Screen::title || v.screen == Screen::pause || v.screen == Screen::settings || v.screen == Screen::death || v.screen == Screen::results);
  if (primary_screen) {
    float dock_height = back == Command::none ? 0.F : row + gap;
    for (auto section : sections)
      if (section.command != Command::none) {
        if (section.command == Command::location_next) section.label = std::string(location_name(v.theme)) + " >";
        if (section.command == Command::current_build) section.label = "BUILD";
        if (section.command == Command::finish && v.screen == Screen::pause) section.label = "END RUN";
        dock_height += button_height(section.label) + gap;
        primary.push_back(std::move(section));
      }
    const float room = safe.height - content_top - dock_height - row - gap - 12;
    if (room >= heading_size) {
      std::string text = sections.front().text;
      if (v.screen == Screen::title) text += "\nLocation: " + std::string(location_name(v.theme)) + ".";
      if (v.screen == Screen::pause) text += "\nHardware and research last for this flight. Hangar ranks are permanent. End Run banks earned credits once; flights shorter than 30 seconds earn none.";
      sections = {{std::move(text), Command::none, {}, true}};
    } else primary.clear();
  }
  struct Line final { std::string text; float size; };
  struct Page final { unsigned section; std::vector<Line> lines; };
  std::vector<Page> pages;
  for (unsigned i = 0; i < sections.size(); ++i) {
    const auto& section = sections[i];
    float dock_height = back == Command::none ? 0.F : row + gap;
    for (const auto& action : primary) dock_height += button_height(action.label) + gap;
    if (primary.empty() && section.command != Command::none) dock_height += button_height(section.label) + gap;
    const float room = safe.height - content_top - dock_height - row - gap - 12;
    std::vector<Line> lines;
    std::istringstream paragraphs(section.text);
    std::string paragraph;
    bool heading = true;
    while (std::getline(paragraphs, paragraph)) {
      float size = heading ? heading_size : body_size;
      if (v.screen == Screen::results && !v.save_error) {
        if (paragraph.starts_with("SCORE ")) size = font(44);
        else if (paragraph.starts_with("+ ")) size = font(38);
      }
      for (auto line : readable_lines(c, paragraph, width, size)) lines.push_back({std::move(line), size});
      heading = false;
    }
    Page page{i, {}};
    float used = 0;
    for (const auto& line : lines) {
      if (!page.lines.empty() && used + line.size > room) {
        pages.push_back(std::move(page));
        page = {i, {}};
        used = 0;
      }
      page.lines.push_back(line);
      used += line.size * 1.25F;
    }
    if (!page.lines.empty()) pages.push_back(std::move(page));
  }
  if (pages.empty()) return std::move(c.output);
  c.output.page_count = static_cast<unsigned>(pages.size());
  c.output.page = std::min(v.ui_page, c.output.page_count - 1);
  const auto& page = pages[c.output.page];
  const auto& section = sections[page.section];
  c.fill(0, 0, safe.width, safe.height, navy);
  if (upgrade_tabs) {
    const float tab_width = (width - 2 * gap) / 3;
    for (unsigned i = 0; i < 3; ++i) {
      const float x = 16 + static_cast<float>(i) * (tab_width + gap);
      const bool selected = i == std::min(v.upgrade_preview, 2U);
      c.fill(x, 8, tab_width, row, selected ? mint : panel);
      if (tab_width >= 88) {
        c.fill(x + 7, 8 + (row - 32) * .5F, 32, 32, panel);
        c.icon(x + 10, 8 + (row - 26) * .5F, choice_icon(v.run->choices[i]), 26);
        c.centered(x + 42, 14, tab_width - 48, number(i + 1), button_size, selected ? navy : white);
      } else c.centered(x, 14, tab_width, number(i + 1), button_size, selected ? navy : white);
      c.output.buttons.push_back({c.rect(x, 8, tab_width, row), static_cast<Command>(static_cast<unsigned>(Command::preview0) + i), "PREVIEW " + number(i + 1), true});
    }
  }
  if (visible_counter) c.centered(16, 8, width, number(c.output.page + 1) + " / " + number(c.output.page_count), count_size, mint);
  float y = content_top;
  for (const auto& line : page.lines) {
    c.text(16, y, line.text, line.size);
    c.output.page_text += line.text + "\n";
    y += line.size * 1.25F;
  }
  const auto button = [&](float x, float top, float w, float height, std::string label, Command command, bool enabled = true) {
    c.fill(x, top, w, height, enabled ? mint : panel);
    const auto lines = readable_lines(c, label, w - 24, button_size);
    const float text_height = button_size + static_cast<float>(lines.empty() ? 0 : lines.size() - 1) * button_size * 1.25F;
    float baseline = top + (height - text_height) * .5F;
    for (const auto& line : lines) {
      c.centered(x + 12, baseline, w - 24, line, button_size, enabled ? navy : muted);
      baseline += button_size * 1.25F;
    }
    c.output.buttons.push_back({c.rect(x, top, w, height), command, std::move(label), enabled});
  };
  float footer = safe.height - 12;
  if (back != Command::none) {
    footer -= row;
    button(16, footer, width, row, "BACK", back);
    footer -= gap;
  }
  footer -= row;
  if (c.output.page_count > 1) {
    button(16, footer, (width - gap) * .5F, row, "<", Command::page_previous, c.output.page > 0);
    button(16 + (width + gap) * .5F, footer, (width - gap) * .5F, row, ">", Command::page_next, c.output.page + 1 < c.output.page_count);
    footer -= gap;
  }
  if (!primary.empty()) {
    const auto first_action = c.output.buttons.size();
    for (auto action = primary.rbegin(); action != primary.rend(); ++action) {
      const float height = button_height(action->label);
      footer -= height;
      button(16, footer, width, height, action->label, action->command, action->enabled);
      footer -= gap;
    }
    std::reverse(c.output.buttons.begin() + static_cast<std::ptrdiff_t>(first_action), c.output.buttons.end());
  } else if (section.command != Command::none) {
    const float height = button_height(section.label);
    button(16, footer - height, width, height, section.label, section.command, section.enabled);
    if (upgrade_tabs && page.section == 0 && footer - height - y >= 104) {
      const auto& pick = v.run->choices[std::min(v.upgrade_preview, 2U)];
      const float icon_top = y + (footer - height - y - 80) * .5F;
      c.fill(safe.width * .5F - 40, icon_top, 80, 80, panel);
      c.icon(safe.width * .5F - 30, icon_top + 8, choice_icon(pick), 60);
      if (pick.hardware)
        for (unsigned rank = 0; rank < 3; ++rank)
          c.fill(safe.width * .5F - 23 + static_cast<float>(rank) * 17, icon_top + 70, 12, 4,
              rank + 1 < pick.next_rank ? mint : rank + 1 == pick.next_rank ? gold : navy);
    }
  }
  return std::move(c.output);
}
Interface build_overview(const View& v) {
  Canvas c(v);
  const auto& run = *v.run;
  c.output.page_count = 3;
  c.output.page = std::min(v.ui_page, 2U);
  const auto line = [&](float x, float y, std::string text, float size, Color color = white) {
    c.fitted(x, y, 322, text, size, color);
    c.output.page_text += text + "\n";
  };
  c.fill(8, 8, 374, 828, navy);
  line(24, 26, "CURRENT BUILD", 29);
  constexpr std::array<std::string_view, 3> pages{"HARDWARE & EVOLUTIONS", "FLIGHT RESEARCH", "PERMANENT HANGAR"};
  line(24, 73, std::string(pages[c.output.page]), 17, mint);
  if (c.output.page == 0) {
    line(24, 108, number(hardware_count(run)) + " / 6 hardware slots", 16, muted);
    c.fill(20, 142, 350, 366, panel);
    unsigned slot = 0;
    for (unsigned i = 0; i < run.modules.size(); ++i) {
      if (!run.modules[i])
        continue;
      const float y = 150 + static_cast<float>(slot++) * 59;
      line(34, y, std::string(gameplay::tuning::module_names[i]), 18);
      line(34, y + 27, "rank " + number(run.modules[i]) + " of 3", 14, muted);
      for (unsigned rank = 0; rank < 3; ++rank)
        c.fill(290 + static_cast<float>(rank) * 18, y + 31, 12, 7, rank < run.modules[i] ? mint : navy);
    }
    for (; slot < 6; ++slot) {
      const float y = 150 + static_cast<float>(slot) * 59;
      line(34, y, "Empty hardware slot", 17, muted);
    }
    line(24, 532, "ACTIVE EVOLUTIONS", 17, mint);
    unsigned row = 0;
    for (unsigned i = 0; i < gameplay::tuning::evolution_names.size(); ++i)
      if (run.evolution_bits & (1U << i))
        line(24, 568 + static_cast<float>(row++) * 34, std::string(gameplay::tuning::evolution_names[i]), 19);
    if (!run.evolution_bits) {
      line(24, 568, "Both ingredients need rank III.", 14, muted);
      line(24, 596, "First two recipes evolve automatically.", 14, muted);
    }
    line(24, 652, "Hardware lasts for this run.", 14, muted);
  } else if (c.output.page == 1) {
    unsigned row = 0;
    for (unsigned i = 0; i < run.research.size(); ++i) {
      if (run.research[i] <= 0)
        continue;
      const float y = 134 + static_cast<float>(row++) * 72;
      line(24, y, std::string(gameplay::tuning::research_names[i]) + ":", 18);
      const bool points = i == static_cast<unsigned>(gameplay::Research::hull_reinforcement) || i == static_cast<unsigned>(gameplay::Research::shield_capacity);
      line(24, y + 30, "+" + metric_number(run.research[i]) + (points ? " points" : i == static_cast<unsigned>(gameplay::Research::emergency_repairs) ? "% immediate repair earned"
                                                                                                                                                      : "% of base"),
          16, mint);
    }
    if (!row) {
      line(24, 146, "No research earned this flight.", 16);
      line(24, 188, "Choose research at a promotion.", 14, muted);
    }
    line(24, 668, "TEMPORARY FLIGHT RESEARCH", 14, muted);
  } else {
    unsigned row = 0;
    for (const auto& track : v.store->hangar()) {
      const float y = 140 + static_cast<float>(row++) * 122;
      line(24, y, std::string(track.title), 20);
      line(24, y + 32, "rank " + number(track.rank) + " of " + number(track.maximum_rank), 16, mint);
      float copy_y = y + 64;
      for (auto text : readable_lines(c, track.benefit, 322, 14)) {
        line(24, copy_y, std::move(text), 14, muted);
        copy_y += std::max(14.F, 14.F / c.scale) * 1.2F;
      }
    }
    line(24, 654, "Hangar ranks apply to future runs.", 14, muted);
  }
  const float navigation_height = std::max(52.F, 44.F / c.scale);
  const float back_height = std::max(44.F, 44.F / c.scale);
  c.button(26, 690, 156, "PREVIOUS", Command::page_previous, c.output.page > 0, white, navigation_height);
  c.button(208, 690, 156, "NEXT", Command::page_next, c.output.page < 2, white, navigation_height);
  c.button(26, 832 - back_height, 338, "BACK TO PAUSE", Command::current_build, true, mint, back_height);
  return std::move(c.output);
}
// Enlarged run text shares two bounded tactical strips. The approved UI atlas
// has transparent vertical bearings: centre the visible caps/digits/slash ink
// (16..59 relative to the nominal anchor at 48pt, including linear-filter
// support), rather than the nominal
// em. Cooked-alpha regressions verify
// this envelope for every rendered character; ordinary typography is unchanged.
Interface enlarged_run(const View& v, const GameplayFraming& framing) {
  Canvas c(v);
  const auto safe = c.safe;
  c.scale = c.type_scale = 1;
  c.ox = safe.x;
  c.oy = safe.y;
  const float size = 14.F * std::max(1.F, v.text_scale), row = GameplayFraming::tactical_row_height;
  const float ink_height = size * 43.F / 48.F;
  const auto line = [&](float x, float y, float width, std::string_view copy, Color color, bool centered = false) {
    if (c.measure(copy, size) > width) c.output.text_overflow = true;
    const float origin = y + (row - ink_height) * .5F - size * 16.F / 48.F;
    if (centered) c.centered(x, origin, width, copy, size, color);
    else c.text(x, origin, copy, size, color);
  };
  const auto bar = [&](float x, float y, float width, float fraction, Color color) {
    c.fill(x, y, width, 2, panel);
    c.fill(x, y, width * std::clamp(fraction, 0.F, 1.F), 2, color);
  };
  const float pause_x = safe.width - row - 4, stats_width = pause_x - 12;
  c.fill(0, 0, safe.width, row, navy);
  // XP remains visible without adding a second header row.
  bar(4, 0, stats_width, v.run->xp / std::max(1.F, v.run->next_level_xp), gold);
  if (v.damage_feedback.age >= 0) {
    const auto cause = v.damage_feedback.cause == gameplay::DamageCause::contact ? "BUMP" :
        v.damage_feedback.cause == gameplay::DamageCause::projectile ? "SHOT" : "HIT";
    const std::string copy = std::string(v.damage_feedback.hull ? "HP:" : "SH:") + cause;
    line(4, 0, stats_width, copy, v.damage_feedback.hull ? coral : mint);
    c.fill(4, row - 3, stats_width, 3, v.damage_feedback.hull ? coral : mint);
  } else {
    const std::string hull = "HP " + number(static_cast<unsigned>(std::max(0.F, v.run->hull)));
    const std::string shield = "SH " + number(static_cast<unsigned>(std::max(0.F, v.run->shield)));
    if (c.measure(hull + "  " + shield, size) <= stats_width) {
      line(4, 0, stats_width, hull + "  " + shield, white);
    } else {
      std::string hp = hull;
      bool hull_fraction = false;
      const float shield_width = c.measure("SH", size);
      if (c.measure(hp, size) + 8 + shield_width > stats_width) {
        const auto percent = static_cast<unsigned>(std::ceil(100.F * std::clamp(v.run->hull / std::max(1.F, v.run->maximum_hull), 0.F, 1.F)));
        hp = "HP" + number(percent) + "%";
      }
      if (framing.banded && c.measure(hp, size) > stats_width - shield_width - 8) {
        const float fraction = std::clamp(v.run->hull / std::max(1.F, v.run->maximum_hull), 0.F, 1.F);
        hp = fraction >= 1 ? "HP MAX" : "HP" + number(static_cast<unsigned>(std::floor(100.F * fraction))) + "%";
      }
      if (framing.banded && c.measure(hp, size) > stats_width - shield_width - 8) {
        hp = "HP";
        hull_fraction = true;
      }
      line(4, 0, stats_width - shield_width - 8, hp, white);
      if (hull_fraction) bar(4, row - 2, stats_width - shield_width - 8, v.run->hull / std::max(1.F, v.run->maximum_hull), mint);
      const float sx = 4 + stats_width - shield_width;
      line(sx, 0, shield_width, "SH", {.57F, .83F, 1, 1});
      bar(sx, row - 2, shield_width, v.run->shield / std::max(1.F, v.run->maximum_shield), {.57F, .83F, 1, 1});
    }
  }
  c.fill(pause_x, 0, row, row, white);
  // Familiar pause marks need no oversized textual bearings.
  c.fill(pause_x + 13, 12, 6, 20, navy);
  c.fill(pause_x + 25, 12, 6, 20, navy);
  c.output.buttons.push_back({c.rect(pause_x, 0, row, row), Command::pause, "PAUSE", true});

  const float bottom = safe.height - row - GameplayFraming::tactical_bottom_margin, available = safe.width - 8;
  const bool detailed = c.measure("DASH 4s", size) + c.measure("SWEEP 2/3", size) + 24 <= available;
  // The new short-window fallback uses a narrow divider so both complete
  // action/status words retain the requested 3.4x type in a 320pt window.
  const float gutter = framing.banded ? 2.F : 8.F;
  const bool dash_icon = framing.banded && c.measure("DASH", size) + std::max(c.measure("BOOST", size), c.measure("ADDED", size)) + 16 + gutter > available;
  const float dash_width = dash_icon ? std::max({44.F, c.measure(">>", size) + 8, c.measure("4s", size) + 8}) :
      std::max(44.F, c.measure(detailed ? "DASH 4s" : "DASH", size) + 8);
  const float status_width = available - dash_width - gutter, dash_x = safe.width - 4 - dash_width;
  const bool ready = v.run->dash_cooldown_seconds <= 0;
  const auto seconds = number(static_cast<unsigned>(std::ceil(v.run->dash_cooldown_seconds))) + "s";
  const std::string dash = ready ? (dash_icon ? ">>" : "DASH") : detailed && !dash_icon ? "DASH " + seconds : seconds;
  c.fill(dash_x, bottom, dash_width, row, ready ? mint : panel);
  line(dash_x + 4, bottom, dash_width - 8, dash, ready ? navy : muted, true);
  c.output.buttons.push_back({c.rect(dash_x, bottom, dash_width, row), Command::dash, ready ? "DASH" : "DASH " + seconds, ready});

  std::string status;
  Color status_color = white;
  float fraction = -1;
  if (v.promotion.installed) {
    status = "ADDED";
    c.output.page_text = "INSTALLED " + v.promotion.installed->title;
    status_color = mint;
  } else if (v.run->harvest_overdrive_seconds > 0) {
    status = "BOOST " + number(static_cast<unsigned>(std::ceil(v.run->harvest_overdrive_seconds))) + "s";
    if (c.measure(status, size) > status_width - 8) status = "BOOST";
    fraction = v.run->harvest_overdrive_seconds / gameplay::tuning::harvest_overdrive_seconds;
    status_color = mint;
  } else if (v.run->harvest_chain) {
    status = "SWEEP " + number(v.run->harvest_chain) + "/" + number(gameplay::tuning::harvest_chain_target);
    if (c.measure(status, size) > status_width - 8) status = number(v.run->harvest_chain) + "/" + number(gameplay::tuning::harvest_chain_target);
    fraction = v.run->harvest_chain_seconds / gameplay::tuning::harvest_chain_window_seconds;
    status_color = mint;
  } else if (v.run->stats.active_seconds < 12) {
    status = v.run->stats.active_seconds < 4 ? "DRAG TO FLY" : "FLY CLOSE";
    if (c.measure(status, size) > status_width - 8) status = v.run->stats.active_seconds < 4 ? "DRAG" : "CLOSE";
  } else {
    status = "LV " + number(v.run->stats.level);
    if (c.measure(status, size) > status_width - 8) status = "LEVEL";
  }
  c.fill(4, bottom, status_width, row, navy);
  line(8, bottom, status_width - 8, status, status_color, true);
  if (fraction >= 0) {
    bar(8, bottom + row - 2, status_width - 8, fraction, mint);
    if (v.run->harvest_chain && v.run->harvest_overdrive_seconds <= 0 && !v.promotion.installed) {
      const float pip = (status_width - 16) / static_cast<float>(gameplay::tuning::harvest_chain_target);
      for (unsigned i = 1; i < gameplay::tuning::harvest_chain_target; ++i)
        c.fill(8 + static_cast<float>(i) * pip, bottom + row - 2, 2, 2, navy);
    }
  }
  if (framing.banded && v.stick_active) {
    // Semantic contacts remain full-window points, including the clear flanks.
    c.ox = c.oy = 0;
    c.fill(v.stick_origin.x - 24, v.stick_origin.y - 24, 48, 48, {.06F, .2F, .25F, .35F});
    const float dx = std::clamp(v.stick_position.x - v.stick_origin.x, -38.F, 38.F), dy = std::clamp(v.stick_position.y - v.stick_origin.y, -38.F, 38.F);
    c.fill(v.stick_origin.x + dx - 8, v.stick_origin.y + dy - 8, 16, 16, {.5F, 1, .81F, .65F});
  }
  return std::move(c.output);
}
Interface compact_run(const View& v, const GameplayFraming& framing) {
  Canvas c(v);
  c.scale = c.type_scale = 1;
  c.ox = c.oy = 0;
  const float size = 14 * (std::isfinite(v.text_scale) ? std::max(1.F, v.text_scale) : 1.F);
  const auto left = framing.left_rail, right = framing.right_rail;
  c.fill(left.x, left.y, left.width, left.height, navy);
  c.fill(right.x, right.y, right.width, right.height, navy);
  const float lx = left.x + 4, lw = left.width - 8;
  float y = left.y + 8;
  const auto lines = [&](std::string_view label, Color color) {
    for (const auto& line : readable_lines(c, label, lw, size)) {
      c.centered(lx, y, lw, line, size, color);
      y += size * 1.15F;
    }
    y += 6;
  };
  lines("HP", muted);
  lines(number(static_cast<unsigned>(std::max(0.F, v.run->hull))), white);
  c.fill(lx, y, lw, 4, panel);
  c.fill(lx, y, lw * std::clamp(v.run->hull / std::max(1.F, v.run->maximum_hull), 0.F, 1.F), 4, mint);
  y += 12;
  lines("SH", muted);
  lines(number(static_cast<unsigned>(std::max(0.F, v.run->shield))), { .57F, .83F, 1, 1 });
  if (v.damage_feedback.age >= 0) {
    const auto cause=v.damage_feedback.cause==gameplay::DamageCause::contact?"BUMP":v.damage_feedback.cause==gameplay::DamageCause::projectile?"SHOT":"HIT";
    lines(std::string(v.damage_feedback.hull?"HP\n":"SH\n")+cause,v.damage_feedback.hull?coral:mint);
  }
  else lines("LV " + number(v.run->stats.level), gold);
  // Secondary score/build details retain their existing paused, readable view.
  if (y + size + 14 <= left.bottom()) {
    lines("XP", gold);
    c.fill(lx, y, lw, 4, panel);
    c.fill(lx, y, lw * std::clamp(v.run->xp / std::max(1.F, v.run->next_level_xp), 0.F, 1.F), 4, gold);
  }
  if (y > left.bottom() - 4) c.output.text_overflow = true;
  const float rx = right.x + 4, rw = right.width - 8, button_height = std::max(44.F, size + 16);
  const float pause_y = right.y + 8, dash_y = right.bottom() - button_height - 8;
  c.fill(rx, pause_y, rw, button_height, white);
  c.centered(rx, pause_y + (button_height - size) * .5F, rw, "II", size, navy);
  c.output.buttons.push_back({{rx, pause_y, rw, button_height}, Command::pause, "PAUSE", true});
  const bool ready = v.run->dash_cooldown_seconds <= 0;
  c.fill(rx, dash_y, rw, button_height, ready ? mint : panel);
  c.centered(rx, dash_y + (button_height - size) * .5F, rw, "DASH", size, ready ? navy : muted);
  c.output.buttons.push_back({{rx, dash_y, rw, button_height}, Command::dash,
      ready ? "DASH" : "DASH " + number(static_cast<unsigned>(std::ceil(v.run->dash_cooldown_seconds))) + "s", ready});
  float status_y = pause_y + button_height + 16;
  const auto status = [&](std::string_view text, Color color) {
    const auto wrapped = readable_lines(c, text, rw, size);
    const float height = static_cast<float>(wrapped.size()) * size * 1.15F;
    if (status_y + height > dash_y - 8) { c.output.text_overflow = true; return; }
    for (const auto& line : wrapped) {
      c.centered(rx, status_y, rw, line, size, color);
      status_y += size * 1.15F;
    }
    status_y += 8;
  };
  if (!ready) status(number(static_cast<unsigned>(std::ceil(v.run->dash_cooldown_seconds))) + "s", muted);
  if (v.promotion.installed) {
    const std::string full = "ADDED\n" + v.promotion.installed->title;
    const auto count = readable_lines(c, full, rw, size).size();
    bool words_fit=true;
    std::istringstream words{v.promotion.installed->title};
    for(std::string word;words>>word;)if(c.measure(word,size)>rw)words_fit=false;
    status(words_fit && status_y + static_cast<float>(count) * size * 1.15F <= dash_y - 8 ? full : "ADDED", mint);
  }
  else if (v.run->harvest_overdrive_seconds > 0) status("BOOST\n" + number(static_cast<unsigned>(std::ceil(v.run->harvest_overdrive_seconds))) + "s", mint);
  else if (v.run->harvest_chain) status("SWEEP\n" + number(v.run->harvest_chain) + "/" + number(gameplay::tuning::harvest_chain_target), mint);
  else if (v.run->stats.active_seconds < 12) status(v.run->stats.active_seconds < 4 ? "DRAG TO FLY" : "FLY CLOSE", white);
  if (v.stick_active) {
    c.fill(v.stick_origin.x - 24, v.stick_origin.y - 24, 48, 48, {.06F, .2F, .25F, .35F});
    const float dx = std::clamp(v.stick_position.x - v.stick_origin.x, -38.F, 38.F), dy = std::clamp(v.stick_position.y - v.stick_origin.y, -38.F, 38.F);
    c.fill(v.stick_origin.x + dx - 8, v.stick_origin.y + dy - 8, 16, 16, {.5F, 1, .81F, .65F});
  }
  return std::move(c.output);
}
} // namespace
void flight_report_panel(Canvas& c, const View& v) {
  const auto report = flight_report(v);
  const auto first_quad = c.output.quads.size();
  const auto line = [&](float x, float y, float width, std::string_view value, float size, Color color = white, bool title = false, bool center = false) {
    c.fitted(x, y, width, value, size, color, title, center);
    if (c.measure(value, 14.F / c.scale, title) > width) c.output.text_overflow = true;
  };
  c.fill(20, 24, 350, 108, navy);
  line(34, 38, 322, report.heading, 29, white, true);
  c.wrap(34, 82, 322, report.status, 16, muted, 2);
  c.fill(20, 150, 350, 260, navy);
  c.fill(20, 150, 4, 260, report.preview ? gold : mint);
  line(38, 164, 314, report.reward_label, 17, muted);
  line(38, 199, 314, "+ " + number(report.credits), 48, report.preview ? gold : mint, true);
  c.fill(24, 272, 346, 138, white);
  line(38, 283, 314, "SCORE " + number(report.score) + (!report.preview && !report.guest && !report.pending ? " / BEST " + number(report.best_score) : ""), 22, navy);
  line(38, 329, 146, "CAPTURES", 14, navy);
  line(206, 329, 146, "SURVIVED", 14, navy);
  line(38, 359, 146, number(report.captures), 25, navy);
  line(206, 359, 146, time_text(report.seconds), 25, navy);
  c.fill(20, 430, 350, 214, navy);
  if (report.preview) {
    line(36, 444, 318, "FLIGHT BUILD", 20, white, true);
    line(36, 483, 318, "REACHED LEVEL " + number(v.run->stats.level), 20, mint);
    line(36, 518, 318, number(hardware_count(*v.run)) + " / 6 hardware slots filled", 16, muted);
    c.wrap(36, 561, 318, "Dash gives brief protection. Close sweeps build Overdrive.", 17, white, 3);
  } else if (report.guest || report.pending) {
    line(36, 446, 318, report.guest ? "GUEST FLIGHT" : "SAVING RESEARCH", 21, white, true);
    c.wrap(36, 491, 318, report.guest ? "Saving is unavailable. Permanent progress has not changed." : "Your reward is not banked yet. Retry saving before continuing.", 18, muted, 5);
  } else {
    line(36, 443, 318, report.blueprint_heading, 14, mint);
    line(36, 475, 318, report.blueprint_name, 22);
    line(36, 510, 318, report.blueprint_detail, 14, muted);
    c.fill(36, 542, 318, 5, panel);
    if (report.blueprint_fraction > 0) c.fill(36, 542, 318 * report.blueprint_fraction, 5, mint);
    line(36, 558, 318, report.hangar_name, 18);
    line(36, 588, 318, report.hangar_detail, 14, muted);
    c.fill(36, 624, 318, 5, panel);
    if (report.hangar_fraction > 0) c.fill(36, 624, 318 * report.hangar_fraction, 5, gold);
  }
  // One short report arrival, no count-up or delayed actions. Text, rewards,
  // semantics and hit targets are final from the first frame.
  if (!v.promotion.reduced_motion && v.report_entry_seconds >= 0 && v.report_entry_seconds < .22F) {
    const float t = std::clamp(v.report_entry_seconds / .22F, 0.F, 1.F);
    const float offset = 4 * (1-t) * (1-t) * c.scale / c.h;
    for (auto i = first_quad; i < c.output.quads.size(); ++i) c.output.quads[i].rect[1] += offset;
  }
  c.button(26, 670, 338, report.preview ? "BANK CREDITS & RESULTS" : "FLY AGAIN", report.preview ? Command::finish : Command::retry, true, mint, 60);
  if (!report.preview) c.button(26, 752, 338, report.hangar_action, Command::hangar, true, white, 56);
}
Interface standard_interface(const View& v) {
  if (v.viewport.width > v.viewport.height && v.screen != Screen::run)
    return wide_interface(v);
  Canvas c(v);
  const auto& run = *v.run;
  const auto& store = *v.store;
  const auto& account = store.account();
  const auto heading = [&](std::string_view title, std::string_view subtitle) {c.fill(8,8,374,828,navy);c.text(24,30,title,29,white,true);c.wrap(24,76,342,subtitle,16,muted,3); };
  if (v.screen == Screen::run) {
    c.button(328, 10, 54, "II", Command::pause, true, white, 48);
    const auto pause_bounds = c.output.buttons.back().bounds;
    const auto safe = c.safe;
    const float original_scale = c.scale, original_ox = c.ox, original_oy = c.oy, original_type_scale = c.type_scale;
    const auto stat_size = [&](float baseline) { return std::max(baseline * original_scale, 14.F) * original_type_scale; };
    const auto short_number = [](std::uint64_t value) {
      if (value < 10000) return number(value);
      constexpr std::array<std::string_view,6> suffix{"K","M","B","T","P","E"};
      unsigned unit = 0;
      value /= 1000;
      while (value >= 1000 && unit + 1 < suffix.size()) { value /= 1000; ++unit; }
      return number(value) + std::string(suffix[unit]);
    };
    c.scale = c.type_scale = 1;
    c.ox = c.oy = 0;
    const float hull_size = stat_size(17), other_size = stat_size(14);
    const bool damaged = v.damage_feedback.age >= 0;
    const std::string hull = (damaged && v.damage_feedback.hull ? std::string(damage_label(v.damage_feedback)) + " " : "HP ") + short_number(static_cast<std::uint64_t>(std::max(0.F, run.hull)));
    const std::string shield = (damaged && !v.damage_feedback.hull ? std::string(damage_label(v.damage_feedback)) + " " : "SH ") + short_number(static_cast<std::uint64_t>(std::max(0.F, run.shield)));
    const std::string level = "LV " + short_number(run.stats.level);
    const std::string score = "SCORE " + short_number(run.stats.score);
    const float left = safe.x + 8, right = pause_bounds.x - 8;
    const float gap = 7;
    const float hull_width = c.measure(hull, hull_size), shield_width = c.measure(shield, other_size), score_width = c.measure(score, other_size), level_width = c.measure(level, hull_size);
    if (hull_width + shield_width + (damaged ? 0 : score_width + gap) + gap > right - left) c.output.text_overflow = true;
    const float panel_height = pause_bounds.height - 4;
    const float top = pause_bounds.y + 2;
    c.fill(left - 4, top, right - left + 8, panel_height, navy);
    c.text(left, top + 2, hull, hull_size, damaged && v.damage_feedback.hull ? coral : white);
    c.text(left + hull_width + gap, top + 2, shield, other_size, damaged && !v.damage_feedback.hull ? mint : Color{.57F,.83F,1,1});
    const float score_x = right - score_width;
    const bool level_in_top = hull_width + shield_width + level_width + score_width + 3 * gap <= right - left;
    if (!damaged && level_in_top)
      c.text(left + hull_width + shield_width + 2 * gap, top + 2, level, hull_size);
    if (!damaged) c.text(score_x, top + 2, score, other_size);
    const float bar_top = top + hull_size + 7;
    const float hull_fraction = std::clamp(run.hull / std::max(1.F, run.maximum_hull), 0.F, 1.F);
    const float shield_fraction = std::clamp(run.shield / std::max(1.F, run.maximum_shield), 0.F, 1.F);
    const auto shield_color = Color{.57F, .83F, 1, 1};
    c.fill(left, bar_top, hull_width, 4, {.16F, .29F, .33F, 1});
    c.fill(left, bar_top, hull_width * hull_fraction, 4, damaged && v.damage_feedback.hull ? coral : mint);
    const float shield_x = left + hull_width + gap;
    c.fill(shield_x, bar_top, shield_width, 4, {.16F, .29F, .33F, 1});
    c.fill(shield_x, bar_top, shield_width * shield_fraction, 4, shield_color);
    float progress_x = shield_x + shield_width + gap;
    if (!damaged && !level_in_top) {
      c.text(progress_x, bar_top - 3, level, other_size);
      progress_x += c.measure(level, other_size) + gap;
    }
    const float label_width = c.measure("XP", other_size);
    const float progress_width = right - progress_x - label_width - 4;
    if (progress_width >= 12 && !damaged) {
      c.text(progress_x, bar_top - 3, "XP", other_size, gold);
      c.fill(progress_x + label_width + 4, bar_top + 3, progress_width, 4, {.16F, .29F, .33F, 1});
      c.fill(progress_x + label_width + 4, bar_top + 3, progress_width * std::clamp(run.xp / std::max(1.F, run.next_level_xp), 0.F, 1.F), 4, gold);
    }
    c.scale = original_scale; c.ox = original_ox; c.oy = original_oy;
    c.type_scale = std::isfinite(v.text_scale) && v.text_scale > 1 ? v.text_scale : 1;
    const bool overdrive = run.harvest_overdrive_seconds > 0;
    if (run.stats.active_seconds < 12 && !overdrive && !run.harvest_chain) {
      c.fill(20, 666, 350, 64, navy);
      const bool keyboard = v.hint_device == input::DeviceKind::keyboard;
      const bool controller = v.hint_device == input::DeviceKind::gamepad;
      const auto movement_hint = keyboard ? "WASD / ARROWS TO FLY" : controller ? "LEFT STICK TO FLY" : "DRAG TO FLY";
      const auto dash_hint = keyboard ? "SPACE TO DODGE" : controller ? "RIGHT FACE BUTTON TO DODGE" : "TAP DASH TO DODGE";
      c.fitted(28, 675, 334, run.stats.active_seconds < 4 ? movement_hint : "FLY CLOSE. CAPTURE FASTER.", 17, white, false, true);
      c.fitted(28, 703, 334, run.stats.active_seconds < 4 ? dash_hint : "3 QUICK SWEEPS = OVERDRIVE", 14, muted, false, true);
    }
    if (overdrive || run.harvest_chain) {
      const auto color = overdrive ? navy : white;
      c.fill(20, 752, 190, 60, overdrive ? mint : navy);
      c.fitted(28, 758, 174, overdrive ? "OVERDRIVE " + number(static_cast<unsigned>(std::ceil(run.harvest_overdrive_seconds))) + "s" : "SWEEP " + number(run.harvest_chain) + "/" + number(gameplay::tuning::harvest_chain_target), 17, color, false, true);
      // Reserve the atlas bearings as well as advances at the physical text
      // floor: tight phones use the shorter label without reducing legibility.
      const float caption_bearing = std::max(14.F, 14.F / c.scale) * 64.F / 48.F;
      const std::string benefit = overdrive ? (c.measure("CAPTURE/FIRE", 14) + caption_bearing <= 174 ? "CAPTURE/FIRE" : "BOOST") : number(gameplay::tuning::harvest_chain_target - run.harvest_chain) + " TO OVERDRIVE";
      c.fitted(28, 785, 174, benefit, 14, color, false, true);
      const float remaining = overdrive ? run.harvest_overdrive_seconds / gameplay::tuning::harvest_overdrive_seconds : run.harvest_chain_seconds / gameplay::tuning::harvest_chain_window_seconds;
      c.fill(32, 807, 166, 2, panel);
      c.fill(32, 807, 166 * std::clamp(remaining, 0.F, 1.F), 2, overdrive ? navy : mint);
    }
    c.button(224, 752, 150, run.dash_cooldown_seconds > 0 ? "DASH " + number(static_cast<unsigned>(std::ceil(run.dash_cooldown_seconds))) + "s" : "DASH", Command::dash, run.dash_cooldown_seconds <= 0, mint, 60);
    if (v.stick_active) {
      const float x = (v.stick_origin.x - c.ox) / c.scale, y = (v.stick_origin.y - c.oy) / c.scale;
      c.fill(x - 24, y - 24, 48, 48, {.06F, .2F, .25F, .35F});
      const float dx = std::clamp((v.stick_position.x - v.stick_origin.x) / c.scale, -38.F, 38.F), dy = std::clamp((v.stick_position.y - v.stick_origin.y) / c.scale, -38.F, 38.F);
      c.fill(x + dx - 8, y + dy - 8, 16, 16, {.5F, 1, .81F, .65F});
    }
  } else {
    c.fill(-1000, -1000, 3000, 3000, {.015F, .045F, .09F, .18F});
    switch (v.screen) {
    case Screen::title:
      // The saucer and its capture stage carry the title screen, with controls
      // kept in one reachable dock rather than covering the world with a form.
      c.fill(34, 40, 322, 146, navy);
      c.centered(34, 48, 322, "HUMAN", 36, white, true);
      c.centered(34, 94, 322, "RESOURCES", 43, white, true);
      c.fitted(34, 151, 322, "EVERY ABDUCTION COUNTS", 14, mint, false, true);
      c.button(54, 566, 282, "LOCATION: " + std::string(location_name(v.theme)) + " >", Command::location_next, true, white, 48);
      c.fill(16, 630, 358, 190, navy);
      c.button(36, 650, 318, "START RUN", Command::start, true, mint, 64);
      c.button(36, 738, 150, "HANGAR", Command::hangar, true, white, 56);
      c.button(204, 738, 150, "SETTINGS", Command::settings, true, white, 56);
      break;
    case Screen::upgrade: {
      const float body_size = std::max(14.F, 14.F / c.scale);
      const float title_size = std::max(19.F, 14.F / c.scale) * c.type_scale;
      const float summary_size = body_size * c.type_scale;
      const float header_height = std::max(66.F, 27 + title_size * 1.25F + 10);
      const float first_y = std::max(110.F, 88 + summary_size * 1.25F + 8);
      constexpr float row_gap = 8.F;
      std::array<float, 3> heights{}, label_heights{};
      float required = first_y + 6;
      for (unsigned i = 0; i < 3; ++i) {
        const auto& pick = run.choices[i];
        const float copy_height = static_cast<float>(readable_lines(c, choice_conditions(pick), 322, body_size).size()) * body_size * 1.08F;
        label_heights[i] = static_cast<float>(readable_lines(c, choice_effect_label(pick), 322, body_size).size()) * body_size * 1.08F;
        heights[i] = header_height + 8 + copy_height + 10;
        required += heights[i] + row_gap;
      }
      const float value_height = std::max(26.F, body_size) + 10;
      // Runtime offers have a structured benefit, so they always show it once.
      // Legacy hand-authored fixtures keep their full description and receive
      // the extra value hierarchy only when the three-card board has room.
      const bool legacy_headline = c.scale >= .8F && required + 3 * (value_height + body_size * 1.08F) <= 774;
      for (unsigned i = 0; i < 3; ++i)
        if (!run.choices[i].effect_label.empty() || legacy_headline)
          heights[i] += label_heights[i] + value_height;
      float footer = first_y + 6;
      for (const auto height : heights) footer += height + row_gap;
      if (c.rect(20, first_y, 350, footer + 60 - first_y).bottom() > c.safe.bottom() + .01F)
        c.output.text_overflow = true;
      c.fill(8, 8, 374, std::min(828.F, footer + 66), navy);
      c.text(24, 24, "PROMOTION / LV " + number(run.stats.level), 29, white, true);
      c.fitted(24, 66, 342, "Choose one. Your flight is paused.", 16, muted);
      if (v.promotion.entry_seconds >= 0 && !v.promotion.reduced_motion)
        c.fill(24, 61, 342 * std::clamp(v.promotion.entry_seconds / PromotionFlow::entry_duration, 0.F, 1.F), 2, gold);
      const unsigned occupied = hardware_count(run);
      c.text(24, 88, number(occupied) + "/6 HARDWARE", 14, mint);
      for (unsigned slot = 0; slot < 6; ++slot)
        c.fill(238 + static_cast<float>(slot) * 21, 91, 16, 8, slot < occupied ? mint : panel);
      float card_y = first_y;
      for (unsigned i = 0; i < 3; ++i) {
        const auto first_quad = c.output.quads.size();
        const auto& pick = run.choices[i];
        const float y = card_y, card_height = heights[i];
        card_y += card_height + row_gap;
        c.sprite(20, y, 350, header_height, 536, 1432, 464, 180);
        c.sprite(20, y + header_height, 350, card_height - header_height, 536, 1612, 464, 412);
        const auto color = pick.rarity == gameplay::Rarity::rare ? gold : pick.rarity == gameplay::Rarity::uncommon ? mint : white;
        c.fill(29, y + 9, 48, 48, panel);
        c.icon(33, y + 13, choice_icon(pick), 40);
        c.fitted(87, y + 6, (v.hint_device == input::DeviceKind::keyboard) ? 210.F : 265.F, !pick.evolution_preview.empty() ? pick.evolution_preview : choice_status(pick), 14, !pick.evolution_preview.empty() ? gold : color);
        c.fitted(87, y + 29, 265, pick.title, 19);
        if ((v.hint_device == input::DeviceKind::keyboard)) c.fitted(304, y + 7, 48, "KEY " + number(i + 1), 14, muted, false, true);
        if (pick.hardware) {
          for (unsigned rank = 0; rank < 3; ++rank)
            c.fill(87 + static_cast<float>(rank) * 16, y + header_height - 9, 12, 4, rank + 1 < pick.next_rank ? mint : rank + 1 == pick.next_rank ? gold : panel);
        }
        float copy_y = y + header_height + 8;
        if (!pick.effect_label.empty() || legacy_headline) {
          c.wrap(34, copy_y, 322, choice_effect_label(pick), body_size, navy, 4, 1.08F);
          copy_y += label_heights[i];
          c.fitted(34, copy_y, 322, choice_change(pick), 26, navy);
          copy_y += value_height;
        }
        c.wrap(34, copy_y, 322, choice_conditions(pick), body_size, navy, 12, 1.08F);
        c.output.buttons.push_back({c.rect(20, y, 350, card_height), static_cast<Command>(static_cast<unsigned>(Command::pick0) + i), "CHOOSE " + number(i + 1), true});
        if (v.promotion.entry_seconds >= 0 && !v.promotion.reduced_motion) {
          const float t = std::clamp((v.promotion.entry_seconds - static_cast<float>(i) * .025F) / .2F, 0.F, 1.F);
          const float eased = 1 - (1-t) * (1-t) * (1-t);
          for (std::size_t q = first_quad; q < c.output.quads.size(); ++q) {
            c.output.quads[q].rect[1] += 8 * c.scale * (1-eased) / c.h;
            c.output.quads[q].color[3] *= .85F + .15F * eased;
          }
        }
      }
      if (run.rerolls_remaining)
        c.button(26, footer, 156, "REROLL (" + number(run.rerolls_remaining) + ")", Command::reroll, true, white, 60);
      c.button(208, footer, 156, "EVOLUTIONS", Command::recipe0, true, white, 60);
      break;
    }
    case Screen::pause:
      heading(v.restored_run ? "RUN SAVED" : "ON HOLD", std::string(location_name(v.theme)) + " / LV " + number(run.stats.level) + " / SCORE " + number(run.stats.score) + " / Clock paused.");
      c.wrap(26, 152, 338, v.restored_run ? "Level " + number(run.stats.level) + " / " + time_text(run.stats.active_seconds) + " saved. " + (run.phase == gameplay::Phase::choosing_upgrade ? "Your earned upgrade is waiting. " : "") + "Resume your flight or end it and bank credits." : "Fly close to capture faster. Chain 3 quick sweeps for Overdrive. Dash through danger: tap Dash or press Space. Avoid coral warnings.", 19, white, 7);
      c.button(26, 360, 338, "RESUME", Command::resume, true, mint, 56);
      c.button(26, 438, 338, "SETTINGS", Command::settings, true, white, 56);
      c.button(26, 516, 338, "CURRENT BUILD", Command::current_build, true, white, 56);
      c.button(26, 690, 338, "END RUN & BANK CREDITS", Command::finish, true, white, 56);
      break;
    case Screen::death:
    case Screen::results:
      flight_report_panel(c, v);
      break;
    case Screen::hangar: {
      c.fill(8, 8, 374, 828, navy);
      c.text(24, 28, "HANGAR", 32, white, true);
      c.icon(24, 83, 0, 28);
      c.text(64, 79, number(account.credits) + " RESEARCH CREDITS", 17, mint);
      c.wrap(24, 108, 342, "Permanent bonuses for your next flight.", 16, muted, 2, 1.F);
      const auto tracks = store.hangar();
      for (unsigned i = 0; i < 4; ++i) {
        const float y = 162 + static_cast<float>(i) * 138;
        const auto& track = tracks[i];
        c.fill(20, y, 350, 126, panel);
        c.icon(34, y + 12, i == 0 ? 3U : i == 3 ? 2U
                                                : 1U,
            30);
        c.fitted(78, y + 10, 276, track.title, 20);
        c.fitted(78, y + 32, 276, "RANK " + number(track.rank) + "/" + number(track.maximum_rank) + (track.next_bonus.empty() ? " / OWNED MAX" : " / OWNED / NEXT"), 14, mint);
        c.fitted(34, y + 52, 180, track.owned_bonus + (track.next_bonus.empty() ? "" : " -> " + track.next_bonus), 20, white);
        c.wrap(34, y + 70, 180, track.bonus_basis, 14, muted, 2, 1.F);
        c.button(226, y + 67, 130, track.rank >= track.maximum_rank ? "MAXED" : number(track.cost) + " CR", static_cast<Command>(static_cast<int>(Command::buy0) + static_cast<int>(i)), track.affordable, mint, 48);
      }
      c.button(26, 756, 148, "BACK", Command::home, true, white, 56);
      c.button(192, 756, 172, "BLUEPRINTS", Command::blueprints, true, mint, 56);
      break;
    }
    case Screen::blueprints: {
      if (v.show_recipes) {
        heading("EARNED EVOLUTIONS", "Both ingredients need rank III. Your first two recipes evolve automatically.");
        for (unsigned slot = 0; slot < 2; ++slot) {
          const unsigned i = (v.recipe_page % 3) * 2 + slot;
          const auto ingredients = gameplay::tuning::evolution_ingredients[i];
          const float y = 140 + static_cast<float>(slot) * 278;
          c.fill(20, y, 350, 264, panel);
          c.wrap(32, y + 8, 326, gameplay::tuning::evolution_names[i], 22, white, 2);
          const auto requirement = std::string(gameplay::tuning::module_names[ingredients[0]]) + " " + number(run.modules[ingredients[0]]) + "/3 + " + std::string(gameplay::tuning::module_names[ingredients[1]]) + " " + number(run.modules[ingredients[1]]) + "/3";
          c.wrap(32, y + 62, 326, requirement, 15, mint, 3);
          c.wrap(32, y + 126, 326, recipe_effects[i], 15, muted, 7);
        }
        c.button(26, 708, 156, "PREVIOUS", Command::recipe_previous, true, white, 52);
        c.button(208, 708, 156, "NEXT", Command::recipe_next, true, white, 52);
        c.button(26, 778, 148, "BACK", Command::home, true, white, 44);
        c.button(192, 778, 172, "BLUEPRINTS", Command::recipes, true, mint, 44);
      } else {
        heading("BLUEPRINT LIBRARY", "Free milestones. Unlocked hardware joins future upgrade draws automatically.");
        const auto blueprints = store.blueprints();
        for (unsigned i = 0; i < 6; ++i) {
          const auto& blueprint = blueprints[i];
          const float y = 136 + static_cast<float>(i) * 98;
          c.fill(20, y, 350, 88, panel);
          c.wrap(32, y + 5, 326, blueprint.title, 18, white, 1);
          c.wrap(32, y + 30, 326, blueprint.milestone, 14, blueprint.unlocked ? mint : gold, 1);
          c.wrap(32, y + 56, 326, "Run: " + blueprint.requirement, 14, muted, 1);
        }
        c.button(26, 756, 148, "BACK", Command::home, true, white, 56);
        c.button(192, 756, 172, "EVOLUTIONS", Command::recipes, true, mint, 56);
      }
      break;
    }
    case Screen::settings:
      heading("FLIGHT SETTINGS", "Drag to fly / WASD or arrows / controller left stick");
      c.button(26, 170, 338, account.settings.reduced_motion ? "REDUCED MOTION: ON" : "REDUCED MOTION: OFF", Command::motion, true, white, 60);
      c.button(26, 254, 338, account.settings.audio ? "SOUND EFFECTS: ON" : "SOUND EFFECTS: OFF", Command::audio, true, white, 60);
      c.button(26, 338, 338, v.haptics_available ? (account.settings.haptics ? "HAPTICS: ON" : "HAPTICS: OFF") : "HAPTICS: UNAVAILABLE", Command::haptics, v.haptics_available, white, 60);
      c.wrap(26, 438, 338, "Fly close to capture faster. Chain 3 sweeps for Overdrive. Dash: Space or tap Dash. Capture and fire are automatic. Phase Drive improves Dash.", 18, muted, 9);
      c.button(26, 628, 338, "PRIVACY POLICY", Command::privacy, true, white, 56);
      c.button(26, 744, 338, "BACK", Command::home, true, mint, 60);
      break;
    default:
      break;
    }
  }
  save_modal(c, v, false);
  return std::move(c.output);
}
Interface interface(const View& v) {
  const auto framing = v.framing ? *v.framing : gameplay_framing(v.viewport, v.safe_area, v.text_scale);
  const bool large = std::isfinite(v.text_scale) && v.text_scale > 1.F;
  const bool enlarged_tactical = framing.banded || (large && !framing.compact);
  if (v.screen == Screen::settings && v.show_privacy) return readable_interface(v);
  if (v.screen == Screen::pause && v.show_build && !v.save_error) return large ? readable_interface(v) : build_overview(v);
  const auto finish = [&](Interface result) {
    if (v.screen != Screen::run || v.save_error) return result;
    {
      Canvas feedback{v};
      feedback.scale = feedback.type_scale = 1;
      feedback.ox = feedback.oy = 0;
      const float text_size = 14 * (std::isfinite(v.text_scale) ? std::max(1.F, v.text_scale) : 1.F);
      float top = feedback.safe.y + 68, bottom = feedback.safe.bottom() - 100;
      if (result.damage_hud_bottom > 0) top = std::max(top, result.damage_hud_bottom + 12);
      for (const auto& button : result.buttons) {
        if (button.command == Command::pause) top = std::max(top, button.bounds.bottom() + 12);
        if (button.command == Command::dash) bottom = std::min(bottom, button.bounds.y - 12);
      }
      // Installation confirmation owns its existing strip under the HUD.
      if (v.promotion.installed && !enlarged_tactical) top += std::max(36.F, text_size + 16) + 12;
      // Banded layout has empty side flanks. Attached labels may use them at
      // the unchanged actor projection while clearing both control strips.
      const ui::Rect field = framing.banded ? ui::Rect{feedback.safe.x + 8, framing.stage.y + 4, feedback.safe.width - 16, framing.stage.height - 8} :
          framing.compact ? ui::Rect{framing.stage.x + 4, framing.stage.y + 4, framing.stage.width - 8, framing.stage.height - 8} :
          ui::Rect{feedback.safe.x + 8, top, feedback.safe.width - 16, std::max(0.F, bottom - top)};
      for (const auto& vitality : enemy_vitality(*v.run, framing, field, text_size)) {
        const auto& r = vitality.bounds;
        feedback.fill(r.x, r.y, r.width, r.height, navy);
        const float bar_y = vitality.boss ? r.bottom() - 9 : r.y + 2;
        const float bar_height = vitality.boss ? 6.F : 4.F;
        feedback.fill(r.x + 2, bar_y, r.width - 4, bar_height, panel);
        feedback.fill(r.x + 2, bar_y, (r.width - 4) * vitality.fraction, bar_height, vitality.boss ? coral : gold);
        if (vitality.boss) {
          const auto percent = static_cast<unsigned>(std::ceil(vitality.fraction * 100));
          if (vitality.stacked) {
            feedback.centered(r.x, r.y + 2, r.width, "BOSS", text_size, white);
            feedback.centered(r.x, r.y + text_size + 2, r.width, number(percent) + "%", text_size, white);
          } else feedback.centered(r.x, r.y + 2, r.width, "BOSS " + number(percent) + "%", text_size, white);
          // Quarter marks convey length even when coral is hard to distinguish.
          for (unsigned i = 1; i < 4; ++i)
            feedback.fill(r.x + 2 + (r.width - 4) * static_cast<float>(i) / 4, bar_y, 1, bar_height, navy);
        }
      }
      result.quads.insert(result.quads.end(), feedback.output.quads.begin(), feedback.output.quads.end());
    }
    if (!v.promotion.installed || framing.compact || enlarged_tactical) return result;
    Canvas c{v};
    c.scale = 1;
    c.ox = c.safe.x;
    c.oy = c.safe.y;
    const float width = c.safe.width - 32;
    const float font = 14;
    std::string label = "Installed: " + v.promotion.installed->title;
    if (c.measure(label, font) > width - 24) label = "Installed";
    const float text_height = font * c.type_scale;
    const float height = std::max(36.F, text_height + 16);
    float hud_bottom = c.safe.y;
    for (const auto& button : result.buttons)
      if (button.command == Command::pause) hud_bottom = button.bounds.bottom();
    const float top = hud_bottom - c.safe.y + 8;
    c.fill(16, top, width, height, navy);
    c.fill(16, top, 3, height, mint);
    c.text(28, top + 8, label, font, mint);
    // The saved selection gets one brief travelling accent while the existing
    // confirmation releases combat on its unchanged120ms schedule.
    if (!v.promotion.reduced_motion && v.promotion.installed_seconds >= 0) {
      const float t = std::clamp(v.promotion.installed_seconds / .35F, 0.F, 1.F);
      c.fill(16, top + height - 2, width * (1 - (1-t) * (1-t)), 2, mint);
    }
    result.quads.insert(result.quads.end(), c.output.quads.begin(), c.output.quads.end());
    result.page_text = "INSTALLED " + v.promotion.installed->title;
    result.text_overflow |= c.output.text_overflow;
    return result;
  };
  if (framing.compact && !framing.banded && v.screen == Screen::run && !v.save_error) return finish(compact_run(v, framing));
  if (enlarged_tactical && v.screen == Screen::run && !v.save_error) return finish(enlarged_run(v, framing));
  if (large && v.screen == Screen::blueprints && v.show_recipes && !v.save_error) {
    // One recipe must not change the navigation model when the next recipe's
    // longer description overflows. Choose consistently for this preference.
    for (unsigned page = 0; page < 3; ++page) {
      auto probe = v;
      probe.recipe_page = page;
      if (standard_interface(probe).text_overflow) return readable_interface(v);
    }
  }
  auto standard = standard_interface(v);
  if ((v.screen == Screen::death || v.screen == Screen::results || v.screen == Screen::upgrade) && standard.text_overflow) return readable_interface(v);
  if (!large || !standard.text_overflow) return finish(std::move(standard));
  return finish(v.screen == Screen::run && !v.save_error ? enlarged_run(v, framing) : readable_interface(v));
}
} // namespace human_resources::product
