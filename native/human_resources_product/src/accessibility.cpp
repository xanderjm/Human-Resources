#include <human_resources/product/accessibility.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <human_resources/gameplay/tuning.h>
#include <product_glyphs.generated.h>
#include "upgrade_presentation.h"

namespace human_resources::product {
namespace {
constexpr ui::NodeId control_base = 100;
constexpr std::array<std::string_view, 9> screen_ids{
    "title", "run", "pause", "upgrade", "death", "results", "hangar", "settings", "blueprints"};
constexpr std::array<std::string_view, 9> headings{
    "Human Resources", "Flight in progress", "On hold", "Promotion", "Signal lost",
    "Research complete", "Hangar", "Flight settings", "Blueprint library"};

std::string context(const View& view, const Button& button) {
  std::string result = "human-resources/" + std::string(screen_ids[static_cast<unsigned>(view.screen)]) +
                       "/" + std::to_string(static_cast<unsigned>(button.command));
  const bool preview = button.command >= Command::preview0 && button.command <= Command::preview2;
  if ((button.command >= Command::pick0 && button.command <= Command::pick2) || preview) {
    const auto index = static_cast<unsigned>(button.command) - static_cast<unsigned>(preview ? Command::preview0 : Command::pick0);
    const auto& choice = view.run->choices[index];
    result += "/" + std::to_string(view.run->stats.level) + "/" + std::to_string(view.run->rerolls_remaining) +
              "/" + std::to_string(static_cast<unsigned>(choice.hardware ? choice.module : gameplay::Module::count)) +
              "/" + std::to_string(static_cast<unsigned>(choice.research)) + "/" + std::to_string(choice.next_rank) +
              "/" + choice.title + "/" + choice.description;
  } else if (button.command >= Command::buy0 && button.command <= Command::buy3) {
    const auto index = static_cast<unsigned>(button.command) - static_cast<unsigned>(Command::buy0);
    result += "/" + std::to_string(view.store->account().ranks[index]);
  }
  if (view.screen == Screen::blueprints)
    result += "/" + std::to_string(view.show_recipes) + "/" + std::to_string(view.recipe_page);
  if (view.screen == Screen::settings)
    result += view.show_privacy ? "/privacy" : "/controls";
  if (view.save_error)
    result += "/protected-save/" + std::string(view.save_message);
  return result;
}

void describe_control(ui::AccessibilityNode& node, const View& view, const Button& button) {
  const auto& account = view.store->account();
  switch (button.command) {
  case Command::preview0:
  case Command::preview1:
  case Command::preview2: {
    const unsigned index = static_cast<unsigned>(button.command) - static_cast<unsigned>(Command::preview0);
    node.label = "Preview " + view.run->choices[index].title;
    node.value = "Choice " + std::to_string(index + 1) + " of 3. Read this offer without choosing it. Your run is paused.";
    node.selected = index == std::min(view.upgrade_preview, 2U);
    break;
  }
  case Command::page_next:
    node.label = "Next page";
    break;
  case Command::page_previous:
    node.label = "Previous page";
    break;
  case Command::privacy:
    node.label = view.show_privacy ? "Back to settings" : "Privacy policy";
    node.value = view.show_privacy ? "Return to flight settings. Your run stays paused." : "Read offline data use, local saves, device backups and deletion.";
    break;
  case Command::current_build:
    node.label = view.show_build ? "Back to paused flight" : "Current build";
    node.value = "Installed flight hardware, temporary research, active evolutions and permanent Hangar ranks.";
    break;
  case Command::location_next:
    node.label = "Change location";
    node.value = std::string(location_name(view.theme)) + ". Activate to choose " + std::string(location_name(next_location(view.theme))) + ". All locations are available.";
    break;
  case Command::back_up_reset:
    node.label = "Back up existing save and reset";
    node.value = "The original must be copied and verified before a fresh account can be created.";
    break;
  case Command::back_up_bank:
    node.label = "Back up saved run and bank credits";
    node.value = "A verified copy of the original is preserved before ending the incompatible run and banking only its recorded checkpoint credits.";
    break;
  case Command::resume:
    if (view.restored_run) {
      node.label = "Resume saved run";
      node.value = view.run->phase == gameplay::Phase::choosing_upgrade ? "Return to your exact earned upgrade choices. The run remains paused until you choose." : "Continue the saved flight with its equipment, enemies and rewards.";
    }
    break;
  case Command::finish:
    node.label = "End run and bank credits";
    node.value = "End this flight and save its earned Research Credits exactly once.";
    break;
  case Command::pause:
    node.label = "Pause run";
    break;
  case Command::dash:
    node.label = "Dash";
    node.value = view.run->dash_cooldown_seconds <= 0 ? "Ready. Press Space, controller east button or tap Dash for a protected burst. Counterfire at targets in range." : "Recharging. " + std::to_string(static_cast<unsigned>(std::ceil(view.run->dash_cooldown_seconds))) + " seconds remaining.";
    break;
  case Command::audio:
    node.role = ui::SemanticRole::toggle;
    node.label = "Sound effects";
    node.checked = account.settings.audio;
    node.value = node.checked ? "On" : "Off";
    break;
  case Command::motion:
    node.role = ui::SemanticRole::toggle;
    node.label = "Reduced motion";
    node.checked = account.settings.reduced_motion;
    node.value = node.checked ? "On" : "Off";
    break;
  case Command::haptics:
    node.role = ui::SemanticRole::toggle;
    node.label = "Haptics";
    node.checked = view.haptics_available && account.settings.haptics;
    node.value = view.haptics_available ? (node.checked ? "On" : "Off") : "Unavailable on this device";
    break;
  case Command::pick0:
  case Command::pick1:
  case Command::pick2: {
    const auto& choice = view.run->choices[static_cast<unsigned>(button.command) - static_cast<unsigned>(Command::pick0)];
    node.label = "Choose " + choice.title;
    node.value = (choice.hardware ? "Rank " + std::to_string(choice.next_rank) + ". " : "") + choice_status(choice) + ". " + choice.description + (choice.hardware && choice.next_rank == 1 ? " Uses one of six hardware slots." : "");
    if (!choice.evolution_preview.empty())
      node.value += ". " + choice.evolution_preview;
    break;
  }
  case Command::buy0:
  case Command::buy1:
  case Command::buy2:
  case Command::buy3: {
    const auto tracks = view.store->hangar();
    const auto& track = tracks[static_cast<unsigned>(button.command) - static_cast<unsigned>(Command::buy0)];
    node.label = std::string(track.title);
    node.value = "Rank " + std::to_string(track.rank) + " of " + std::to_string(track.maximum_rank) + ". " +
                 track.benefit + ". " + (track.rank >= track.maximum_rank ? "Maximum rank" : std::to_string(track.cost) + " credits");
    break;
  }
  default:
    break;
  }
}

std::string screen_summary(const View& view) {
  if (view.save_error)
    return std::string(view.save_message);
  const auto& run = *view.run;
  const auto& account = view.store->account();
  switch (view.screen) {
  case Screen::run:
    return std::string(view.damage_feedback.age >= 0 ? view.damage_feedback.hull ? "Hull hit. " : "Shield hit. " : "") + "Hull " + std::to_string(static_cast<unsigned>(std::max(0.F, run.hull))) + " of " +
           std::to_string(static_cast<unsigned>(run.maximum_hull)) + ". Shield " +
           std::to_string(static_cast<unsigned>(std::max(0.F, run.shield))) + ". Level " + std::to_string(run.stats.level) +
           ". Score " + std::to_string(run.stats.score) + ". Captures " + std::to_string(run.stats.captures) + ". " + (run.harvest_overdrive_seconds > 0 ? "Overdrive active, " + std::to_string(static_cast<unsigned>(std::ceil(run.harvest_overdrive_seconds))) + " seconds remaining. Capture and firing are boosted. " : run.harvest_chain ? "Sweep chain " + std::to_string(run.harvest_chain) + " of " + std::to_string(gameplay::tuning::harvest_chain_target) + ". " + std::to_string(static_cast<unsigned>(std::ceil(run.harvest_chain_seconds))) + " seconds to keep the chain. " : "") + "Drag to fly. Pass close to specimens for faster captures. Chain three quick sweeps for Overdrive. Capture and fire are automatic.";
  case Screen::upgrade:
    return "Level " + std::to_string(run.stats.level) + ". Choose one upgrade. Your run is paused. " + std::to_string(hardware_count(run)) + " of six hardware slots installed. New hardware uses a slot; ranking up improves installed hardware.";
  case Screen::pause:
    if (view.restored_run)
      return std::string(location_name(view.theme)) + ". Saved run at level " + std::to_string(run.stats.level) + ". " + (run.phase == gameplay::Phase::choosing_upgrade ? "An earned upgrade is waiting. " : "") + "Choose Resume or End run and bank credits. The run clock is paused.";
    return std::string(location_name(view.theme)) + ". Your run clock is paused.";
  case Screen::death:
    return "Score " + std::to_string(run.stats.score) + ". Captures " + std::to_string(run.stats.captures) + ". Bank credits to continue.";
  case Screen::results:
    if (view.pending_results)
      return "Saving " + std::to_string(progression::base_credits(*view.pending_results)) + " credits from score " + std::to_string(view.pending_results->score) + ". Keep the game open and retry saving.";
    return view.guest ? "Guest run. Saving is unavailable." : "Earned " + std::to_string(account.results.base_credits) + " Research Credits. Bank " + std::to_string(account.credits) + ". Best score " + std::to_string(account.best_score) + ".";
  case Screen::hangar:
    return std::to_string(account.credits) + " credits. Permanent research applies to future runs.";
  case Screen::blueprints: {
    std::string summary;
    if (view.show_recipes) {
      summary = "Both ingredients require rank three. The first two completed recipes evolve automatically. ";
      for (unsigned slot = 0; slot < 2; ++slot) {
        const unsigned i = (view.recipe_page % 3) * 2 + slot;
        const auto ingredients = gameplay::tuning::evolution_ingredients[i];
        summary += std::string(gameplay::tuning::evolution_names[i]) + ". " + std::string(gameplay::tuning::module_names[ingredients[0]]) + " and " + std::string(gameplay::tuning::module_names[ingredients[1]]) + ". " + std::string(recipe_effects[i]) + ". ";
      }
    } else
      for (const auto& blueprint : view.store->blueprints())
        summary += std::string(blueprint.title) + ". Account milestone: " + blueprint.milestone +
                   ". Required in this run before it can appear: " + blueprint.requirement + ". ";
    return summary;
  }
  case Screen::settings:
    return "Drag to move. Keyboard WASD or arrow keys. Controller left stick. Capture and fire are automatic.";
  case Screen::title:
    return std::string(location_name(view.theme)) + " selected. Fly the saucer. Gather specimens. Outrun the resistance.";
  }
  return {};
}
} // namespace

std::vector<ui::AccessibilityNode> accessibility_snapshot(const View& view,
    const Interface& interface, const Command focused) {
  if (!view.run || !view.store || !view.viewport.finite() || view.viewport.width <= 0 || view.viewport.height <= 0)
    return {};
  const auto safe = ui::inset(view.viewport, view.safe_area);
  if (safe.width <= 0 || safe.height <= 0)
    return {};
  const auto key = std::string(screen_ids[static_cast<unsigned>(view.screen)]);
  std::vector<ui::AccessibilityNode> nodes;
  nodes.reserve(interface.buttons.size() + 3);
  nodes.push_back({.id = 1, .stable_id = "human-resources/" + key, .role = ui::SemanticRole::group, .frame = safe});
  auto heading = std::string(headings[static_cast<unsigned>(view.screen)]);
  if (view.screen == Screen::results && view.store->account().results.recovered)
    heading = "Recovered run";
  if (view.screen == Screen::pause && view.restored_run)
    heading = "Saved run";
  if (view.screen == Screen::pause && view.show_build)
    heading = "Current build";
  if (view.screen == Screen::settings && view.show_privacy)
    heading = "Privacy policy";
  if (view.save_error)
    heading = "Progress protected";
  const std::string page_context = view.screen == Screen::upgrade && !view.save_error ? "Level " + std::to_string(view.run->stats.level) + ". Your run is paused. Choice " + std::to_string(std::min(view.upgrade_preview, 2U) + 1) + " of 3. " : "";
  const auto spoken_context = view.screen == Screen::run && view.promotion.installed && !view.save_error
      ? screen_summary(view) + " Installed: " + view.promotion.installed->title + "."
      : interface.page_text.empty() ? screen_summary(view) : page_context + "Page " + std::to_string(interface.page + 1) + " of " + std::to_string(interface.page_count) + ". " + interface.page_text;
  nodes.push_back({.id = 2, .stable_id = "human-resources/" + key + "/heading", .parent_id = 1, .role = ui::SemanticRole::label, .label = std::move(heading), .value = spoken_context, .frame = {safe.x, safe.y, safe.width, std::min(80.F, safe.height)}});
  for (const auto& button : interface.buttons) {
    ui::AccessibilityNode node{.id = control_base + static_cast<ui::NodeId>(button.command),
        .stable_id = context(view, button),
        .parent_id = 1,
        .role = ui::SemanticRole::button,
        .label = button.label,
        .frame = button.bounds,
        .focusable = true,
        .disabled = !button.enabled,
        .selected = button.enabled && button.command == focused};
    if (view.screen == Screen::upgrade) node.stable_id += "/preview/" + std::to_string(std::min(view.upgrade_preview, 2U));
    if (interface.page_count > 1)
      node.stable_id += "/page/" + std::to_string(interface.page) + "/" + interface.page_text;
    describe_control(node, view, button);
    nodes.push_back(std::move(node));
  }
  if (view.save_error)
    nodes.push_back({.id = 3, .stable_id = "human-resources/save-error", .parent_id = 1, .role = ui::SemanticRole::label, .label = "Progress protected", .value = std::string(view.save_message), .frame = {safe.x, safe.y + std::min(80.F, safe.height), safe.width, std::min(44.F, safe.height)}});
  return nodes;
}

Command accessibility_command(const accessibility::Action& action, const Interface& interface) {
  if (action.kind != accessibility::ActionKind::activate)
    return Command::none;
  for (const auto& button : interface.buttons)
    if (button.enabled && action.node_id == control_base + static_cast<ui::NodeId>(button.command))
      return button.command;
  return Command::none;
}
} // namespace human_resources::product
