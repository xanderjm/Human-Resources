#include "account_internal.h"

#include <array>
#include <new>
#include <utility>

namespace human_resources::progression {
namespace {
class Writer final {
public:
  std::vector<std::byte> bytes;
  void value(std::uint64_t value) {
    for (unsigned i = 0; i < 8; ++i) bytes.push_back(static_cast<std::byte>((value >> (i * 8)) & 255));
  }
  void receipt(const RunReceipt& r) {
    for (const auto v : {r.run_id, r.active_milliseconds, r.score, r.captures, r.soldier_kills,
             r.jeep_kills, r.tank_kills, r.vehicle_captures, r.boss_kills}) value(v);
  }
};
class Reader final {
public:
  std::span<const std::byte> bytes;
  std::size_t position{};
  bool valid{true};
  std::uint64_t value() noexcept {
    if (position + 8 > bytes.size()) { valid = false; return 0; }
    std::uint64_t v{};
    for (unsigned i = 0; i < 8; ++i) v |= std::to_integer<std::uint64_t>(bytes[position++]) << (i * 8);
    return v;
  }
  bool boolean() noexcept {
    const auto v = value();
    if (v > 1) valid = false;
    return v == 1;
  }
  void receipt(RunReceipt& r) noexcept {
    r.run_id = value(); r.active_milliseconds = value(); r.score = value(); r.captures = value();
    r.soldier_kills = value(); r.jeep_kills = value(); r.tank_kills = value();
    r.vehicle_captures = value(); r.boss_kills = value();
  }
};
}
std::vector<std::byte> encode_account(const Account& a) {
  if (!detail::valid_account(a)) return {};
  Writer w;
  w.value(0x43415248); // HRAC payload identity, separate from foundation envelope.
  w.value(2);
  w.value(a.credits);
  for (const auto rank : a.ranks) w.value(rank);
  w.value(a.unlocked_blueprints);
  for (const auto v : {a.counters.captures, a.counters.military_kills, a.counters.active_milliseconds,
           a.counters.base_credits, a.counters.completed_runs, a.best_score, a.best_active_milliseconds}) w.value(v);
  w.value(a.settings.audio); w.value(a.settings.haptics); w.value(a.settings.reduced_motion);
  w.value(a.next_run_id); w.receipt(a.checkpoint); w.value(a.revive_used);
  w.receipt(a.results.receipt); w.value(a.results.base_credits); w.value(a.results.bonus_credits); w.value(a.results.bonus_consumed);
  w.value(a.results.newly_unlocked); w.value(a.results.recovered);
  w.value(a.results.reward_policy);
  w.value(a.continuation.location); w.value(a.continuation.reward_policy);
  w.value(a.continuation.earned_base_credits); w.value(a.continuation.game_state.size());
  w.bytes.insert(w.bytes.end(), a.continuation.game_state.begin(), a.continuation.game_state.end());
  return std::move(w.bytes);
}
detail::AccountDecodeIssue detail::decode_versioned_account(const std::span<const std::byte> payload,
    const std::uint32_t envelope_schema, Account& output) noexcept {
  using Issue = detail::AccountDecodeIssue;
  try {
  if (payload.size() > detail::maximum_payload) return Issue::malformed;
  Reader r{payload};
  if (r.value() != 0x43415248) return Issue::malformed;
  const auto version = r.value();
  if (!r.valid) return Issue::malformed;
  if (version != 1 && version != 2) return Issue::unsupported;
  if (envelope_schema != 0 && envelope_schema != version) return Issue::malformed;
  Account a;
  a.credits = r.value();
  for (auto& rank : a.ranks) {
    const auto v = r.value();
    if (v > 255) return Issue::malformed;
    rank = static_cast<std::uint8_t>(v);
  }
  const auto unlocked = r.value();
  if (unlocked > 65535) return Issue::malformed;
  a.unlocked_blueprints = static_cast<std::uint32_t>(unlocked);
  a.counters.captures = r.value(); a.counters.military_kills = r.value();
  a.counters.active_milliseconds = r.value(); a.counters.base_credits = r.value();
  a.counters.completed_runs = r.value(); a.best_score = r.value(); a.best_active_milliseconds = r.value();
  a.settings.audio = r.boolean(); a.settings.haptics = r.boolean(); a.settings.reduced_motion = r.boolean();
  a.next_run_id = r.value(); r.receipt(a.checkpoint); a.revive_used = r.boolean();
  r.receipt(a.results.receipt); a.results.base_credits = r.value(); a.results.bonus_credits = r.value(); a.results.bonus_consumed = r.boolean();
  const auto newly_unlocked = r.value();
  if (newly_unlocked > 65535) return Issue::malformed;
  a.results.newly_unlocked = static_cast<std::uint32_t>(newly_unlocked); a.results.recovered = r.boolean();
  if (version == 2) {
    const auto result_policy = r.value();
    const auto location = r.value();
    const auto run_policy = r.value();
    a.continuation.earned_base_credits = r.value();
    const auto size = r.value();
    if (!r.valid) return Issue::malformed;
    if (result_policy != 1 || run_policy != 1) return Issue::unsupported;
    if (!r.valid || location > 2 || size > maximum_continuation_bytes || size > payload.size() - r.position)
      return Issue::malformed;
    a.continuation.location = static_cast<std::uint8_t>(location);
    a.continuation.game_state.assign(payload.begin() + static_cast<std::ptrdiff_t>(r.position),
        payload.begin() + static_cast<std::ptrdiff_t>(r.position + static_cast<std::size_t>(size)));
    r.position += static_cast<std::size_t>(size);
  }
  if (!r.valid || r.position != payload.size() || !detail::valid_account(a)) return Issue::malformed;
  output = std::move(a);
  return Issue::none;
  } catch (const std::bad_alloc&) {
    return Issue::malformed;
  }
}
bool decode_account(const std::span<const std::byte> payload, Account& output) noexcept {
  return detail::decode_versioned_account(payload, 0, output) == detail::AccountDecodeIssue::none;
}
} // namespace human_resources::progression
