#include "flight_report.h"
#include <algorithm>
#include <human_resources/gameplay/tuning.h>

namespace human_resources::product {
PauseBankPreview pause_bank_preview(const View& view) {
  PauseBankPreview preview;
  if (!view.current_receipt && !view.guest) {
    preview.heading = "BANK PREVIEW UNAVAILABLE";
    preview.detail = "Resume to continue your flight.";
    return preview;
  }
  preview.credits = view.guest ? 0 : progression::base_credits(*view.current_receipt);
  preview.heading = "BANK NOW +" + std::to_string(preview.credits) + " CR";
  if (view.guest) preview.detail = "Saving unavailable. No saved credits.";
  else if (view.current_receipt->active_milliseconds < gameplay::tuning::credit_minimum_seconds * 1000) {
    const auto seconds = gameplay::tuning::credit_minimum_seconds;
    preview.detail = "Resume to reach " + std::to_string(seconds / 60) + ":" + (seconds % 60 < 10 ? "0" : "") + std::to_string(seconds % 60) + " and earn credits.";
  } else preview.detail = "Ends this flight. Saves earned credits once.";
  return preview;
}
namespace {
bool newly_unlocked(const progression::BlueprintView& blueprint, std::uint32_t bits) {
  for (unsigned i = 0; i < static_cast<unsigned>(gameplay::Module::count); ++i)
    if (gameplay::module_id(static_cast<gameplay::Module>(i)) == blueprint.id)
      return (bits & gameplay::blueprint_bit(static_cast<gameplay::Module>(i))) != 0;
  return false;
}
void account_goals(FlightReport& report, const progression::AccountStore& store) {
  const auto blueprints = store.blueprints();
  const progression::BlueprintView* selected = nullptr;
  unsigned unlocked_count = 0;
  for (const auto& blueprint : blueprints) {
    if (newly_unlocked(blueprint, store.account().results.newly_unlocked)) {
      if (!unlocked_count++) selected = &blueprint;
      report.summary += " Unlocked blueprint: " + std::string(blueprint.title) + ". Run requirements: " + blueprint.requirement + ".";
    }
  }
  if (unlocked_count) {
    report.blueprint_heading = unlocked_count == 1 ? "BLUEPRINT UNLOCKED" : "BLUEPRINTS UNLOCKED";
    report.blueprint_name = std::string(selected->title) + (unlocked_count > 1 ? " + " + std::to_string(unlocked_count - 1) + " more" : "");
    report.blueprint_detail = "Run requirements apply";
    report.blueprint_fraction = 1;
  } else {
    for (const auto& blueprint : blueprints)
      if (!blueprint.unlocked && (!selected || static_cast<double>(blueprint.progress) / static_cast<double>(blueprint.threshold) > static_cast<double>(selected->progress) / static_cast<double>(selected->threshold))) selected = &blueprint;
    report.blueprint_heading = selected ? "NEXT BLUEPRINT" : "BLUEPRINT LIBRARY";
    report.blueprint_name = selected ? std::string(selected->title) : "All blueprints unlocked";
    report.blueprint_detail = selected ? selected->milestone : "Explore new combinations";
    report.blueprint_fraction = selected ? static_cast<float>(selected->progress) / static_cast<float>(selected->threshold) : 1;
    report.summary += " " + report.blueprint_heading + ": " + report.blueprint_name + ". " + report.blueprint_detail + ".";
  }
  const auto tracks = store.hangar();
  const progression::TrackView* next = nullptr;
  for (const auto& track : tracks)
    if (track.rank < track.maximum_rank && (!next || track.cost < next->cost)) next = &track;
  if (!next) {
    report.hangar_name = "Hangar complete";
    report.hangar_detail = "Bank " + std::to_string(report.bank) + " CR / max rank";
    report.hangar_fraction = 1;
    report.summary += " All permanent Hangar tracks are at maximum.";
    return;
  }
  report.hangar_name = std::string(next->title);
  report.hangar_fraction = std::min(1.F, static_cast<float>(report.bank) / static_cast<float>(next->cost));
  const auto remaining = next->cost > report.bank ? next->cost - report.bank : 0;
  report.hangar_detail = "Bank " + std::to_string(report.bank) + " / " + std::to_string(next->cost) + " CR";
  report.hangar_action = next->affordable ? "UPGRADE IN HANGAR" : "HANGAR";
  report.summary += " Bank " + std::to_string(report.bank) + ". " + report.hangar_name + ": " + report.hangar_detail + ". " + (remaining ? std::to_string(remaining) + " more credits needed. " : "Ready to upgrade. ") + "Benefit " + next->benefit + ". Permanent upgrades apply to future flights.";
}
} // namespace
FlightReport flight_report(const View& view) {
  FlightReport report;
  const auto& account = view.store->account();
  const auto& results = account.results;
  report.preview = view.screen == Screen::death;
  report.pending = view.pending_results != nullptr;
  report.guest = view.guest;
  report.recovered = !report.preview && results.recovered;
  const auto* receipt = report.preview ? view.current_receipt : view.pending_results ? view.pending_results : &results.receipt;
  report.score = report.guest || !receipt ? view.run->stats.score : receipt->score;
  report.captures = report.guest || !receipt ? view.run->stats.captures : receipt->captures;
  report.seconds = report.guest || !receipt ? view.run->stats.active_seconds : static_cast<double>(receipt->active_milliseconds) / 1000.;
  report.bank = account.credits;
  report.best_score = account.best_score;
  report.credits = report.guest ? 0 : report.preview || report.pending ? (receipt ? progression::base_credits(*receipt) : account.continuation.earned_base_credits) : results.base_credits + results.bonus_credits;
  report.heading = report.preview ? "SIGNAL LOST" : report.recovered ? "RECOVERED FLIGHT" : "RESEARCH COMPLETE";
  report.reward_label = report.guest ? "GUEST FLIGHT" : report.preview ? "CREDITS TO BANK" : report.pending ? "CREDITS TO SAVE" : "CREDITS SAVED";
  report.status = report.guest ? "Saving unavailable. This flight earns no saved credits." : report.preview ? "Your flight ended. Bank its earned research." : report.pending ? "Saving credits. Keep the game open." : report.recovered ? "Recorded rewards recovered and saved." : std::string(location_name(view.theme)) + " / Research Credits saved.";
  if (!report.guest && !report.pending && !report.recovered && report.credits == 0 && report.seconds < gameplay::tuning::credit_minimum_seconds)
    report.status = "Flights under " + std::to_string(gameplay::tuning::credit_minimum_seconds) + " seconds earn no credits.";
  report.summary = report.heading + ". " + report.status + " Score " + std::to_string(report.score) + ". Captures " + std::to_string(report.captures) + ". Survived " + std::to_string(static_cast<unsigned>(report.seconds)) + " seconds. " + report.reward_label + ": " + std::to_string(report.credits) + ".";
  if (!report.preview && !report.pending && !report.guest) {
    report.summary += " Bank " + std::to_string(report.bank) + ". Best score " + std::to_string(report.best_score) + ".";
    account_goals(report, *view.store);
  }
  return report;
}
} // namespace human_resources::product
