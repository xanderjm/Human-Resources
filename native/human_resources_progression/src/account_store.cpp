#include "account_internal.h"

#include <human_resources/gameplay/gameplay.h>
#include <human_resources/gameplay/tuning.h>

#include <algorithm>
#include <cmath>
#include <bit>
#include <string>
#include <utility>

namespace human_resources::progression {
namespace tuning = gameplay::tuning;
namespace {
MutationResult reject(const MutationIssue issue) { return {.issue = issue}; }
constexpr std::array<std::string_view, 4> track_names{"Weapon Calibration", "Hull", "Shield", "Biological Capture"};
constexpr std::array<std::string_view, 4> track_benefits{
    "% original weapon damage", " starting and maximum hull",
    " starting and maximum shield", "% base capture rate"};
std::string_view milestone_unit(const std::string_view counter) {
  if (counter == "captures") return "captures";
  if (counter == "military_kills") return "military defeats";
  if (counter == "base_credits") return "base credits";
  if (counter == "active_seconds") return "active seconds";
  return counter;
}
}
AccountStore::AccountStore(storage::Backend& backend)
    : document_(backend, storage::Scope::saves, {"account.bin"}, 2, detail::maximum_payload),
      account_(detail::fresh_account()) {}

LoadResult AccountStore::load(const CheckpointRecovery recovery) {
  ready_ = false;
  recoverable_continuation_ = false;
  constexpr std::array<std::uint32_t, 1> legacy_schemas{1};
  const auto loaded = document_.load(legacy_schemas);
  switch (loaded.read_issue) {
  case storage::ReadIssue::not_found:
    account_ = detail::fresh_account(); ready_ = true;
    return {.issue = LoadIssue::missing};
  case storage::ReadIssue::unavailable: return {.issue = LoadIssue::unavailable};
  case storage::ReadIssue::too_large: return {.issue = LoadIssue::too_large};
  case storage::ReadIssue::io_failure: return {.issue = LoadIssue::io_failure};
  case storage::ReadIssue::none: break;
  }
  if (loaded.envelope.issue == storage::EnvelopeIssue::unsupported_schema)
    return {.issue = LoadIssue::unsupported_schema};
  Account decoded;
  if (!loaded.envelope.valid())
    return {.issue = LoadIssue::corrupt};
  const auto decoded_issue = detail::decode_versioned_account(loaded.envelope.payload,
      loaded.envelope.schema_version, decoded);
  if (decoded_issue == detail::AccountDecodeIssue::unsupported) return {.issue = LoadIssue::unsupported_schema};
  if (decoded_issue != detail::AccountDecodeIssue::none) return {.issue = LoadIssue::corrupt};
  if (!document_.accept_loaded_payload()) return {.issue = LoadIssue::corrupt};
  account_ = decoded; ready_ = true;
  if (!decoded.continuation.game_state.empty()) {
    if (!detail::valid_continuation(decoded)) {
      ready_ = false;
      recoverable_continuation_ = true;
      return {.issue = LoadIssue::incompatible_continuation};
    }
    if (recovery == CheckpointRecovery::preserve_active) return {.issue = LoadIssue::loaded};
    const auto info = gameplay::inspect_saved_state(decoded.continuation.game_state);
    if (info.phase != gameplay::Phase::dead) return {.issue = LoadIssue::resumable_run};
    const auto result = commit(detail::settled_recorded(decoded, decoded.checkpoint,
        decoded.continuation.earned_base_credits, true));
    if (!result.succeeded()) {
      ready_ = false;
      return {.issue = LoadIssue::recovered_checkpoint,
          .recovery_write_issue = result.write_issue == storage::WriteIssue::none ? storage::WriteIssue::protected_data : result.write_issue};
    }
    return {.issue = LoadIssue::recovered_checkpoint};
  }
  if (decoded.checkpoint.run_id == 0 || recovery == CheckpointRecovery::preserve_active)
    return {.issue = LoadIssue::loaded};
  const auto result = commit(detail::settled(decoded, decoded.checkpoint, true));
  if (!result.succeeded()) {
    ready_ = false;
    return {.issue = LoadIssue::recovered_checkpoint, .recovery_write_issue =
        result.write_issue == storage::WriteIssue::none ? storage::WriteIssue::protected_data : result.write_issue};
  }
  return {.issue = LoadIssue::recovered_checkpoint};
}
MutationResult AccountStore::commit(Account candidate) {
  if (!writable()) return reject(MutationIssue::protected_save);
  const auto payload = encode_account(candidate);
  if (payload.empty()) return reject(MutationIssue::invalid_receipt);
  const auto result = document_.save(payload);
  if (!result.succeeded()) return {.issue = MutationIssue::storage_failure, .write_issue = result.issue};
  account_ = std::move(candidate);
  return {};
}
MutationResult AccountStore::begin_run() {
  if (!writable()) return reject(MutationIssue::protected_save);
  if (account_.checkpoint.run_id != 0) return reject(MutationIssue::run_active);
  auto candidate = account_;
  candidate.checkpoint = {.run_id = candidate.next_run_id++};
  candidate.results = {}; candidate.revive_used = false;
  candidate.continuation = {};
  return commit(candidate);
}
MutationResult AccountStore::begin_run(const RunContinuation& continuation) {
  if (!writable()) return reject(MutationIssue::protected_save);
  if (account_.checkpoint.run_id != 0) return reject(MutationIssue::run_active);
  auto candidate = account_;
  candidate.checkpoint = {.run_id = candidate.next_run_id++};
  candidate.results = {}; candidate.revive_used = false; candidate.continuation = continuation;
  if (!detail::valid_continuation(candidate) ||
      gameplay::inspect_saved_state(continuation.game_state).phase != gameplay::Phase::active)
    return reject(MutationIssue::invalid_receipt);
  return commit(std::move(candidate));
}
MutationResult AccountStore::checkpoint(const RunReceipt& receipt, const bool background) {
  if (!writable()) return reject(MutationIssue::protected_save);
  if (!account_.continuation.game_state.empty()) return reject(MutationIssue::invalid_receipt);
  if (!detail::follows(receipt, account_.checkpoint)) return reject(MutationIssue::invalid_receipt);
  if (!background && receipt.active_milliseconds - account_.checkpoint.active_milliseconds < 10000) return {};
  auto candidate = account_; candidate.checkpoint = receipt;
  return commit(candidate);
}
MutationResult AccountStore::checkpoint(const RunReceipt& receipt,
    const RunContinuation& continuation, const bool background) {
  if (!writable()) return reject(MutationIssue::protected_save);
  if (!detail::follows(receipt, account_.checkpoint) || account_.continuation.game_state.empty() ||
      continuation.location != account_.continuation.location) return reject(MutationIssue::invalid_receipt);
  if (!background && receipt.active_milliseconds - account_.checkpoint.active_milliseconds < 10000) return {};
  auto candidate = account_; candidate.checkpoint = receipt; candidate.continuation = continuation;
  if (!detail::valid_continuation(candidate)) return reject(MutationIssue::invalid_receipt);
  return commit(std::move(candidate));
}
MutationResult AccountStore::settle(const RunReceipt& receipt) {
  if (!writable()) return reject(MutationIssue::protected_save);
  if (receipt.run_id != 0 && receipt.run_id == account_.results.receipt.run_id) return reject(MutationIssue::duplicate);
  if (account_.checkpoint.run_id == 0) return reject(MutationIssue::no_active_run);
  if (!detail::follows(receipt, account_.checkpoint)) return reject(MutationIssue::invalid_receipt);
  if (!account_.continuation.game_state.empty()) return settle(receipt, account_.continuation);
  return commit(detail::settled(account_, receipt, false));
}
MutationResult AccountStore::settle(const RunReceipt& receipt, const RunContinuation& continuation) {
  if (!writable()) return reject(MutationIssue::protected_save);
  if (receipt.run_id != 0 && receipt.run_id == account_.results.receipt.run_id) return reject(MutationIssue::duplicate);
  if (!detail::follows(receipt, account_.checkpoint) || account_.continuation.game_state.empty() ||
      continuation.location != account_.continuation.location) return reject(MutationIssue::invalid_receipt);
  auto candidate = account_; candidate.checkpoint = receipt; candidate.continuation = continuation;
  if (!detail::valid_continuation(candidate)) return reject(MutationIssue::invalid_receipt);
  return commit(detail::settled(std::move(candidate), receipt, false));
}
MutationResult AccountStore::purchase(const Track track) {
  if (!writable()) return reject(MutationIssue::protected_save);
  if (account_.checkpoint.run_id != 0) return reject(MutationIssue::run_active);
  const auto index = static_cast<std::size_t>(track);
  if (index >= account_.ranks.size()) return reject(MutationIssue::ineligible);
  const auto rank = account_.ranks[index];
  if (rank >= tuning::permanent_maximum_ranks[index]) return reject(MutationIssue::capped);
  const auto cost = tuning::permanent_costs[rank];
  if (account_.credits < cost) return reject(MutationIssue::insufficient_credits);
  auto candidate = account_; candidate.credits -= cost; ++candidate.ranks[index];
  return commit(candidate);
}
bool AccountStore::credit_bonus_available() const noexcept {
  if (!writable() || account_.checkpoint.run_id != 0 || account_.results.receipt.run_id == 0 ||
      account_.results.base_credits == 0 || account_.results.bonus_consumed) return false;
  std::uint64_t remaining{};
  for (const auto rank : account_.ranks)
    for (std::size_t next = rank; next < tuning::permanent_costs.size(); ++next) remaining += tuning::permanent_costs[next];
  return account_.credits < remaining;
}
MutationResult AccountStore::credit_bonus(const std::uint64_t run_id, const PlacementOutcome outcome) {
  if (!writable()) return reject(MutationIssue::protected_save);
  if (run_id == 0 || run_id != account_.results.receipt.run_id) return reject(MutationIssue::invalid_receipt);
  if (account_.results.bonus_consumed) return reject(MutationIssue::duplicate);
  if (outcome != PlacementOutcome::completed) return reject(MutationIssue::placement_incomplete);
  if (!credit_bonus_available()) return reject(MutationIssue::ineligible);
  auto candidate = account_;
  candidate.results.bonus_credits = candidate.results.base_credits * tuning::bonus_numerator / tuning::bonus_denominator;
  candidate.credits += candidate.results.bonus_credits;
  candidate.results.bonus_consumed = true;
  return commit(candidate);
}
bool AccountStore::revive_available(const RunReceipt& receipt) const noexcept {
  return writable() && !account_.revive_used && detail::follows(receipt, account_.checkpoint) &&
      receipt.active_milliseconds >= tuning::revive_minimum_seconds * 1000;
}
MutationResult AccountStore::revive(const RunReceipt& receipt, const PlacementOutcome outcome) {
  if (!writable()) return reject(MutationIssue::protected_save);
  if (!account_.continuation.game_state.empty()) return reject(MutationIssue::invalid_receipt);
  if (outcome != PlacementOutcome::completed) return reject(MutationIssue::placement_incomplete);
  if (!revive_available(receipt)) return reject(MutationIssue::ineligible);
  auto candidate = account_; candidate.checkpoint = receipt; candidate.revive_used = true;
  return commit(candidate);
}
MutationResult AccountStore::revive(const RunReceipt& receipt,
    const RunContinuation& continuation, const PlacementOutcome outcome) {
  if (!writable()) return reject(MutationIssue::protected_save);
  if (outcome != PlacementOutcome::completed) return reject(MutationIssue::placement_incomplete);
  if (!revive_available(receipt) || account_.continuation.game_state.empty() ||
      continuation.location != account_.continuation.location) return reject(MutationIssue::ineligible);
  auto candidate = account_; candidate.checkpoint = receipt; candidate.revive_used = true;
  candidate.continuation = continuation;
  if (!detail::valid_continuation(candidate) ||
      gameplay::inspect_saved_state(continuation.game_state).phase == gameplay::Phase::dead)
    return reject(MutationIssue::invalid_receipt);
  return commit(std::move(candidate));
}
MutationResult AccountStore::update_settings(const Settings settings) {
  auto candidate = account_; candidate.settings = settings;
  return commit(candidate);
}
std::array<TrackView, 4> AccountStore::hangar() const {
  std::array<TrackView, 4> result{};
  for (std::size_t i = 0; i < result.size(); ++i) {
    const auto rank = account_.ranks[i];
    const auto cost = rank < tuning::permanent_maximum_ranks[i] ? tuning::permanent_costs[rank] : 0;
    const auto gain = static_cast<int>(std::lround(tuning::permanent_per_rank[i] * ((i == 0 || i == 3) ? 100 : 1)));
    const auto benefit = "+" + std::to_string(gain) + std::string(track_benefits[i]);
    result[i] = {static_cast<Track>(i), track_names[i], benefit, rank,
        tuning::permanent_maximum_ranks[i], cost,
        writable() && account_.checkpoint.run_id == 0 && cost != 0 && account_.credits >= cost};
  }
  return result;
}
std::array<BlueprintView, 6> AccountStore::blueprints() const {
  std::array<BlueprintView, 6> result{};
  for (std::size_t i = 0; i < result.size(); ++i) {
    const auto& rule = tuning::unlock_rules[i];
    const auto module = static_cast<std::size_t>(std::countr_zero(rule.blueprint));
    std::string requirement;
    constexpr std::array<std::string_view, 4> numerals{"", " I", " II", " III"};
    for (std::size_t ingredient = 0; ingredient < tuning::module_ids.size(); ++ingredient) {
      const auto rank = tuning::prerequisites[module][ingredient];
      if (rank == 0) continue;
      if (!requirement.empty()) requirement += " and ";
      requirement += tuning::module_names[ingredient];
      requirement += numerals[rank];
    }
    if (requirement.empty()) requirement = "None";
    const auto progress = std::min(detail::counter_value(account_.counters, rule.counter), rule.threshold);
    const bool unlocked = (account_.unlocked_blueprints & rule.blueprint) != 0;
    const auto milestone = (unlocked ? std::string("UNLOCKED / ") : std::to_string(progress) + " / ") +
        std::to_string(rule.threshold) + " " + std::string(milestone_unit(rule.counter));
    result[i] = {tuning::module_ids[module], tuning::module_names[module], milestone,
        std::move(requirement), progress, rule.threshold, unlocked};
  }
  return result;
}
MutationResult AccountStore::preserve_and_reset(const storage::RelativePath& backup) {
  const auto preserved = document_.preserve_for_reset(backup);
  if (!preserved.succeeded()) return {.issue = MutationIssue::storage_failure, .write_issue = preserved.issue};
  ready_ = true;
  return commit(detail::fresh_account());
}
MutationResult AccountStore::preserve_and_bank_checkpoint(const storage::RelativePath& backup) {
  if (!recoverable_continuation_ || account_.checkpoint.run_id == 0 || account_.continuation.game_state.empty())
    return reject(MutationIssue::protected_save);
  const auto preserved = document_.preserve_original(backup);
  if (!preserved.succeeded()) return {.issue = MutationIssue::storage_failure, .write_issue = preserved.issue};
  ready_ = true;
  const auto result = commit(detail::settled_recorded(account_, account_.checkpoint,
      account_.continuation.earned_base_credits, true));
  if (result.succeeded()) recoverable_continuation_ = false;
  else ready_ = false;
  return result;
}
} // namespace human_resources::progression
