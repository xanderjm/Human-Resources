#pragma once

#include <human_resources/progression/progression.h>

#include <memory>

namespace human_resources::progression {

enum class SessionOperation : std::uint8_t { none, begin, checkpoint, settle, purchase, bonus, revive, settings, reset, recover_checkpoint };

// Composition's single account mutation owner. Retains unsaved intent without
// exposing an independently writable guest account or replaying stale recovery.
class AccountSession final {
public:
  explicit AccountSession(storage::Backend& backend);
  [[nodiscard]] LoadResult load();
  [[nodiscard]] MutationResult retry_save();
  [[nodiscard]] MutationResult preserve_and_reset(const storage::RelativePath& backup);
  [[nodiscard]] MutationResult begin_run();
  [[nodiscard]] MutationResult begin_run(const RunContinuation& continuation);
  [[nodiscard]] MutationResult checkpoint(const RunReceipt& receipt, bool background = false);
  [[nodiscard]] MutationResult checkpoint(const RunReceipt& receipt,
      const RunContinuation& continuation, bool background = false);
  [[nodiscard]] bool checkpoint_due(const RunReceipt& receipt, bool boundary = false) const noexcept;
  [[nodiscard]] MutationResult settle(const RunReceipt& receipt);
  [[nodiscard]] MutationResult purchase(Track track);
  [[nodiscard]] MutationResult credit_bonus(std::uint64_t run_id, PlacementOutcome outcome);
  [[nodiscard]] MutationResult revive(const RunReceipt& receipt, PlacementOutcome outcome);
  [[nodiscard]] MutationResult revive(const RunReceipt& receipt,
      const RunContinuation& continuation, PlacementOutcome outcome);
  [[nodiscard]] MutationResult preserve_and_bank_checkpoint(const storage::RelativePath& backup);
  [[nodiscard]] MutationResult update_settings(Settings settings);
  [[nodiscard]] const AccountStore& store() const noexcept { return *store_; }
  [[nodiscard]] const Account& account() const noexcept { return store_->account(); }
  [[nodiscard]] const RunReceipt& latest_receipt() const noexcept { return latest_; }
  [[nodiscard]] LoadResult load_result() const noexcept { return loaded_; }
  [[nodiscard]] bool pending() const noexcept { return operation_ != SessionOperation::none; }
  [[nodiscard]] SessionOperation pending_operation() const noexcept { return operation_; }
  [[nodiscard]] bool writable() const noexcept { return trusted_ && store_->writable() && !pending(); }
private:
  [[nodiscard]] MutationResult apply();
  [[nodiscard]] MutationResult request(SessionOperation operation);
  [[nodiscard]] bool accept_receipt(const RunReceipt& receipt);
  storage::Backend& backend_;
  std::unique_ptr<AccountStore> store_;
  LoadResult loaded_;
  RunReceipt latest_;
  SessionOperation operation_{SessionOperation::none};
  Track track_{};
  Settings settings_;
  RunContinuation continuation_;
  storage::RelativePath recovery_backup_;
  std::uint64_t bonus_run_{};
  bool trusted_{};
  bool established_save_{};
  bool full_continuation_{};
};

} // namespace human_resources::progression
