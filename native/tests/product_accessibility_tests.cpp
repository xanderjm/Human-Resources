#include "progression_test_backend.h"
#include "test_support.h"
#include <human_resources/product/accessibility.h>

#include <algorithm>
#include <array>
#include <iostream>
#include <cctype>
#include "../human_resources_product/src/privacy_policy.h"

namespace {
namespace hr = human_resources;
using namespace hr::product;
using test_support::require;

struct Fixture final {
  progression_tests::MemoryBackend backend;
  hr::progression::AccountStore store{backend};
  hr::gameplay::Game game;
  Fixture() {
    require(store.load().playable(), "in-memory save loads");
    game.start();
  }
  View view(Screen screen = Screen::title) {
    return {screen, &game.snapshot(), &store, {0, 0, 844, 390}, {}};
  }
};
const hr::ui::AccessibilityNode& named(const std::vector<hr::ui::AccessibilityNode>& nodes, std::string_view label) {
  const auto found = std::find_if(nodes.begin(), nodes.end(), [&](const auto& node) { return node.label == label; });
  require(found != nodes.end(), "expected descriptive semantic label");
  return *found;
}
void actual_controls_and_settings_semantics() {
  Fixture f;
  auto view = f.view(Screen::title);
  auto interface = hr::product::interface(view);
  auto nodes = accessibility_snapshot(view, interface, Command::start);
  hr::accessibility::Session session;
  require(session.publish(nodes), "product snapshot has unique durable identities");
  const auto start = named(nodes, "START RUN");
  require(start.selected && start.focusable && !start.disabled, "keyboard selection exposed on real enabled control");
  require(start.frame.x == interface.buttons[0].bounds.x && start.frame.width == interface.buttons[0].bounds.width,
      "native target copies actual product hit bounds");
  require(session.request(start.stable_id, hr::accessibility::ActionKind::activate), "native press queues semantic action");
  const auto actions = session.drain_actions();
  require(actions.size() == 1 && accessibility_command(actions[0], interface) == Command::start, "validated native action resolves real start command");
  view.screen = Screen::settings;
  interface = hr::product::interface(view);
  nodes = accessibility_snapshot(view, interface);
  const auto& sound = named(nodes, "Sound effects");
  const auto& motion = named(nodes, "Reduced motion");
  require(sound.role == hr::ui::SemanticRole::toggle && sound.checked && sound.value == "On", "sound setting exposes checked state");
  require(motion.role == hr::ui::SemanticRole::toggle && !motion.checked, "reduced motion exposes actual saved state");
  const auto& unavailable_haptics = named(nodes, "Haptics");
  require(unavailable_haptics.disabled && !unavailable_haptics.checked && unavailable_haptics.value == "Unavailable on this device",
      "unsupported haptics does not pretend enabled feedback exists");
  view.haptics_available = true;
  const auto supported_nodes = accessibility_snapshot(view, hr::product::interface(view));
  require(!named(supported_nodes, "Haptics").disabled && named(supported_nodes, "Haptics").checked,
      "supported haptics exposes the saved enabled preference");
  require(session.publish(nodes), "settings snapshot valid");
  require(!session.request(start.stable_id, hr::accessibility::ActionKind::activate), "departed title element cannot start run");
  require(accessibility_command(actions[0], interface) == Command::none, "current interface rejects old start command");
  view.screen = Screen::run;
  nodes = accessibility_snapshot(view, hr::product::interface(view));
  require(named(nodes, "Pause run").role == hr::ui::SemanticRole::button, "icon-only pause gets meaningful spoken name");
}
void replaced_offer_and_purchase_reject_stale_actions() {
  Fixture f;
  auto run = f.game.snapshot();
  run.stats.level = 2;
  run.rerolls_remaining = 2;
  run.choices[0].hardware = true;
  run.choices[0].next_rank = 2;
  run.choices[0].title = "Laser Array";
  run.choices[0].description = "Install another automatic laser.";
  auto view = f.view(Screen::upgrade);
  view.run = &run;
  auto interface = hr::product::interface(view);
  auto nodes = accessibility_snapshot(view, interface);
  const auto old = named(nodes, "Choose Laser Array");
  require(old.value.find("automatic laser") != std::string::npos, "upgrade effect is spoken with choice name");
  require(old.value.starts_with("Rank 2."), "hardware rank remains spoken after removing the duplicate title suffix");
  hr::accessibility::Session session;
  require(session.publish(nodes), "first upgrade snapshot publishes");
  require(session.request(old.stable_id, hr::accessibility::ActionKind::activate), "old offer initially valid");
  --run.rerolls_remaining;
  run.choices[0].title = "Shield Generator";
  require(session.publish(accessibility_snapshot(view, hr::product::interface(view))), "rerolled offer snapshot publishes");
  require(session.drain_actions().empty(), "queued old upgrade cannot select replacement at same button index");
  view = f.view(Screen::hangar);
  interface = hr::product::interface(view);
  nodes = accessibility_snapshot(view, interface);
  require(session.publish(nodes), "hangar snapshot publishes");
  for (const auto& node : nodes)
    if (node.role == hr::ui::SemanticRole::button && node.disabled) {
      require(!node.value.empty(), "disabled purchase still describes rank benefit and price");
      require(!session.request(node.stable_id, hr::accessibility::ActionKind::activate), "unaffordable purchase rejects native activation");
    }
  require(accessibility_command({9999, hr::accessibility::ActionKind::activate}, interface) == Command::none,
      "unknown native ID does nothing");
  require(accessibility_command({100 + static_cast<hr::ui::NodeId>(Command::home), hr::accessibility::ActionKind::increment}, interface) == Command::none,
      "buttons reject adjustment actions");
}
void blueprint_semantics_name_account_milestones_and_run_requirements() {
  Fixture f;
  auto view = f.view(Screen::blueprints);
  const auto nodes = accessibility_snapshot(view, hr::product::interface(view));
  const auto& summary = named(nodes, "Blueprint library").value;
  require(summary.find("Heavy Winch. Account milestone: 0 / 300 captures") != std::string::npos,
      "library speaks readable blueprint title and account unlock unit");
  require(summary.find("Required in this run before it can appear: Beam Splitter") != std::string::npos,
      "library distinguishes contextual draw requirements from account unlocks");
  require(summary.find("800 base credits") != std::string::npos && summary.find("60 military defeats") != std::string::npos &&
      summary.find("1800 active seconds") != std::string::npos,
      "library speaks all account milestone units");
}
void page_changes_invalidate_queued_actions_without_reindexing_offers() {
  Fixture f;
  auto run = f.game.snapshot();
  for (unsigned i = 0; i < 3; ++i) {
    run.choices[i].title = "Offer " + std::to_string(i + 1);
    run.choices[i].description = "Jeep capture hull threshold: 60 -> 75%. Uses 2 slots; 8s vehicle cooldown. Tanks <=35% hull. Vehicle kills: +40% bio speed, 5s.";
  }
  auto view = f.view(Screen::upgrade);
  view.run = &run;
  view.viewport = {0,0,320,568};
  view.text_scale = 2;
  auto page = hr::product::interface(view);
  hr::accessibility::Session session;
  auto nodes = accessibility_snapshot(view, page);
  require(session.publish(nodes), "first larger-text page publishes");
  const auto old_pick = named(nodes, "Choose Offer 1");
  require(session.request(old_pick.stable_id, hr::accessibility::ActionKind::activate), "visible first choice can queue");
  const auto preview = named(nodes, "Preview Offer 2");
  require(session.request(preview.stable_id, hr::accessibility::ActionKind::activate), "direct second preview is available without traversing first-offer details");
  const auto navigation = session.drain_actions();
  require(std::any_of(navigation.begin(), navigation.end(), [&](const auto& action) { return accessibility_command(action,page) == Command::preview1; }), "preview command is distinct from choosing actual offer two");
  require(session.request(old_pick.stable_id, hr::accessibility::ActionKind::activate), "old selected choice can queue before preview changes");
  view.upgrade_preview = 1;
  view.ui_page = 0;
  page = hr::product::interface(view);
  nodes = accessibility_snapshot(view, page);
  require(session.publish(nodes), "second choice page publishes");
  require(session.drain_actions().empty(), "queued first-choice action cannot activate a different visible page");
  const auto pick = named(nodes, "Choose Offer 2");
  require(session.request(pick.stable_id, hr::accessibility::ActionKind::activate), "actual second choice queues");
  const auto actions = session.drain_actions();
  require(actions.size() == 1 && accessibility_command(actions[0], page) == Command::pick1, "paged second offer still selects actual index one");
  require(session.request(pick.stable_id, hr::accessibility::ActionKind::activate), "offer action can queue before preference refresh");
  view.text_scale = 1;
  view.ui_page = 0;
  const auto default_page = hr::product::interface(view);
  require(default_page.page_count == 1, "restoring default text returns exact default three-card layout");
  require(session.publish(accessibility_snapshot(view, default_page)), "default preference snapshot publishes");
  require(session.drain_actions().empty(), "preference/layout refresh drops queued old native identity");
}

// The measured reader can break a long word between glyphs. Compare every
// non-whitespace character so those legitimate line breaks cannot hide loss.
std::string words(std::string_view text) {
  std::string result;
  for (const char source : text) {
    const auto ch = static_cast<unsigned char>(source);
    if (!std::isspace(ch)) result += static_cast<char>(ch);
  }
  return result;
}
void offline_privacy_is_reachable_complete_and_modal() {
  Fixture f;
  f.game.pause();
  const auto saved = f.game.save_state();
  auto view = f.view(Screen::settings);
  view.viewport = {0,0,390,844};
  auto controls = hr::product::interface(view);
  auto nodes = accessibility_snapshot(view, controls);
  const auto entry = named(nodes, "Privacy policy");
  hr::accessibility::Session session;
  require(session.publish(nodes) && session.request(entry.stable_id, hr::accessibility::ActionKind::activate), "actual Settings policy entry accepts native activation");
  const auto actions = session.drain_actions();
  require(actions.size() == 1 && accessibility_command(actions[0], controls) == Command::privacy, "policy entry uses its distinct appended command");
  require(session.request(entry.stable_id, hr::accessibility::ActionKind::activate), "settings action can queue before policy opens");
  view.show_privacy = true;
  auto page = hr::product::interface(view);
  nodes = accessibility_snapshot(view, page);
  require(session.publish(nodes) && session.drain_actions().empty(), "opening policy invalidates old Settings command identity");
  require(named(nodes, "Privacy policy").value.find("Human Resources") != std::string::npos, "policy heading speaks actual visible copy");
  const auto back = named(nodes, "Back to settings");
  require(accessibility_command({back.id, hr::accessibility::ActionKind::activate}, page) == Command::privacy, "policy Back returns to Settings using the toggle, not Home");
  for (const auto size : {hr::ui::Point{320,568},hr::ui::Point{390,844},hr::ui::Point{844,390}})
    for (const float scale : {1.F,2.F,3.2F}) {
      view.viewport = {0,0,size.x,size.y}; view.text_scale = scale; view.ui_page = 0;
      const auto count = hr::product::interface(view).page_count;
      std::string complete;
      for (unsigned i = 0; i < count; ++i) {
        view.ui_page = i; page = hr::product::interface(view);
        require(!page.text_overflow, "policy pages fit actual measured text preference");
        complete += page.page_text + " ";
        nodes = accessibility_snapshot(view,page);
        require(session.publish(nodes), "every policy page has valid native semantics");
        require(named(nodes, "Privacy policy").value.find(page.page_text) != std::string::npos, "native reader receives complete visible policy page");
        require(std::all_of(page.buttons.begin(),page.buttons.end(),[](const auto& b) { return b.command == Command::privacy || b.command == Command::page_next || b.command == Command::page_previous; }), "policy has only paging and Back, no hidden settings mutations");
      }
      for (const auto section : privacy_policy_sections) {
        if (words(complete).find(words(section)) == std::string::npos) std::cerr << "Policy copy mismatch " << size.x << "x" << size.y << " scale=" << scale << "\nExpected " << words(section) << "\nActual " << words(complete) << "\n";
        require(words(complete).find(words(section)) != std::string::npos, "every complete policy section survives pagination without dropped suffixes");
      }
    }
  view.save_error = true; view.save_message = "Existing save remains protected.";
  page = hr::product::interface(view);
  nodes = accessibility_snapshot(view,page);
  require(named(nodes,"Progress protected").value.find("protected") != std::string::npos, "protected save takes precedence over policy reader");
  require(std::none_of(page.buttons.begin(),page.buttons.end(),[](const auto& b) { return b.command == Command::privacy; }), "policy cannot dismiss protected-save modal");
  require(saved == f.game.save_state(), "reading policy cannot change paused Game state");
}

bool overlaps(hr::ui::Rect a, hr::ui::Rect b) {
  const auto shared = hr::ui::intersect(a, b);
  return shared.width > .01F && shared.height > .01F;
}
void phone_layout_hit_regions() {
  Fixture f;
  struct Phone final {
    float width, height;
    hr::ui::Insets insets;
  };
  constexpr std::array<Phone, 8> phones{{{375, 667, {0, 20, 0, 0}}, {390, 844, {0, 47, 0, 34}},
      {393, 852, {0, 59, 0, 34}}, {640, 360, {}}, {667, 375, {}}, {844, 390, {}},
      {844, 390, {47, 0, 47, 21}}, {932, 430, {59, 0, 59, 21}}}};
  unsigned screens = 0;
  for (const auto& phone : phones)
    for (unsigned screen = 0; screen < 9; ++screen) {
      auto view = f.view(static_cast<Screen>(screen));
      view.viewport = {0, 0, phone.width, phone.height};
      view.safe_area = phone.insets;
      const auto safe = hr::ui::inset(view.viewport, view.safe_area);
      const auto interface = hr::product::interface(view);
      const auto nodes = accessibility_snapshot(view, interface);
      hr::accessibility::Session session;
      require(session.publish(nodes), "all phone screen snapshots publish");
      for (std::size_t i = 0; i < interface.buttons.size(); ++i) {
        const auto& a = interface.buttons[i].bounds;
        if (a.x < safe.x || a.y < safe.y || a.right() > safe.right() + .01F || a.bottom() > safe.bottom() + .01F)
          std::cerr << "Outside safe area: " << phone.width << 'x' << phone.height << " screen=" << screen << " control=" << interface.buttons[i].label << " bounds=" << a.x << ',' << a.y << ',' << a.width << ',' << a.height << '\n';
        require(a.width >= 44 && a.height >= 44, "phone controls retain forty-four point hit targets");
        require(a.x >= safe.x && a.y >= safe.y && a.right() <= safe.right() + .01F && a.bottom() <= safe.bottom() + .01F,
            "controls remain inside actual phone safe area");
        for (std::size_t j = i + 1; j < interface.buttons.size(); ++j)
          require(!overlaps(a, interface.buttons[j].bounds), "independent controls cannot have overlapping hit targets");
      }
      ++screens;
    }
  std::cout << "Product accessibility and safe-area targets verified across " << screens << " phone screens\n";
}
} // namespace
int main() {
  actual_controls_and_settings_semantics();
  replaced_offer_and_purchase_reject_stale_actions();
  blueprint_semantics_name_account_milestones_and_run_requirements();
  page_changes_invalidate_queued_actions_without_reindexing_offers();
  offline_privacy_is_reachable_complete_and_modal();
  phone_layout_hit_regions();
  return 0;
}
