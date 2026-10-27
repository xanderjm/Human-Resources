#include <algorithm>
#include <array>
#include <cmath>
#include <human_resources/gameplay/tuning.h>
#include <human_resources/product/presentation.h>
#include "thumbstick.h"
#include <human_resources/product/combat_feedback.h>
#include <product_glyphs.generated.h>
#include <sstream>
#include "privacy_policy.h"
#include "upgrade_presentation.h"
#include "flight_report.h"
#include "ui_canvas.h"
#include "enamel_screens.h"
#include "enamel_components.h"
#include "enamel_hud.h"
#include "save_protection.h"
namespace human_resources::product {
namespace {
using detail::Canvas;
using detail::Color;
using detail::navy;
using detail::panel;
using detail::mint;
using detail::white;
using detail::muted;
using detail::gold;
using detail::coral;
using detail::number;
using detail::time_text;
} // namespace

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
    const std::string message = "PROGRESS PROTECTED\n" + std::string(v.save_message.empty() ? "Saving failed. Retry before continuing." : v.save_message)+"\n"+std::string(protection_notice(v));
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
    add("CURRENT BUILD\n" + number(installed) + " / 6 hardware slots. Hardware, flight research and evolutions last for this run only. Permanent Hangar ranks apply to future runs. Level abduction: +" + metric_number(gameplay::level_capture_speed_bonus(run.stats.level) * 100) + "% of base rate. " + level_capture_reward() + ".");
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
    for (const auto i : gameplay::tuning::active_evolutions)
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
    add(pick.title + "\n" + std::string(choice_effect_label(pick)) + "\n" + choice_change(pick) + (choice_conditions(pick).empty() ? "" : "\n" + std::string(choice_conditions(pick))) + "\n" + choice_status(pick) + "\n" + level_capture_reward() + ". Flight paused.\n" + number(hardware_count(run)) + " of 6 hardware slots occupied." + (pick.evolution_preview.empty() ? "" : "\nEvolves: " + pick.evolution_preview), static_cast<Command>(static_cast<unsigned>(Command::pick0) + i), "CHOOSE " + number(i + 1));
    if (run.rerolls_remaining) add("REROLL\nReplace all three offers. " + number(run.rerolls_remaining) + " remaining.", Command::reroll, "REROLL");
    add("EVOLUTIONS\nRead all five recipes and your ingredient ranks.", Command::recipe0, "EVOLUTIONS");
    break;
  }
  case Screen::pause: {
    const auto bank = pause_bank_preview(v);
    add(std::string(v.restored_run ? "RUN SAVED\n" : "ON HOLD\n") + bank.heading + "\n" + bank.detail + "\n" + std::string(location_name(v.theme)) + " / Level " + number(run.stats.level) + " / " + time_text(run.stats.active_seconds) + ". Score " + number(run.stats.score) + ". Hull " + number(static_cast<unsigned>(std::max(0.F, run.hull))) + ", shield " + number(static_cast<unsigned>(std::max(0.F, run.shield))) + ". The run clock is paused. Stay near specimens and avoid coral shots and warning circles." + (run.phase == gameplay::Phase::choosing_upgrade ? " Your earned upgrade is waiting." : ""), Command::resume, "RESUME");
    add("CURRENT BUILD\nInspect installed hardware, temporary flight research, evolutions and permanent Hangar ranks.", Command::current_build, "CURRENT BUILD");
    add("FLIGHT SETTINGS\nThe run stays paused while you change settings.", Command::settings, "SETTINGS");
    add("END RUN\n" + bank.heading + "\n" + bank.detail, Command::finish, "END & BANK");
    break;
  }
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
      add("HANGAR\n" + number(account.credits) + " RESEARCH CREDITS\nPermanent bonuses apply to your next flight.\n" + std::string(track.title) + " / rank " + number(track.rank) + " of " + number(track.maximum_rank) + "\nOwned: " + track.owned_bonus + " " + track.bonus_basis + "." + (track.next_bonus.empty() ? " Maximum rank." : "\nNext: " + track.next_bonus + " " + track.bonus_basis + "."), static_cast<Command>(static_cast<unsigned>(Command::buy0) + i), track.rank >= track.maximum_rank ? "MAXED" : "BUY / " + number(track.cost) + " CR", track.affordable);
    }
    add("BLUEPRINT LIBRARY\nFree milestones unlock hardware in future upgrade draws.", Command::blueprints, "BLUEPRINTS");
    add("NEXT FLIGHT\nStart in " + std::string(location_name(v.theme)) + " with your purchased permanent Hangar bonuses. Temporary flight upgrades reset.", Command::start, "NEXT FLIGHT");
    break;
  }
  case Screen::blueprints:
    if (v.show_recipes) {
      for (const auto i : gameplay::tuning::active_evolutions) {
        const auto pair = gameplay::tuning::evolution_ingredients[i];
        add("EARNED EVOLUTIONS\n" + std::string(gameplay::tuning::evolution_names[i]) + "\n" + std::string(gameplay::tuning::module_names[pair[0]]) + " " + number(run.modules[pair[0]]) + "/3 + " + std::string(gameplay::tuning::module_names[pair[1]]) + " " + number(run.modules[pair[1]]) + "/3\n" + std::string(recipe_effects[i]) + "\nBoth ingredients need rank III. The first two completed recipes evolve automatically.");
      }
      add("BLUEPRINT LIBRARY\nRead free account milestones and this-run requirements.", Command::recipes, "BLUEPRINTS");
    } else {
      for (const auto& blueprint : v.store->blueprints())
        add("BLUEPRINT LIBRARY\n" + std::string(blueprint.title) + "\nAccount milestone: " + blueprint.milestone + "\nRequired in this run: " + blueprint.requirement + "\nUnlocked hardware joins future upgrade draws automatically.");
      add("EARNED EVOLUTIONS\nRead all five recipes and your ingredient ranks.", Command::recipes, "EVOLUTIONS");
    }
    break;
  case Screen::settings:
    add("FLIGHT SETTINGS\nDrag to fly / WASD or arrows / controller left stick. Capture and fire stay active while you move. Fly close for faster captures. Keep moving through targets. Three sweeps trigger Overdrive. Dash through danger: flick, press Space or tap Dash. Phase Drive improves it.");
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
std::vector<std::string> readable_lines(const Canvas& c, std::string_view value, float width, float size,
                                        enamel::Font font=enamel::Font::prose) {
  return enamel::lines(c,value,width,size,font);
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
  const auto button_height = [&](std::string_view label, float target_width = -1) {
    const auto lines = readable_lines(c, label, (target_width < 0 ? width : target_width) - 24, button_size,enamel::Font::body);
    return std::max(44.F, button_size + static_cast<float>(lines.empty() ? 0 : lines.size() - 1) * button_size * 1.25F + 12);
  };
  // Keep useful primary menu actions together. Only genuinely overflowing
  // content/items page; a small text preference does not turn menus into tours.
  std::vector<ReadableSection> primary;
  const float hangar_flight_width = width - 44 - gap;
  const float hangar_footer_height = button_height("NEXT FLIGHT", hangar_flight_width);
  const bool hangar_split_footer = v.screen == Screen::hangar && !v.save_error &&
      enamel::measure(c,"FLIGHT",button_size,enamel::Font::body) <= hangar_flight_width - 24 &&
      safe.height - content_top - hangar_footer_height - gap - 2 * (row + gap) - 12 >= heading_size + body_size;
  const float back_height = hangar_split_footer ? hangar_footer_height : row;
  if (hangar_split_footer) sections.pop_back();
  const bool primary_screen = !v.save_error && !v.show_build && !v.show_privacy && (v.screen == Screen::title || v.screen == Screen::pause || v.screen == Screen::settings || v.screen == Screen::death || v.screen == Screen::results);
  if (primary_screen) {
    float dock_height = back == Command::none ? 0.F : back_height + gap;
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
      if (v.screen == Screen::pause) text += "\nHardware and research last for this flight. Hangar ranks are permanent. End Run finishes this flight and banks earned credits once.";
      sections = {{std::move(text), Command::none, {}, true}};
    } else primary.clear();
  }
  struct Line final { std::string text; float size; enamel::Font font; };
  struct Page final { unsigned section; std::vector<Line> lines; };
  std::vector<Page> pages;
  for (unsigned i = 0; i < sections.size(); ++i) {
    const auto& section = sections[i];
    float dock_height = back == Command::none ? 0.F : back_height + gap;
    for (const auto& action : primary) dock_height += button_height(action.label) + gap;
    if (section.command != Command::none && (primary.empty() || !primary_screen)) dock_height += button_height(section.label) + gap;
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
      const auto face=heading?enamel::Font::compact:enamel::Font::prose;
      for (auto line : readable_lines(c, paragraph, width, size,face)) lines.push_back({std::move(line), size,face});
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
  enamel::panel(c,enamel::Sprite::navy_panel,{0,0,safe.width,safe.height},12);
  if (upgrade_tabs) {
    const float tab_width = (width - 2 * gap) / 3;
    for (unsigned i = 0; i < 3; ++i) {
      const float x = 16 + static_cast<float>(i) * (tab_width + gap);
      const bool selected = i == std::min(v.upgrade_preview, 2U);
      enamel::panel(c,selected?enamel::Sprite::cream_panel:enamel::Sprite::navy_panel,{x,8,tab_width,row},10);
      if (tab_width >= 88) {
        enamel::icon(c,{x+10,8+(row-26)*.5F,26,26},enamel::reward_icon(v.run->choices[i]));
        enamel::label(c,{x+42,14,tab_width-48,button_size*1.25F},number(i+1),{button_size,selected?enamel::ink:enamel::cream,enamel::Font::body,true,false,0,false});
      } else enamel::label(c,{x,14,tab_width,button_size*1.25F},number(i+1),{button_size,selected?enamel::ink:enamel::cream,enamel::Font::body,true,false,0,false});
      c.output.buttons.push_back({c.rect(x, 8, tab_width, row), static_cast<Command>(static_cast<unsigned>(Command::preview0) + i), "PREVIEW " + number(i + 1), true});
    }
  }
  if (visible_counter) enamel::label(c,{16,8,width,count_size*1.25F},number(c.output.page+1)+" / "+number(c.output.page_count),{count_size,enamel::active,enamel::Font::body,true,false,0,false});
  float y = content_top;
  for (const auto& line : page.lines) {
    enamel::label(c,{16,y,width,line.size*1.25F},line.text,{line.size,enamel::cream,line.font,false,false,0,false});
    c.output.page_text += line.text + "\n";
    y += line.size * 1.25F;
  }
  const auto button = [&](float x, float top, float w, float height, std::string label, Command command, bool enabled = true) {
    if(label=="<" || label==">") {
      const float visual_height=w==44?44.F:height;
      const float y=top+(height-visual_height)*.5F;
      enamel::action_panel(c,{x,y,w,visual_height},label,command,enamel::Sprite::secondary,enabled);
      enamel::icon(c,{x+(w-24)*.5F,y+(visual_height-24)*.5F,24,24},label=="<"?enamel::Sprite::arrow_left:enamel::Sprite::arrow_right);
      return;
    }
    enamel::action_panel(c,{x,top,w,height},label,command,enamel::Sprite::secondary,enabled);
    const auto lines = readable_lines(c, label, w - 24, button_size,enamel::Font::body);
    const float text_height = button_size + static_cast<float>(lines.empty() ? 0 : lines.size() - 1) * button_size * 1.25F;
    float baseline = top + (height - text_height) * .5F;
    for (const auto& line : lines) {
      enamel::label(c,{x+12,baseline,w-24,button_size*1.25F},line,{button_size,enabled?enamel::ink:muted,enamel::Font::body,true,false,0,false});
      baseline += button_size * 1.25F;
    }
  };
  float footer = safe.height - 12;
  if (back != Command::none) {
    footer -= back_height;
    if (hangar_split_footer) {
      button(16, footer, 44, back_height, "<", back);
      c.output.buttons.back().label = "BACK";
      button(16 + 44 + gap, footer, hangar_flight_width, back_height, "NEXT FLIGHT", Command::start);
    } else button(16, footer, width, row, "BACK", back);
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
  }
  if (section.command != Command::none && (primary.empty() || !primary_screen)) {
    const float height = button_height(section.label);
    button(16, footer - height, width, height, section.label, section.command, section.enabled);
    if (upgrade_tabs && page.section == 0 && footer - height - y >= 104) {
      const auto& pick = v.run->choices[std::min(v.upgrade_preview, 2U)];
      const float icon_top = y + (footer - height - y - 80) * .5F;
      enamel::panel(c,enamel::Sprite::blueprint_tile,{safe.width*.5F-40,icon_top,80,80},10);
      enamel::icon(c,{safe.width*.5F-30,icon_top+8,60,54},enamel::reward_icon(pick));
      if(pick.hardware)enamel::pips(c,{safe.width*.5F-23,icon_top+70,46,6},pick.next_rank,3);
    }
  }
  // Enter/accept keeps activating the visible section before the footer flight.
  if (hangar_split_footer) {
    const auto flight = std::find_if(c.output.buttons.begin(), c.output.buttons.end(), [](const auto& b) { return b.command == Command::start; });
    std::rotate(flight, flight + 1, c.output.buttons.end());
  }
  return std::move(c.output);
}
// Tactical variants keep the verified physical strips and control identities.
// Shared frames, actions and source-measured live type never change that field.
Interface enlarged_run(const View& v, const GameplayFraming& framing) {
  Canvas c(v);
  const auto safe = c.safe;
  c.scale = c.type_scale = 1;
  c.ox = safe.x;
  c.oy = safe.y;
  const float size = 14.F * std::max(1.F, v.text_scale), row = GameplayFraming::tactical_row_height;
  const auto measure=[&](std::string_view text){return enamel::measure(c,text,size);};
  const auto line = [&](float x, float y, float width, std::string_view copy, Color color, bool centered = false) {
    if (measure(copy) > width+.01F) c.output.text_overflow = true;
    enamel::tactical_label(c,{x,y,width,row},copy,size,color,centered);
  };
  const auto bar = [&](float x, float y, float width, float fraction, Color color) {
    c.fill(x, y, width, 2, panel);
    c.fill(x, y, width * std::clamp(fraction, 0.F, 1.F), 2, color);
  };
  const float pause_x = safe.width - row - 4, all_stats = pause_x - 12;
  const auto level_copy="LV "+enamel::short_number(v.run->stats.level)+" / XP";
  const auto score_copy="SCORE "+enamel::short_number(v.run->stats.score)+" / "+time_text(v.run->stats.active_seconds);
  const bool wide_stats=safe.width>=620 && measure(level_copy)+16<=(all_stats-16)*.24F &&
      measure(score_copy)+16<=(all_stats-16)*.34F;
  const float stats_width=wide_stats?(all_stats-16)*.42F:all_stats;
  enamel::panel(c,enamel::Sprite::navy_panel,{0,0,wide_stats?stats_width:safe.width,row},8);
  c.output.hud_bottom=safe.y+row;
  if(wide_stats) {
    const float level_x=stats_width+8,level_width=(all_stats-16)*.24F,score_x=level_x+level_width+8;
    enamel::panel(c,enamel::Sprite::navy_panel,{level_x,0,level_width,row},8);
    enamel::panel(c,enamel::Sprite::navy_panel,{score_x,0,all_stats-score_x,row},8);
    line(level_x+8,0,level_width-16,level_copy,enamel::active,true);
    bar(level_x+8,row-4,level_width-16,v.run->xp/std::max(1.F,v.run->next_level_xp),gold);
    line(score_x+8,0,all_stats-score_x-16,score_copy,enamel::cream,true);
  } else {
    // XP remains visible without adding a second header row.
    bar(4,0,stats_width,v.run->xp/std::max(1.F,v.run->next_level_xp),gold);
  }
  if (v.damage_feedback.age >= 0) {
    const auto cause = v.damage_feedback.cause == gameplay::DamageCause::contact ? "BUMP" :
        v.damage_feedback.cause == gameplay::DamageCause::projectile ? "SHOT" : "HIT";
    const std::string copy = std::string(v.damage_feedback.hull ? "HP:" : "SH:") + cause;
    line(4, 0, stats_width, copy, v.damage_feedback.hull ? coral : mint);
    c.fill(4, row - 3, stats_width, 3, v.damage_feedback.hull ? coral : mint);
  } else {
    const std::string hull = "HP " + number(static_cast<unsigned>(std::max(0.F, v.run->hull)));
    const std::string shield = "SH " + number(static_cast<unsigned>(std::max(0.F, v.run->shield)));
    if (measure(hull + "  " + shield) <= stats_width) {
      line(4, 0, stats_width, hull + "  " + shield, white);
    } else {
      std::string hp = hull;
      bool hull_fraction = false;
      const float shield_width = measure("SH");
      if (measure(hp) + 8 + shield_width > stats_width) {
        const auto percent = static_cast<unsigned>(std::ceil(100.F * std::clamp(v.run->hull / std::max(1.F, v.run->maximum_hull), 0.F, 1.F)));
        hp = "HP" + number(percent) + "%";
      }
      if (framing.banded && measure(hp) > stats_width - shield_width - 8) {
        const float fraction = std::clamp(v.run->hull / std::max(1.F, v.run->maximum_hull), 0.F, 1.F);
        hp = fraction >= 1 ? "HP MAX" : "HP" + number(static_cast<unsigned>(std::floor(100.F * fraction))) + "%";
      }
      if (framing.banded && measure(hp) > stats_width - shield_width - 8) {
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
  enamel::action_panel(c,{pause_x,0,row,row},"PAUSE",Command::pause,enamel::Sprite::secondary);
  enamel::icon(c,{pause_x+10,9,24,25},enamel::Sprite::pause);

  const float bottom = safe.height - row - GameplayFraming::tactical_bottom_margin, available = safe.width - 8;
  const bool detailed = measure("DASH 4s") + measure("SWEEP 2/3") + 24 <= available;
  // The new short-window fallback uses a narrow divider so both complete
  // action/status words retain the requested 3.4x type in a 320pt window.
  const float gutter = framing.banded ? 2.F : 8.F;
  const bool dash_icon = framing.banded && measure("DASH") + std::max(measure("BOOST"), measure("ADDED")) + 16 + gutter > available;
  const float dash_width = dash_icon ? std::max({44.F, measure(">>") + 8, measure("4s") + 8}) :
      std::max(44.F, measure(detailed ? "DASH 4s" : "DASH") + 8);
  const float status_width = std::min(available-dash_width-gutter,wide_stats?360.F:available),dash_x=safe.width-4-dash_width;
  const bool ready = v.run->dash_cooldown_seconds <= 0;
  const auto seconds = number(static_cast<unsigned>(std::ceil(v.run->dash_cooldown_seconds))) + "s";
  const std::string dash = ready ? (dash_icon ? ">>" : "DASH") : detailed && !dash_icon ? "DASH " + seconds : seconds;
  enamel::action_panel(c,{dash_x,bottom,dash_width,row},ready?"DASH":"DASH "+seconds,Command::dash,enamel::Sprite::primary,ready);
  line(dash_x + 4, bottom, dash_width - 8, dash, ready ? enamel::cream : muted, true);

  std::string status;
  Color status_color = white;
  float fraction = -1;
  if (v.promotion.installed) {
    status = "ADDED";
    c.output.page_text = "INSTALLED " + v.promotion.installed->title;
    status_color = mint;
  } else if (v.run->harvest_overdrive_seconds > 0) {
    status = "BOOST " + number(static_cast<unsigned>(std::ceil(v.run->harvest_overdrive_seconds))) + "s";
    if (measure(status) > status_width - 8) status = "BOOST";
    fraction = v.run->harvest_overdrive_seconds / gameplay::tuning::harvest_overdrive_seconds;
    status_color = mint;
  } else if (v.run->harvest_chain) {
    status = "SWEEP " + number(v.run->harvest_chain) + "/" + number(gameplay::tuning::harvest_chain_target);
    if (measure(status) > status_width - 8) status = number(v.run->harvest_chain) + "/" + number(gameplay::tuning::harvest_chain_target);
    fraction = v.run->harvest_chain_seconds / gameplay::tuning::harvest_chain_window_seconds;
    status_color = mint;
  } else if (v.run->stats.active_seconds < 12) {
    status = v.run->stats.active_seconds < 4 ? "DRAG TO FLY" : "KEEP MOVING";
    if (measure(status) > status_width - 8) status = v.run->stats.active_seconds < 4 ? "DRAG" : "MOVE";
  } else {
    status = "LV " + number(v.run->stats.level);
    if (measure(status) > status_width - 8) status = "LEVEL";
  }
  enamel::panel(c,enamel::Sprite::navy_panel,{4,bottom,status_width,row},8);
  line(8, bottom, status_width - 8, status, status_color, true);
  if (fraction >= 0) {
    bar(8, bottom + row - 2, status_width - 8, fraction, mint);
    if (v.run->harvest_chain && v.run->harvest_overdrive_seconds <= 0 && !v.promotion.installed) {
      const float pip = (status_width - 16) / static_cast<float>(gameplay::tuning::harvest_chain_target);
      for (unsigned i = 1; i < gameplay::tuning::harvest_chain_target; ++i)
        c.fill(8 + static_cast<float>(i) * pip, bottom + row - 2, 2, 2, navy);
    }
  }
  detail::append_thumbstick(c.output, v);
  return std::move(c.output);
}
Interface compact_run(const View& v, const GameplayFraming& framing) {
  Canvas c(v);
  c.scale = c.type_scale = 1;
  c.ox = c.oy = 0;
  const float size = 14 * (std::isfinite(v.text_scale) ? std::max(1.F, v.text_scale) : 1.F);
  const auto left = framing.left_rail, right = framing.right_rail;
  enamel::panel(c,enamel::Sprite::navy_panel,left,8);
  enamel::panel(c,enamel::Sprite::navy_panel,right,8);
  const float lx = left.x + 4, lw = left.width - 8;
  float y = left.y + 8;
  const auto lines = [&](std::string_view label, Color color) {
    for (const auto& line : readable_lines(c, label, lw, size)) {
      enamel::label(c,{lx,y,lw,size*1.15F},line,{size,color,enamel::Font::body,true,false,0,false});
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
  enamel::action_panel(c,{rx,pause_y,rw,button_height},"PAUSE",Command::pause,enamel::Sprite::secondary);
  enamel::icon(c,{rx+(rw-24)*.5F,pause_y+(button_height-25)*.5F,24,25},enamel::Sprite::pause);
  const bool ready = v.run->dash_cooldown_seconds <= 0;
  enamel::action_panel(c,{rx,dash_y,rw,button_height},ready?"DASH":"DASH "+number(static_cast<unsigned>(std::ceil(v.run->dash_cooldown_seconds)))+"s",Command::dash,enamel::Sprite::primary,ready);
  enamel::tactical_label(c,{rx,dash_y,rw,button_height},"DASH",size,ready?enamel::cream:muted,true);
  float status_y = pause_y + button_height + 16;
  const auto status = [&](std::string_view text, Color color) {
    const auto wrapped = readable_lines(c, text, rw, size);
    const float height = static_cast<float>(wrapped.size()) * size * 1.15F;
    if (status_y + height > dash_y - 8) { c.output.text_overflow = true; return; }
    for (const auto& line : wrapped) {
      enamel::label(c,{rx,status_y,rw,size*1.15F},line,{size,color,enamel::Font::body,true,false,0,false});
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
  else if (v.run->stats.active_seconds < 12) status(v.run->stats.active_seconds < 4 ? "DRAG TO FLY" : "KEEP MOVING", white);
  detail::append_thumbstick(c.output, v);
  return std::move(c.output);
}
} // namespace
Interface interface(const View& v) {
  if (auto themed = enamel_interface(v)) return std::move(*themed);
  const auto framing = v.framing ? *v.framing : gameplay_framing(v.viewport, v.safe_area, v.text_scale);
  if(v.save_error || v.screen!=Screen::run)return readable_interface(v);
  const auto finish = [&](Interface result,bool separate_receipt=false) {
    if (v.screen != Screen::run || v.save_error) return result;
    {
      Canvas feedback{v};
      feedback.scale = feedback.type_scale = 1;
      feedback.ox = feedback.oy = 0;
      const float text_size = 14 * (std::isfinite(v.text_scale) ? std::max(1.F, v.text_scale) : 1.F);
      float top = feedback.safe.y + 68, bottom = feedback.safe.bottom() - 100;
      if (result.damage_hud_bottom > 0) top = std::max(top, result.damage_hud_bottom + 12);
      if (result.hud_bottom > 0) top = std::max(top, result.hud_bottom + 12);
      for (const auto& button : result.buttons) {
        if (button.command == Command::pause) top = std::max(top, button.bounds.bottom() + 12);
        if (button.command == Command::dash) bottom = std::min(bottom, button.bounds.y - 12);
      }
      // Installation confirmation owns its existing strip under the HUD.
      if (v.promotion.installed && separate_receipt) top += std::max(36.F, text_size + 16) + 12;
      // Banded layout has empty side flanks. Attached labels may use them at
      // the unchanged actor projection while clearing both control strips.
      const ui::Rect field = framing.banded ? ui::Rect{feedback.safe.x + 8, framing.stage.y + 4, feedback.safe.width - 16, framing.stage.height - 8} :
          framing.compact ? ui::Rect{framing.stage.x + 4, framing.stage.y + 4, framing.stage.width - 8, framing.stage.height - 8} :
          ui::Rect{feedback.safe.x + 8, top, feedback.safe.width - 16, std::max(0.F, bottom - top)};
      for (const auto& vitality : enemy_vitality(*v.run, framing, field, text_size)) {
        const auto& r = vitality.bounds;
        enamel::panel(feedback,enamel::Sprite::navy_panel,r,4);
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
    if (!v.promotion.installed || !separate_receipt) return result;
    Canvas c{v};
    c.scale = 1;
    c.ox = c.safe.x;
    c.oy = c.safe.y;
    const float width = c.safe.width - 32;
    const float font = 14;
    std::string label = "Installed: " + v.promotion.installed->title;
    if (c.measure(label, font) > width - 68) label = "Installed";
    const float text_height = font * c.type_scale;
    const float height = std::max(36.F, text_height + 16);
    float hud_bottom = std::max(c.safe.y,result.hud_bottom);
    for (const auto& button : result.buttons)
      if (button.command == Command::pause) hud_bottom = std::max(hud_bottom,button.bounds.bottom());
    const float top = hud_bottom - c.safe.y + 8;
    enamel::panel(c,enamel::Sprite::cream_panel,{16,top,width,height},8);
    const auto& installed=*v.promotion.installed;
    enamel::icon(c,{22,top+4,28,28},installed.hardware?enamel::hardware_icon(installed.module):enamel::Sprite::build);
    enamel::label(c,{58,top+10,width-68,text_height+4},label,{font,enamel::ink,enamel::Font::prose});
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
  if(auto hud=enamel_hud(v,framing))return finish(std::move(*hud),true);
  if(framing.compact && !framing.banded)return finish(compact_run(v,framing));
  return finish(enlarged_run(v,framing));
}
} // namespace human_resources::product
