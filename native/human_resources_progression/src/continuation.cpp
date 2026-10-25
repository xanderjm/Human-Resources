#include "account_internal.h"

#include <human_resources/gameplay/gameplay.h>

#include <cmath>

namespace human_resources::progression {
namespace {
RunReceipt receipt_from(const gameplay::RunStats& stats, const std::uint64_t run_id) {
  return {.run_id = run_id,
      .active_milliseconds = static_cast<std::uint64_t>(stats.active_seconds * 1000),
      .score = stats.score, .captures = stats.captures, .soldier_kills = stats.soldier_kills,
      .jeep_kills = stats.jeep_kills, .tank_kills = stats.tank_kills,
      .vehicle_captures = stats.vehicle_captures, .boss_kills = stats.boss_kills};
}
bool same_receipt(const RunReceipt& a, const RunReceipt& b) {
  return a.run_id == b.run_id && a.active_milliseconds == b.active_milliseconds && a.score == b.score &&
      a.captures == b.captures && a.soldier_kills == b.soldier_kills && a.jeep_kills == b.jeep_kills &&
      a.tank_kills == b.tank_kills && a.vehicle_captures == b.vehicle_captures && a.boss_kills == b.boss_kills;
}
}
RunContinuation continuation_for(const gameplay::Game& game, const std::uint8_t location) {
  RunContinuation result;
  result.location = location;
  result.game_state = game.save_state();
  const auto& stats = game.snapshot().stats;
  if (!std::isfinite(stats.active_seconds) || stats.active_seconds < 0 || stats.active_seconds > 1000000000)
    return {};
  result.earned_base_credits = base_credits(receipt_from(stats, 0));
  return result;
}
bool detail::valid_continuation(const Account& account) {
  const auto& continuation = account.continuation;
  if (continuation.game_state.empty() || continuation.location > 2 || continuation.reward_policy != 1 ||
      continuation.game_state.size() > maximum_continuation_bytes) return false;
  const auto info = gameplay::inspect_saved_state(continuation.game_state);
  if (!info.valid() || info.phase == gameplay::Phase::ready || info.revive_count != (account.revive_used ? 1U : 0U) ||
      info.setup.permanent_ranks != account.ranks || info.setup.unlocked_blueprints != account.unlocked_blueprints ||
      !std::isfinite(info.stats.active_seconds) || info.stats.active_seconds < 0 || info.stats.active_seconds > 1000000000)
    return false;
  return same_receipt(receipt_from(info.stats, account.checkpoint.run_id), account.checkpoint) &&
      continuation.earned_base_credits == base_credits(account.checkpoint);
}
} // namespace human_resources::progression
