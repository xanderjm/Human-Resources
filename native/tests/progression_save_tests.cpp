#include <human_resources/progression/progression.h>

#include "progression_test_backend.h"
#include "test_support.h"

namespace {
using namespace human_resources::progression;
using namespace human_resources::storage;
using test_support::require;
using progression_tests::MemoryBackend;
void interrupted_checkpoint_settles_once() {
  MemoryBackend backend;
  {
    AccountStore first(backend); (void)first.load(); (void)first.begin_run();
    const RunReceipt checkpoint{.run_id = first.account().checkpoint.run_id,
        .active_milliseconds = 90000, .score = 500, .captures = 300};
    require(first.checkpoint(checkpoint).succeeded(), "durable checkpoint before process death");
  }
  AccountStore second(backend);
  require(second.load().issue == LoadIssue::recovered_checkpoint, "process death goes to results");
  const auto credits = second.account().credits;
  require(credits == 111 && second.account().results.recovered && second.account().checkpoint.run_id == 0,
      "last checkpoint banks once without resuming run");
  AccountStore third(backend);
  require(third.load().issue == LoadIssue::loaded && third.account().credits == credits &&
      third.account().counters.completed_runs == 1, "second launch never resettles recovery");
  require(third.credit_bonus(third.account().results.receipt.run_id, PlacementOutcome::completed).succeeded(), "recovered result permits bonus");
  AccountStore fourth(backend); (void)fourth.load();
  require(!fourth.credit_bonus_available(), "bonus state survives process death");
}
void failed_mutations_preserve_published_account() {
  MemoryBackend backend; AccountStore store(backend); (void)store.load(); (void)store.begin_run();
  RunReceipt receipt{.run_id = store.account().checkpoint.run_id, .active_milliseconds = 90000, .score = 500};
  backend.write_failure = WriteIssue::quota;
  require(store.settle(receipt).issue == MutationIssue::storage_failure && store.account().credits == 0 &&
      store.account().checkpoint.run_id == receipt.run_id, "failed settlement rolls back account");
  backend.write_failure = WriteIssue::none;
  require(store.settle(receipt).succeeded() && store.account().credits == 111, "retry settlement works once");
  backend.write_failure = WriteIssue::io_failure;
  require(store.purchase(Track::damage).issue == MutationIssue::storage_failure && store.account().ranks[0] == 0 &&
      store.account().credits == 111, "failed purchase retains credits and rank");
  backend.write_failure = WriteIssue::none;
  require(store.purchase(Track::damage).succeeded() && store.account().credits == 31, "retry purchase commits atomically");
  require(store.update_settings({false, false, true}).succeeded(), "settings save through product document");
  AccountStore reopened(backend); (void)reopened.load();
  require(!reopened.account().settings.audio && reopened.account().settings.reduced_motion, "settings restored");
}
void bad_data_is_distinct_and_protected() {
  MemoryBackend backend; AccountStore store(backend); (void)store.load();
  require(store.update_settings({}).succeeded(), "seed valid account");
  const auto original = backend.files["account.bin"];
  for (const auto issue : {ReadIssue::unavailable, ReadIssue::too_large, ReadIssue::io_failure}) {
    backend.read_failure = issue; AccountStore failed(backend); const auto result = failed.load();
    require(!result.playable() && result.issue != LoadIssue::missing && !failed.writable(), "failed read never new account");
    require(failed.begin_run().issue == MutationIssue::protected_save && backend.files["account.bin"] == original,
        "failed read preserves bytes");
  }
  backend.read_failure = ReadIssue::none;
  backend.files["account.bin"] = encode_envelope(3, {});
  AccountStore future(backend);
  require(future.load().issue == LoadIssue::unsupported_schema && !future.writable(), "future schema protected");
  require(future.preserve_and_reset({"future-backup.bin"}).succeeded(), "explicit reset preserves original");
  require(backend.files["future-backup.bin"] == encode_envelope(3, {}), "future original backup retained");
  backend.files["account.bin"] = original; backend.files["account.bin"].back() ^= std::byte{1};
  AccountStore corrupt(backend);
  require(corrupt.load().issue == LoadIssue::corrupt && !corrupt.writable(), "digest corruption protected");
  backend.files["account.bin"] = encode_envelope(1, std::array{std::byte{1}});
  AccountStore bad_payload(backend);
  require(bad_payload.load().issue == LoadIssue::corrupt && !bad_payload.writable(), "valid envelope malformed product payload protected");
  backend.files["account.bin"] = original; AccountStore changed(backend); (void)changed.load();
  backend.files["account.bin"].push_back(std::byte{2});
  require(changed.begin_run().issue == MutationIssue::storage_failure && !changed.writable(), "external concurrent replacement protected");
}
void payload_decoder_rejects_truncation_and_impossible_ranks() {
  MemoryBackend backend; AccountStore store(backend); (void)store.load();
  const auto valid = encode_account(store.account());
  Account decoded;
  for (std::size_t n = 0; n < valid.size(); ++n)
    require(!decode_account(std::span(valid).first(n), decoded), "every truncation rejected");
  auto invalid = valid; invalid[24] = std::byte{6};
  require(!decode_account(invalid, decoded), "out-of-range permanent rank rejected");
  invalid = valid; invalid.push_back(std::byte{0});
  require(!decode_account(invalid, decoded), "trailing bytes rejected");
  require(decode_account(valid, decoded), "canonical payload round trips");
}
void interrupted_recovery_write_can_retry_without_duplication() {
  MemoryBackend backend;
  AccountStore first(backend); (void)first.load(); (void)first.begin_run();
  const RunReceipt receipt{.run_id = first.account().checkpoint.run_id, .active_milliseconds = 90000, .score = 500};
  require(first.checkpoint(receipt).succeeded(), "checkpoint recovery fixture");
  backend.write_failure = WriteIssue::io_failure;
  AccountStore recovered(backend);
  const auto failed = recovered.load();
  require(failed.issue == LoadIssue::recovered_checkpoint && !failed.playable() && !recovered.writable(),
      "failed recovery is explicit and cannot start a conflicting run");
  require(recovered.begin_run().issue == MutationIssue::protected_save, "failed recovery stays protected");
  backend.write_failure = WriteIssue::none;
  require(recovered.load().playable() && recovered.account().credits == 111, "load retries pending settlement once");
  require(recovered.load().issue == LoadIssue::loaded && recovered.account().credits == 111,
      "repeated load does not duplicate retried recovery");
}
void bonus_receipt_is_independent_of_amount() {
  MemoryBackend backend; AccountStore store(backend); (void)store.load(); (void)store.begin_run();
  const RunReceipt receipt{.run_id = store.account().checkpoint.run_id, .active_milliseconds = 1000};
  require(store.settle(receipt).succeeded(), "settle zero-credit short attempt");
  auto account = store.account();
  account.results.bonus_consumed = true;
  const auto bytes = encode_account(account);
  Account decoded;
  require(decode_account(bytes, decoded) && decoded.results.bonus_consumed && decoded.results.bonus_credits == 0,
      "zero-valued completion receipt is durable");
  backend.files["account.bin"] = encode_envelope(2, bytes);
  AccountStore reopened(backend); (void)reopened.load();
  require(reopened.credit_bonus(receipt.run_id, PlacementOutcome::completed).issue == MutationIssue::duplicate,
      "zero-valued completed reward never repeats");
}

}
int main() {
  interrupted_checkpoint_settles_once(); interrupted_recovery_write_can_retry_without_duplication();
  bonus_receipt_is_independent_of_amount(); failed_mutations_preserve_published_account();
  bad_data_is_distinct_and_protected(); payload_decoder_rejects_truncation_and_impossible_ranks();
}
