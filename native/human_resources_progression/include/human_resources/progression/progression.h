#pragma once

#include <human_resources/storage/storage.h>

#include <array>
#include <cstdint>
#include <span>
#include <string_view>
#include <string>
#include <vector>

namespace human_resources::gameplay { class Game; }

namespace human_resources::progression {

enum class Track : std::uint8_t { damage, hull, shield, capture };
struct Settings final {
  bool audio{true};
  bool haptics{true};
  bool reduced_motion{};
};
struct RunReceipt final {
  std::uint64_t run_id{};
  std::uint64_t active_milliseconds{};
  std::uint64_t score{};
  std::uint64_t captures{};
  std::uint64_t soldier_kills{};
  std::uint64_t jeep_kills{};
  std::uint64_t tank_kills{};
  std::uint64_t vehicle_captures{};
  std::uint64_t boss_kills{};
};
struct Counters final {
  std::uint64_t captures{};
  std::uint64_t military_kills{};
  std::uint64_t active_milliseconds{};
  std::uint64_t base_credits{};
  std::uint64_t completed_runs{};
};
struct Results final {
  RunReceipt receipt;
  std::uint64_t base_credits{};
  std::uint64_t bonus_credits{};
  bool bonus_consumed{};
  std::uint32_t newly_unlocked{};
  bool recovered{};
  std::uint32_t reward_policy{1}; // Historical recorded rewards use their original policy.
};
inline constexpr std::size_t maximum_continuation_bytes = 48 * 1024;
// Gameplay owns the versioned bytes. Location is the stable product location
// identity: 0 farmland, 1 suburbia, 2 desert; no renderer state is persisted.
struct RunContinuation final {
  std::uint8_t location{};
  std::uint32_t reward_policy{1};
  std::uint64_t earned_base_credits{};
  std::vector<std::byte> game_state;
};
// Capture at a simulation boundary, only when checkpoint_due or a choice,
// pause, death or lifecycle boundary requires a durable write.
[[nodiscard]] RunContinuation continuation_for(const gameplay::Game& game, std::uint8_t location);
struct Account final {
  std::uint64_t credits{};
  std::array<std::uint8_t, 4> ranks{};
  std::uint32_t unlocked_blueprints{};
  Counters counters;
  std::uint64_t best_score{};
  std::uint64_t best_active_milliseconds{};
  Settings settings;
  std::uint64_t next_run_id{1};
  RunReceipt checkpoint;
  bool revive_used{};
  Results results;
  RunContinuation continuation;
};
struct TrackView final {
  Track track{};
  std::string_view title;
  std::string benefit;
  std::uint32_t rank{};
  std::uint32_t maximum_rank{};
  std::uint64_t cost{};
  bool affordable{};
  // Display projections of the owned and next permanent totals, from the same
  // tuning as run setup. A capped track has no next bonus.
  std::string owned_bonus;
  std::string next_bonus;
  std::string bonus_basis;
};
struct BlueprintView final {
  std::string_view id;
  std::string_view title;
  // Account unlock progress includes its unit; requirement is a separate run draw prerequisite.
  std::string milestone;
  std::string requirement;
  std::uint64_t progress{};
  std::uint64_t threshold{};
  bool unlocked{};
};
enum class LoadIssue : std::uint8_t {
  loaded, missing, recovered_checkpoint, corrupt, unsupported_schema, unavailable, too_large, io_failure,
  resumable_run, incompatible_continuation
};
struct LoadResult final {
  LoadIssue issue{LoadIssue::unavailable};
  storage::WriteIssue recovery_write_issue{storage::WriteIssue::none};
  [[nodiscard]] bool playable() const noexcept {
    return issue == LoadIssue::loaded || issue == LoadIssue::missing || issue == LoadIssue::resumable_run ||
        (issue == LoadIssue::recovered_checkpoint && recovery_write_issue == storage::WriteIssue::none);
  }
};
enum class MutationIssue : std::uint8_t {
  none, protected_save, invalid_receipt, run_active, no_active_run, insufficient_credits,
  capped, duplicate, ineligible, placement_incomplete, storage_failure
};
struct MutationResult final {
  MutationIssue issue{MutationIssue::none};
  storage::WriteIssue write_issue{storage::WriteIssue::none};
  [[nodiscard]] bool succeeded() const noexcept { return issue == MutationIssue::none; }
};
enum class PlacementOutcome : std::uint8_t { completed, cancelled, failed, unavailable };
enum class CheckpointRecovery : std::uint8_t { settle, preserve_active };
// An explicit local test double. Composition chooses the outcome; no live SDK is linked.
struct LocalPlacement final {
  PlacementOutcome outcome{PlacementOutcome::unavailable};
  [[nodiscard]] PlacementOutcome complete() const noexcept { return outcome; }
};

[[nodiscard]] std::uint64_t base_credits(const RunReceipt& receipt) noexcept;
[[nodiscard]] std::vector<std::byte> encode_account(const Account& account);
[[nodiscard]] bool decode_account(std::span<const std::byte> payload, Account& output) noexcept;

// Single-threaded product owner; Backend must outlive this object. Every mutation
// commits atomically before publishing state. Failed saves retain the prior state.
class AccountStore final {
public:
  explicit AccountStore(storage::Backend& backend);
  [[nodiscard]] LoadResult load(CheckpointRecovery recovery = CheckpointRecovery::settle);
  [[nodiscard]] const Account& account() const noexcept { return account_; }
  [[nodiscard]] bool writable() const noexcept { return ready_ && document_.writable(); }
  [[nodiscard]] MutationResult begin_run();
  [[nodiscard]] MutationResult begin_run(const RunContinuation& continuation);
  // Call with active counters regularly; writes at ten active seconds or on background.
  [[nodiscard]] MutationResult checkpoint(const RunReceipt& receipt, bool background = false);
  [[nodiscard]] MutationResult checkpoint(const RunReceipt& receipt,
      const RunContinuation& continuation, bool background = false);
  [[nodiscard]] MutationResult settle(const RunReceipt& receipt);
  [[nodiscard]] MutationResult settle(const RunReceipt& receipt, const RunContinuation& continuation);
  [[nodiscard]] MutationResult purchase(Track track);
  [[nodiscard]] MutationResult credit_bonus(std::uint64_t run_id, PlacementOutcome outcome);
  [[nodiscard]] MutationResult revive(const RunReceipt& receipt, PlacementOutcome outcome);
  [[nodiscard]] MutationResult revive(const RunReceipt& receipt,
      const RunContinuation& continuation, PlacementOutcome outcome);
  [[nodiscard]] MutationResult update_settings(Settings settings);
  [[nodiscard]] bool credit_bonus_available() const noexcept;
  [[nodiscard]] bool revive_available(const RunReceipt& receipt) const noexcept;
  [[nodiscard]] std::array<TrackView, 4> hangar() const;
  [[nodiscard]] std::array<BlueprintView, 6> blueprints() const;
  // Explicit recovery preserves and verifies the original before resetting.
  [[nodiscard]] MutationResult preserve_and_reset(const storage::RelativePath& backup);
  [[nodiscard]] MutationResult preserve_and_bank_checkpoint(const storage::RelativePath& backup);
private:
  [[nodiscard]] MutationResult commit(Account candidate);
  storage::ProtectedDocument document_;
  Account account_;
  bool ready_{};
  bool recoverable_continuation_{};
};

} // namespace human_resources::progression
