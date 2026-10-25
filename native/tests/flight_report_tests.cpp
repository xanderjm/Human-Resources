#include "progression_test_backend.h"
#include "test_support.h"
#include "../human_resources_product/src/flight_report.h"
#include <human_resources/product/accessibility.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <human_resources/gameplay/tuning.h>

namespace {
namespace hr = human_resources;
using namespace hr::product;
using test_support::require;
struct Fixture final {
  progression_tests::MemoryBackend backend;
  hr::progression::AccountStore store{backend};
  hr::gameplay::Game game;
  hr::progression::RunReceipt receipt;
  Fixture() {
    require(store.load().playable() && store.begin_run().succeeded(), "real account begins flight");
    game.start();
    receipt = {.run_id=store.account().checkpoint.run_id, .active_milliseconds=75000, .score=85, .captures=23, .jeep_kills=4};
  }
  View view(Screen screen) {
    View v{screen, &game.snapshot(), &store, {0,0,320,568}, {0,20,0,0}};
    v.current_receipt = &receipt;
    return v;
  }
};
void pause_previews_current_receipt_without_mutation() {
  Fixture f;
  auto view = f.view(Screen::pause);
  const auto writes = f.backend.writes;
  const auto minimum_ms = hr::gameplay::tuning::credit_minimum_seconds * 1000;
  f.receipt.active_milliseconds = minimum_ms - 1;
  auto bank = pause_bank_preview(view);
  require(bank.credits == 0 && bank.heading == "BANK NOW +0 CR" && bank.detail.starts_with("Resume to reach "), "pause explains minimum before ending a short flight");
  f.receipt.active_milliseconds = minimum_ms;
  f.receipt.boss_kills = 1;
  bank = pause_bank_preview(view);
  require(bank.credits == hr::progression::base_credits(f.receipt) && bank.credits > 0 && bank.detail.find("Resume to reach") == std::string::npos, "exact minimum uses canonical current vehicle and boss rewards");
  require(bank.heading == "BANK NOW +" + std::to_string(bank.credits) + " CR", "pause names the exact bankable receipt amount");
  view.restored_run = true;
  require(pause_bank_preview(view).heading == bank.heading, "restored flight uses the same exact receipt projection");
  view.guest = true;
  bank = pause_bank_preview(view);
  require(bank.credits == 0 && bank.detail.find("Saving unavailable") != std::string::npos, "guest cannot promise saved rewards");
  view.guest = false; view.current_receipt = nullptr;
  require(pause_bank_preview(view).heading == "BANK PREVIEW UNAVAILABLE", "missing receipt cannot reuse old results or claim a zero reward");
  require(f.backend.writes == writes && f.store.account().credits == 0, "all pause previews leave bank and storage untouched");
}
void actual_preview_settlement_and_no_mutation() {
  Fixture f;
  auto view = f.view(Screen::death);
  const auto writes = f.backend.writes;
  auto report = flight_report(view);
  require(report.preview && report.credits == hr::progression::base_credits(f.receipt) && report.credits == 34, "preview uses actual current receipt and reward owner");
  require(report.score == 85 && report.captures == 23 && report.seconds == 75, "report uses actual receipt rather than unrelated presentation snapshot");
  require(report.reward_label == "CREDITS TO BANK" && f.store.account().credits == 0, "preview does not claim banked rewards");
  require(f.backend.writes == writes, "preview performs no write");
  f.receipt.active_milliseconds = 29999;
  report = flight_report(view);
  require(report.credits == 0 && report.status.find("under 30 seconds") != std::string::npos, "zero reward explains existing minimum");
  f.receipt.active_milliseconds = 75000;
  require(f.store.settle(f.receipt).succeeded(), "real settlement");
  view.screen = Screen::results;
  report = flight_report(view);
  require(report.reward_label == "CREDITS SAVED" && report.bank == 34 && report.credits == 34, "saved amount and current wallet stay distinct");
  require(!report.blueprint_name.empty() && !report.hangar_name.empty() && report.hangar_detail.find("34 / 80") != std::string::npos, "real account goals connect earned flight to progression");
  require(f.store.credit_bonus(f.receipt.run_id, hr::progression::PlacementOutcome::completed).succeeded(), "existing optional placement test double records bonus");
  report = flight_report(view);
  require(report.credits == 51 && report.bank == 51, "historical recorded bonus is included without re-evaluating reward formula");
  view.guest = true;
  report = flight_report(view);
  require(report.credits == 0 && report.reward_label == "GUEST FLIGHT" && report.hangar_name.empty(), "guest cannot claim rewards or funded progression");
  view.guest = false;
  view.pending_results = &f.receipt;
  report = flight_report(view);
  require(report.pending && report.reward_label == "CREDITS TO SAVE" && report.hangar_name.empty(), "pending receipt cannot advertise settled progression");
}
void unlocks_caps_and_recovery_are_real() {
  Fixture f;
  f.receipt = {.run_id=f.receipt.run_id, .active_milliseconds=3600000, .score=1500, .captures=710, .soldier_kills=60, .jeep_kills=4, .tank_kills=3, .vehicle_captures=2, .boss_kills=2};
  require(f.store.settle(f.receipt).succeeded(), "earned large receipt unlocks real catalogue");
  auto view = f.view(Screen::results);
  auto report = flight_report(view);
  require(report.blueprint_heading == "BLUEPRINTS UNLOCKED" && report.blueprint_name.find("more") != std::string::npos && report.summary.find("Run requirements:") != std::string::npos, "actual new unlocks are named and all requirements remain available");
  require(report.hangar_action == "UPGRADE IN HANGAR", "affordable actual track gives contextual navigation");
  for (unsigned run=0; run<9; ++run) {
    require(f.store.begin_run().succeeded(), "fund complete hangar through real runs");
    f.receipt.run_id = f.store.account().checkpoint.run_id;
    require(f.store.settle(f.receipt).succeeded(), "bank real run");
  }
  for (unsigned track=0; track<4; ++track)
    while (f.store.hangar()[track].rank < f.store.hangar()[track].maximum_rank)
      require(f.store.purchase(static_cast<hr::progression::Track>(track)).succeeded(), "real earned purchase");
  report = flight_report(view);
  require(report.blueprint_name == "All blueprints unlocked" && report.hangar_name == "Hangar complete" && report.hangar_action == "HANGAR", "complete account has no fake next purchase/unlock");
  require(interface(view).page_count == 1, "ordinary fully capped account retains compact report hierarchy on320");
  require(f.store.begin_run().succeeded(), "begin interrupted flight");
  f.receipt.run_id = f.store.account().checkpoint.run_id;
  require(f.store.checkpoint(f.receipt, true).succeeded(), "actual interrupted checkpoint");
  hr::progression::AccountStore cold{f.backend};
  require(cold.load().issue == hr::progression::LoadIssue::recovered_checkpoint, "real checkpoint recovery");
  view.store = &cold;
  report = flight_report(view);
  require(report.recovered && report.heading == "RECOVERED FLIGHT" && report.credits == cold.account().results.base_credits, "recovery uses recorded settled reward");
}
void stable_actions_safe_pages_and_motion() {
  Fixture f;
  require(f.store.settle(f.receipt).succeeded(), "settled UI fixture");
  for (const auto screen : {Screen::death,Screen::results}) {
    auto view = f.view(screen);
    const auto writes = f.backend.writes;
    view.report_entry_seconds = 0;
    const auto entering = interface(view);
    view.report_entry_seconds = -1;
    const auto arrived = interface(view);
    require(entering.buttons.size() == arrived.buttons.size() && entering.buttons.size() == (screen == Screen::death ? 1U : 2U), "report animation never delays actions");
    for (unsigned i=0;i<entering.buttons.size();++i) {
      const auto& a=entering.buttons[i]; const auto& b=arrived.buttons[i];
      require(a.enabled && a.command == b.command && a.bounds.x == b.bounds.x && a.bounds.y == b.bounds.y && a.bounds.width == b.bounds.width && a.bounds.height == b.bounds.height && a.bounds.height>=44, "actions retain exact final touch geometry throughout reveal");
    }
    view.report_entry_seconds = 0;
    view.promotion.reduced_motion = true;
    const auto reduced = interface(view);
    require(reduced.quads.size() == arrived.quads.size(), "reduced motion has same report content");
    for (unsigned i=0;i<reduced.quads.size();++i) require(reduced.quads[i].rect == arrived.quads[i].rect, "reduced motion is immediately settled");
    for (const float text_scale : {1.F,1.5F,2.F,3.F}) {
      view.text_scale = text_scale;
      auto ui=interface(view);
      for (unsigned page=0;page<ui.page_count;++page) {
        view.ui_page=page; ui=interface(view);
        require(!ui.text_overflow, "report chooses complete readable pages at expanded text");
        for (const auto& button:ui.buttons) require(button.bounds.x>=0 && button.bounds.y>=20 && button.bounds.right()<=320.01F && button.bounds.bottom()<=568.01F && button.bounds.width>=44 && button.bounds.height>=44, "all report-page controls remain safe and touch sized");
      }
    }
    view.save_error = true; view.text_scale = 1; view.ui_page=0;
    const auto protected_ui=interface(view);
    require(protected_ui.buttons.size() == 1 && protected_ui.buttons[0].command == Command::retry_save, "save protection has exclusive recovery action");
    require(f.backend.writes == writes, "all report layouts, pages, accessibility and animation are read only");
  }
}
} // namespace
int main() { pause_previews_current_receipt_without_mutation(); actual_preview_settlement_and_no_mutation(); unlocks_caps_and_recovery_are_real(); stable_actions_safe_pages_and_motion(); }
