#include <human_resources/progression/account_session.h>

#include "account_internal.h"
#include <human_resources/gameplay/gameplay.h>

namespace human_resources::progression {
namespace {
MutationResult protected_save() { return {.issue = MutationIssue::protected_save}; }
bool retryable(const MutationResult result) {
  return result.issue == MutationIssue::storage_failure || result.issue == MutationIssue::protected_save;
}
}

AccountSession::AccountSession(storage::Backend& backend)
    : backend_(backend), store_(std::make_unique<AccountStore>(backend)) {}

LoadResult AccountSession::load() {
  // Repeated UI/lifecycle loads must not settle an actual live run's old checkpoint.
  if (trusted_ || pending()) return loaded_;
  loaded_ = store_->load();
  trusted_ = loaded_.playable();
  if (trusted_) {
    latest_ = account().checkpoint;
    continuation_ = account().continuation;
    full_continuation_ = !continuation_.game_state.empty();
    established_save_ = loaded_.issue != LoadIssue::missing;
  }
  return loaded_;
}

bool AccountSession::accept_receipt(const RunReceipt& receipt) {
  if (!trusted_ || !detail::follows(receipt, account().checkpoint)) return false;
  if (latest_.run_id != 0 && !detail::follows(receipt, latest_)) return false;
  latest_ = receipt;
  return true;
}

MutationResult AccountSession::request(const SessionOperation operation) {
  if (!trusted_ || pending()) return protected_save();
  operation_ = operation;
  return apply();
}

MutationResult AccountSession::apply() {
  MutationResult result;
  switch (operation_) {
  case SessionOperation::none: return {};
  case SessionOperation::begin: result = full_continuation_ ? store_->begin_run(continuation_) : store_->begin_run(); break;
  case SessionOperation::checkpoint: result = full_continuation_ ? store_->checkpoint(latest_, continuation_, true) : store_->checkpoint(latest_, true); break;
  case SessionOperation::settle: result = full_continuation_ ? store_->settle(latest_, continuation_) : store_->settle(latest_); break;
  case SessionOperation::purchase: result = store_->purchase(track_); break;
  case SessionOperation::bonus: result = store_->credit_bonus(bonus_run_, PlacementOutcome::completed); break;
  case SessionOperation::revive: result = full_continuation_ ? store_->revive(latest_, continuation_, PlacementOutcome::completed) : store_->revive(latest_, PlacementOutcome::completed); break;
  case SessionOperation::settings: result = store_->update_settings(settings_); break;
  // Only reached after preserve_and_reset verified the backup but fresh save failed.
  case SessionOperation::reset: result = store_->update_settings({}); break;
  case SessionOperation::recover_checkpoint: result = store_->preserve_and_bank_checkpoint(recovery_backup_); break;
  }
  if (result.succeeded()) {
    established_save_ = true;
    if (operation_ == SessionOperation::begin) latest_ = account().checkpoint;
    if (operation_ == SessionOperation::reset || operation_ == SessionOperation::recover_checkpoint) {
      trusted_ = true;
      loaded_ = {.issue = operation_ == SessionOperation::reset ? LoadIssue::loaded : LoadIssue::recovered_checkpoint};
      latest_ = {};
      continuation_ = {};
      full_continuation_ = false;
    }
    operation_ = SessionOperation::none;
  } else if (!retryable(result)) {
    operation_ = SessionOperation::none;
  }
  return result;
}

MutationResult AccountSession::retry_save() {
  if (operation_ == SessionOperation::recover_checkpoint) return apply();
  if (!trusted_ && operation_ != SessionOperation::reset) {
    const auto result = load();
    return result.playable() ? MutationResult{} : protected_save();
  }
  if (!store_->writable()) {
    // Reload protection, not process-death recovery. An older settled checkpoint
    // is not the full live receipt, even if it has the same run ID.
    auto candidate = std::make_unique<AccountStore>(backend_);
    const auto result = candidate->load(CheckpointRecovery::preserve_active);
    if (!result.playable() || (established_save_ && result.issue == LoadIssue::missing) ||
        encode_account(candidate->account()) != encode_account(account()))
      return protected_save();
    store_ = std::move(candidate);
  }
  if (pending()) return apply();
  // A live receipt may be newer than its persisted ten-second checkpoint.
  if (latest_.run_id != 0 && account().checkpoint.run_id != 0)
    return full_continuation_ ? checkpoint(latest_, continuation_, true) : checkpoint(latest_, true);
  return {};
}

MutationResult AccountSession::begin_run() {
  if (pending()) return protected_save();
  full_continuation_ = false;
  continuation_ = {};
  return request(SessionOperation::begin);
}
MutationResult AccountSession::begin_run(const RunContinuation& continuation) {
  if (pending()) return protected_save();
  continuation_ = continuation;
  full_continuation_ = true;
  return request(SessionOperation::begin);
}
bool AccountSession::checkpoint_due(const RunReceipt& receipt, const bool boundary) const noexcept {
  return boundary || operation_ == SessionOperation::checkpoint ||
      (receipt.active_milliseconds >= account().checkpoint.active_milliseconds &&
          receipt.active_milliseconds - account().checkpoint.active_milliseconds >= 10000);
}

MutationResult AccountSession::checkpoint(const RunReceipt& receipt, const bool background) {
  if (pending() && operation_ != SessionOperation::checkpoint) return protected_save();
  if (!account().continuation.game_state.empty()) return {.issue = MutationIssue::invalid_receipt};
  if (!accept_receipt(receipt)) return {.issue = MutationIssue::invalid_receipt};
  if (operation_ == SessionOperation::checkpoint) return apply();
  const auto result = store_->checkpoint(receipt, background);
  if (retryable(result)) operation_ = SessionOperation::checkpoint;
  return result;
}
MutationResult AccountSession::checkpoint(const RunReceipt& receipt,
    const RunContinuation& continuation, const bool background) {
  if (pending() && operation_ != SessionOperation::checkpoint) return protected_save();
  auto candidate = account(); candidate.checkpoint = receipt; candidate.continuation = continuation;
  if (account().continuation.game_state.empty() || continuation.location != account().continuation.location ||
      !detail::valid_continuation(candidate)) return {.issue = MutationIssue::invalid_receipt};
  if (!accept_receipt(receipt)) return {.issue = MutationIssue::invalid_receipt};
  continuation_ = continuation;
  full_continuation_ = true;
  if (operation_ == SessionOperation::checkpoint) return apply();
  const auto result = store_->checkpoint(receipt, continuation_, background);
  if (retryable(result)) operation_ = SessionOperation::checkpoint;
  return result;
}

MutationResult AccountSession::settle(const RunReceipt& receipt) {
  if (pending() && operation_ != SessionOperation::checkpoint) return protected_save();
  if (receipt.run_id != 0 && receipt.run_id == account().results.receipt.run_id)
    return {.issue = MutationIssue::duplicate};
  if (full_continuation_) {
    auto candidate = account(); candidate.checkpoint = receipt; candidate.continuation = continuation_;
    if (!detail::valid_continuation(candidate)) return {.issue = MutationIssue::invalid_receipt};
  }
  if (!accept_receipt(receipt)) return {.issue = MutationIssue::invalid_receipt};
  operation_ = SessionOperation::settle;
  return apply();
}

MutationResult AccountSession::purchase(const Track track) {
  if (pending()) return protected_save();
  track_ = track;
  return request(SessionOperation::purchase);
}
MutationResult AccountSession::credit_bonus(const std::uint64_t run_id, const PlacementOutcome outcome) {
  if (outcome != PlacementOutcome::completed) return {.issue = MutationIssue::placement_incomplete};
  if (pending()) return protected_save();
  bonus_run_ = run_id;
  return request(SessionOperation::bonus);
}
MutationResult AccountSession::revive(const RunReceipt& receipt, const PlacementOutcome outcome) {
  if (outcome != PlacementOutcome::completed) return {.issue = MutationIssue::placement_incomplete};
  if (pending() && operation_ != SessionOperation::checkpoint) return protected_save();
  if (!account().continuation.game_state.empty()) return {.issue = MutationIssue::invalid_receipt};
  if (!accept_receipt(receipt)) return {.issue = MutationIssue::invalid_receipt};
  operation_ = SessionOperation::revive;
  return apply();
}
MutationResult AccountSession::revive(const RunReceipt& receipt,
    const RunContinuation& continuation, const PlacementOutcome outcome) {
  if (outcome != PlacementOutcome::completed) return {.issue = MutationIssue::placement_incomplete};
  if (pending() && operation_ != SessionOperation::checkpoint) return protected_save();
  if (account().revive_used) return {.issue = MutationIssue::ineligible};
  auto candidate = account(); candidate.checkpoint = receipt; candidate.continuation = continuation; candidate.revive_used = true;
  if (account().continuation.game_state.empty() || continuation.location != account().continuation.location ||
      !detail::valid_continuation(candidate) ||
      gameplay::inspect_saved_state(continuation.game_state).phase == gameplay::Phase::dead)
    return {.issue = MutationIssue::invalid_receipt};
  if (!accept_receipt(receipt)) return {.issue = MutationIssue::invalid_receipt};
  continuation_ = continuation;
  full_continuation_ = true;
  operation_ = SessionOperation::revive;
  return apply();
}
MutationResult AccountSession::preserve_and_bank_checkpoint(const storage::RelativePath& backup) {
  if (trusted_ || pending() || loaded_.issue != LoadIssue::incompatible_continuation) return protected_save();
  recovery_backup_ = backup;
  operation_ = SessionOperation::recover_checkpoint;
  return apply();
}
MutationResult AccountSession::update_settings(const Settings settings) {
  if (pending()) return protected_save();
  settings_ = settings;
  return request(SessionOperation::settings);
}

MutationResult AccountSession::preserve_and_reset(const storage::RelativePath& backup) {
  if (trusted_ || pending() || (loaded_.issue != LoadIssue::corrupt &&
      loaded_.issue != LoadIssue::unsupported_schema)) return protected_save();
  const auto result = store_->preserve_and_reset(backup);
  if (result.succeeded()) {
    trusted_ = true;
    established_save_ = true;
    loaded_ = {.issue = LoadIssue::loaded};
    latest_ = {};
  } else if (store_->writable()) {
    operation_ = SessionOperation::reset;
  }
  return result;
}

} // namespace human_resources::progression
