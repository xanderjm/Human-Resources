#include "account_internal.h"

#include <human_resources/gameplay/tuning.h>

#include <algorithm>
#include <array>
#include <limits>
#include <utility>

namespace human_resources::progression {
namespace tuning = gameplay::tuning;

std::uint64_t base_credits(const RunReceipt& receipt) noexcept {
  if (!detail::valid_receipt(receipt) || receipt.active_milliseconds < tuning::credit_minimum_seconds * 1000)
    return 0;
  return receipt.active_milliseconds / (tuning::credit_seconds_per * 1000) +
      receipt.score / tuning::credit_score_per +
      tuning::credit_per_vehicle * (receipt.jeep_kills + receipt.tank_kills + receipt.vehicle_captures) +
      tuning::credit_per_boss * receipt.boss_kills;
}

namespace detail {
namespace {
std::array<std::uint64_t, 9> values(const RunReceipt& r) noexcept {
  return {r.run_id, r.active_milliseconds, r.score, r.captures, r.soldier_kills,
      r.jeep_kills, r.tank_kills, r.vehicle_captures, r.boss_kills};
}
}
bool valid_receipt(const RunReceipt& receipt) noexcept {
  const auto fields = values(receipt);
  return std::all_of(fields.begin(), fields.end(), [](auto v) { return v <= 1000000000000ULL; });
}
bool follows(const RunReceipt& newer, const RunReceipt& older) noexcept {
  if (newer.run_id != older.run_id || newer.run_id == 0 || !valid_receipt(newer)) return false;
  const auto a = values(newer), b = values(older);
  for (std::size_t i = 1; i < a.size(); ++i)
    if (a[i] < b[i]) return false;
  return true;
}
std::uint64_t counter_value(const Counters& c, const std::string_view counter) noexcept {
  if (counter == "captures") return c.captures;
  if (counter == "military_kills") return c.military_kills;
  if (counter == "base_credits") return c.base_credits;
  if (counter == "active_seconds") return c.active_milliseconds / 1000;
  return 0;
}
std::uint32_t unlocked(const Counters& counters) noexcept {
  auto result = tuning::starter_blueprints;
  for (const auto& rule : tuning::unlock_rules)
    if (counter_value(counters, rule.counter) >= rule.threshold) result |= rule.blueprint;
  return result;
}
Account fresh_account() {
  Account account;
  account.unlocked_blueprints = unlocked(account.counters);
  return account;
}
Account settled(Account account, const RunReceipt& receipt, const bool recovered) {
  return settled_recorded(std::move(account), receipt, base_credits(receipt), recovered);
}
Account settled_recorded(Account account, const RunReceipt& receipt,
    const std::uint64_t credits, const bool recovered) {
  account.credits += credits;
  account.counters.captures += receipt.captures;
  account.counters.military_kills += receipt.soldier_kills + receipt.jeep_kills + receipt.tank_kills;
  account.counters.active_milliseconds += receipt.active_milliseconds;
  account.counters.base_credits += credits;
  ++account.counters.completed_runs;
  const auto unlocked_before = account.unlocked_blueprints;
  account.unlocked_blueprints |= unlocked(account.counters);
  account.best_score = std::max(account.best_score, receipt.score);
  account.best_active_milliseconds = std::max(account.best_active_milliseconds, receipt.active_milliseconds);
  account.checkpoint = {};
  account.continuation = {};
  account.results = {.receipt = receipt, .base_credits = credits,
      .newly_unlocked = account.unlocked_blueprints & ~unlocked_before, .recovered = recovered};
  return account;
}
bool valid_account(const Account& a) noexcept {
  const std::array fields{a.credits, a.counters.captures, a.counters.military_kills,
      a.counters.active_milliseconds, a.counters.base_credits, a.counters.completed_runs,
      a.best_score, a.best_active_milliseconds};
  if (std::any_of(fields.begin(), fields.end(), [](auto v) { return v > maximum_value; })) return false;
  if (a.next_run_id == 0 || a.next_run_id > 1000000000000ULL ||
      (a.unlocked_blueprints & ~65535U) != 0 ||
      (a.unlocked_blueprints & tuning::starter_blueprints) != tuning::starter_blueprints) return false;
  for (std::size_t i = 0; i < a.ranks.size(); ++i)
    if (a.ranks[i] > tuning::permanent_maximum_ranks[i]) return false;
  if (!valid_receipt(a.checkpoint) || !valid_receipt(a.results.receipt)) return false;
  for (const auto& receipt : {a.checkpoint, a.results.receipt}) {
    const auto fields = values(receipt);
    if (receipt.run_id == 0 && std::any_of(fields.begin(), fields.end(), [](auto v) { return v != 0; })) return false;
  }
  if (a.checkpoint.run_id != 0 && (a.checkpoint.run_id + 1 != a.next_run_id || a.results.receipt.run_id != 0)) return false;
  if (a.results.receipt.run_id != 0 && a.results.receipt.run_id + 1 != a.next_run_id) return false;
  // Saved results are historical values. Tuning changes cannot rewrite earned
  // credits or make a previously valid account unreadable.
  if (a.results.reward_policy != 1 || a.results.base_credits > maximum_value ||
      a.results.bonus_credits > maximum_value || a.results.base_credits > a.counters.base_credits) return false;
  if (a.results.receipt.run_id == 0 && (a.results.base_credits != 0 || a.results.bonus_credits != 0)) return false;
  if (a.results.bonus_consumed && a.results.bonus_credits !=
      a.results.base_credits / 2) return false; // Recorded reward policy 1 is +50%.
  if (!a.results.bonus_consumed && a.results.bonus_credits != 0) return false;
  if (a.results.bonus_consumed && a.results.receipt.run_id == 0) return false;
  if (a.results.receipt.run_id != 0 && (a.counters.completed_runs == 0 ||
      a.best_score < a.results.receipt.score || a.best_active_milliseconds < a.results.receipt.active_milliseconds)) return false;
  if ((a.results.newly_unlocked & ~a.unlocked_blueprints) != 0) return false;
  if (a.checkpoint.run_id == 0 && a.results.receipt.run_id == 0 && a.next_run_id != 1) return false;
  const auto& continuation = a.continuation;
  if (continuation.location > 2 || continuation.reward_policy != 1 ||
      continuation.earned_base_credits > maximum_value || continuation.game_state.size() > maximum_continuation_bytes) return false;
  if (!continuation.game_state.empty() && a.checkpoint.run_id == 0) return false;
  if (continuation.game_state.empty() && (continuation.earned_base_credits != 0 || continuation.location != 0)) return false;
  return true;
}
} // namespace detail
} // namespace human_resources::progression
