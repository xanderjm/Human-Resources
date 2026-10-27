#include "progression_test_backend.h"
#include "test_support.h"
#include <human_resources/product/accessibility.h>
#include <human_resources/progression/account_session.h>

#include <algorithm>
#include <array>
namespace {
namespace hr = human_resources;
using namespace hr::product;
using test_support::require;
hr::progression::RunReceipt receipt(const hr::gameplay::Game& game, std::uint64_t id) {
  const auto& s = game.snapshot().stats;
  return {id, static_cast<std::uint64_t>(s.active_seconds * 1000), s.score, s.captures, s.soldier_kills, s.jeep_kills, s.tank_kills, s.vehicle_captures, s.boss_kills};
}
const hr::ui::AccessibilityNode& named(const std::vector<hr::ui::AccessibilityNode>& nodes, std::string_view label) {
  const auto found = std::find_if(nodes.begin(), nodes.end(), [&](const auto& node) { return node.label == label; });
  require(found != nodes.end(), "expected recovery control has an accessible label");
  return *found;
}
bool command(const Interface& ui, Command value) {
  return std::any_of(ui.buttons.begin(), ui.buttons.end(), [&](const auto& b) { return b.command == value && b.enabled; });
}
void earned_choice_recovery_preserves_location_and_offer() {
  progression_tests::MemoryBackend backend;
  hr::progression::AccountSession session(backend);
  require(session.load().playable(), "fresh session loads");
  hr::gameplay::Game game;
  game.start({.seed = 514});
  require(session.begin_run(hr::progression::continuation_for(game, 2)).succeeded(), "actual initial Game is atomically committed with Desert");
  for (unsigned frame = 0; frame < 1800 && game.snapshot().phase == hr::gameplay::Phase::active; ++frame)
    game.advance(1.F / 60);
  require(game.snapshot().phase == hr::gameplay::Phase::choosing_upgrade, "real earned choice reached");
  const auto original = game.snapshot();
  require(session.checkpoint(receipt(game, session.account().checkpoint.run_id), hr::progression::continuation_for(game, 2), true).succeeded(), "exact earned choice checkpoint commits");
  hr::progression::AccountSession reopened(backend);
  require(reopened.load().issue == hr::progression::LoadIssue::resumable_run, "cold load offers a resumable run");
  hr::gameplay::Game restored;
  require(restored.restore_state(reopened.account().continuation.game_state) == hr::gameplay::SaveStateIssue::none, "actual saved Game restores");
  restored.pause();
  require(restored.snapshot().phase == hr::gameplay::Phase::choosing_upgrade, "pause cannot erase an earned choice");
  for (const auto viewport : std::array<hr::ui::Rect, 3>{{{0, 0, 320, 568}, {0, 0, 390, 844}, {0, 0, 430, 932}}}) {
    View view{Screen::pause, &restored.snapshot(), &reopened.store(), viewport, {}};
    view.restored_run = true;
    view.theme = static_cast<hr::presentation::SceneryTheme>(reopened.account().continuation.location);
    const auto ui = interface(view);
    require(command(ui, Command::resume) && command(ui, Command::finish), "saved run offers Resume or explicit End and Bank");
    require(!command(ui, Command::start) && !command(ui, Command::revive), "recovery never starts a fresh run or revives one");
    const auto nodes = accessibility_snapshot(view, ui);
    require(named(nodes, "Saved run").value.find("Desert") != std::string::npos, "recovery speaks saved location");
    require(named(nodes, "Resume saved run").value.find("earned upgrade") != std::string::npos, "recovery explains exact waiting choice");
    view.screen = Screen::upgrade;
    const auto cards = accessibility_snapshot(view, interface(view));
    for (std::size_t i = 0; i < 3; ++i) {
      require(restored.snapshot().choices[i].title == original.choices[i].title, "saved offer identity retained");
      require(named(cards, "Choose " + original.choices[i].title).value.find(original.choices[i].description) != std::string::npos, "restored card retains its actual effect");
    }
  }
}
void protected_recovery_is_explicit_and_touch_safe() {
  progression_tests::MemoryBackend backend;
  hr::progression::AccountStore store(backend);
  require(store.load().playable(), "fixture store loads");
  hr::gameplay::Game game;
  game.start();
  for (const auto viewport : std::array<hr::ui::Rect, 3>{{{0, 0, 320, 568}, {0, 0, 390, 844}, {0, 0, 430, 932}}}) {
    View view{Screen::title, &game.snapshot(), &store, viewport, {}};
    view.save_error = true;
    view.can_bank_checkpoint = true;
    view.save_message = "This run cannot resume on this build. Preserve a verified backup and bank its saved credits.";
    const auto ui = interface(view);
    require(ui.buttons.size() == 2 && command(ui, Command::retry_save) && command(ui, Command::back_up_bank), "incompatible run has Retry and explicit preserved-backup banking only");
    require(!command(ui, Command::back_up_reset) && !command(ui, Command::start), "incompatible continuation cannot silently reset account or start fresh");
    for (std::size_t i = 0; i < ui.buttons.size(); ++i) {
      const auto b = ui.buttons[i].bounds;
      require(b.height >= 44 && b.width >= 44, "recovery action remains touch sized");
      require(b.x >= 0 && b.y >= 0 && b.right() <= viewport.width + .01F && b.bottom() <= viewport.height + .01F, "recovery actions fit actual phone bounds");
      for (std::size_t j = i + 1; j < ui.buttons.size(); ++j) {
        const auto overlap = hr::ui::intersect(b, ui.buttons[j].bounds);
        require(overlap.width < .01F || overlap.height < .01F, "backup and retry actions never overlap");
      }
    }
    const auto nodes = accessibility_snapshot(view, ui);
    require(named(nodes, "Back up saved run and bank credits").value.find("verified") != std::string::npos, "spoken action explains preservation before settlement");
    view.can_bank_checkpoint = false;
    require(interface(view).buttons.size() == 1, "ordinary failed exact checkpoint permits Retry only");
    view.can_reset_save = true;
    const auto outer = interface(view);
    require(command(outer, Command::back_up_reset) && !command(outer, Command::back_up_bank), "outer corrupt-account reset remains distinct from inner-run recovery");
  }
}
} // namespace
int main() {
  earned_choice_recovery_preserves_location_and_offer();
  protected_recovery_is_explicit_and_touch_safe();
}
