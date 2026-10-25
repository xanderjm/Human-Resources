#include <human_resources/gameplay/gameplay.h>
#include <human_resources/progression/account_session.h>
#include "progression_test_backend.h"
#include <algorithm>
#include <array>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace gp = human_resources::gameplay;
namespace pr = human_resources::progression;
namespace st = human_resources::storage;
using progression_tests::MemoryBackend;
void require(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}
std::vector<std::byte> read_file(const std::string& path) {
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  require(file.good() && file.tellg() >= 24 && file.tellg() <= 65560, "bounded original fixture");
  std::vector<std::byte> bytes(static_cast<std::size_t>(file.tellg()));
  file.seekg(0);
  require(static_cast<bool>(file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()))), "read original");
  return bytes;
}
void write_file(const std::string& path, const std::vector<std::byte>& bytes) {
  std::ofstream file(path, std::ios::binary);
  file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
  require(file.good(), "write isolated evidence");
}
auto receipt(const pr::RunReceipt& r) {
  return std::array{r.run_id, r.active_milliseconds, r.score, r.captures, r.soldier_kills,
      r.jeep_kills, r.tank_kills, r.vehicle_captures, r.boss_kills};
}
class FailureBackend final : public st::Backend {
public:
  MemoryBackend memory;
  bool reject_account{};
  st::ReadResult read(st::Scope scope, const st::RelativePath& path, std::size_t limit) override {
    return memory.read(scope, path, limit);
  }
  st::WriteResult write_atomically(st::Scope scope, const st::RelativePath& path, std::span<const std::byte> bytes) override {
    if (reject_account && path.value == "account.bin") return {.issue = st::WriteIssue::quota};
    return memory.write_atomically(scope, path, bytes);
  }
};
int main(int argc, char** argv) {
  try {
    require(argc == 5, "usage: account_authority_upgrade original output-prefix settled|continuation historical-bank-reference");
    const std::string original_path = argv[1], prefix = argv[2];
    const bool has_continuation = std::string(argv[3]) == "continuation";
    const auto original = read_file(original_path);
    const auto envelope = st::decode_envelope(original, 2, 65536);
    pr::Account before;
    require(envelope.valid() && pr::decode_account(envelope.payload, before), "historical outer account remains valid");
    require(pr::encode_account(before) == envelope.payload, "historical outer fields roundtrip exactly");
    FailureBackend backend;
    backend.memory.files["account.bin"] = original;
    pr::AccountSession session(backend);
    const auto loaded = session.load();
    require(pr::encode_account(session.account()) == envelope.payload, "loaded wallet/ranks/receipts/settings/bytes remain exact");
    require(backend.memory.writes == 0 && backend.memory.files["account.bin"] == original, "load never rewrites historical account");
    if (!has_continuation) {
      require(before.continuation.game_state.empty() && before.credits == 7 && before.results.base_credits == 7 && before.ranks == std::array<std::uint8_t,4>{1,0,0,0} && before.counters.completed_runs == 2,
          "actual settled seven-credit wallet and purchased rank-one result");
      require(loaded.issue == pr::LoadIssue::loaded && session.writable(), "historical settled account remains writable");
      const auto current_formula = pr::base_credits(before.results.receipt);
      write_file(prefix + "-unchanged.bin", backend.memory.files["account.bin"]);
      std::cout << "{\"passed\":true,\"scope\":\"historical native Android settled fixture\",\"load_issue\":\"loaded\",\"credits\":7,\"rank_damage\":1,\"historical_result_credits\":7,\"current_formula_same_receipt\":"
          << current_formula << ",\"payload_roundtrip_exact\":true,\"writes\":0,\"original_unchanged\":true}\n";
    } else {
      require(before.checkpoint.run_id == 1 && before.checkpoint.active_milliseconds == 250016 && before.continuation.earned_base_credits == 80 && before.credits == 0, "actual Android positive eighty-credit pending run");
      require(before.continuation.game_state.size() > 12 && before.continuation.game_state[8] == std::byte{2}, "source fixture is actual authority-two continuation");
      require(gp::inspect_saved_state(before.continuation.game_state).issue == gp::SaveStateIssue::incompatible_rules,
          "old authority rejected explicitly as incompatible rules");
      require(loaded.issue == pr::LoadIssue::incompatible_continuation && !session.writable(), "known account protects incompatible continuation");
      require(session.begin_run().issue == pr::MutationIssue::protected_save &&
          session.purchase(pr::Track::damage).issue == pr::MutationIssue::protected_save &&
          session.update_settings({}).issue == pr::MutationIssue::protected_save,
          "ordinary start/purchase/settings cannot replace protected account");
      require(backend.memory.writes == 0 && backend.memory.files["account.bin"] == original, "rejected ordinary actions preserve original");
      backend.reject_account = true;
      require(!session.preserve_and_bank_checkpoint({"original-backup.bin"}).succeeded(), "injected isolated replacement failure stays failed");
      require(backend.memory.files["original-backup.bin"] == original && backend.memory.files["account.bin"] == original,
          "verified exact backup precedes failed replacement");
      backend.reject_account = false;
      pr::AccountSession cold_retry(backend);
      require(cold_retry.load().issue == pr::LoadIssue::incompatible_continuation &&
          cold_retry.preserve_and_bank_checkpoint({"original-backup.bin"}).succeeded(), "explicit bank reuses verified identical backup after failure");
      const auto after = cold_retry.account();
      const auto recorded = before.continuation.earned_base_credits;
      require(after.credits == before.credits + recorded && after.ranks == before.ranks &&
          (after.unlocked_blueprints & before.unlocked_blueprints) == before.unlocked_blueprints && after.next_run_id == before.next_run_id &&
          after.settings.audio == before.settings.audio && after.settings.haptics == before.settings.haptics &&
          after.settings.reduced_motion == before.settings.reduced_motion, "bank preserves existing wallet/ranks/unlocks/settings/identity");
      require(after.checkpoint.run_id == 0 && after.continuation.game_state.empty() &&
          receipt(after.results.receipt) == receipt(before.checkpoint) && after.results.recovered &&
          after.results.base_credits == recorded && after.results.bonus_credits == 0 && !after.results.bonus_consumed &&
          after.results.newly_unlocked == (after.unlocked_blueprints & ~before.unlocked_blueprints) && after.results.reward_policy == before.continuation.reward_policy,
          "recorded reward and exact checkpoint terminal result replace old continuation");
      require(after.counters.completed_runs == before.counters.completed_runs + 1 &&
          after.counters.captures == before.counters.captures + before.checkpoint.captures &&
          after.counters.military_kills == before.counters.military_kills + before.checkpoint.soldier_kills + before.checkpoint.jeep_kills + before.checkpoint.tank_kills &&
          after.counters.active_milliseconds == before.counters.active_milliseconds + before.checkpoint.active_milliseconds &&
          after.counters.base_credits == before.counters.base_credits + recorded &&
          after.best_score == std::max(before.best_score, before.checkpoint.score) &&
          after.best_active_milliseconds == std::max(before.best_active_milliseconds, before.checkpoint.active_milliseconds),
          "terminal counters increment exactly once without reset");
      const auto reference = read_file(argv[4]);
      const auto reference_envelope = st::decode_envelope(reference, 2, 65536);
      pr::Account historical_bank;
      require(reference_envelope.valid() && pr::decode_account(reference_envelope.payload, historical_bank) && historical_bank.credits == 80 && historical_bank.results.receipt.run_id == 1, "actual historical Android eighty-credit settlement reference");
      historical_bank.results.recovered = true;
      require(pr::encode_account(after) == pr::encode_account(historical_bank), "every settled outer field exactly matches actual historical Android result except explicit recovered marker");
      const auto banked = backend.memory.files["account.bin"];
      const auto writes_after_bank = backend.memory.writes;
      require(!cold_retry.preserve_and_bank_checkpoint({"original-backup.bin"}).succeeded(), "repeated bank rejected");
      pr::AccountSession reopened(backend);
      require(reopened.load().issue == pr::LoadIssue::loaded && pr::encode_account(reopened.account()) == pr::encode_account(after),
          "new owner loads exact settled account");
      require(reopened.settle(before.checkpoint).issue == pr::MutationIssue::duplicate &&
          !reopened.preserve_and_bank_checkpoint({"original-backup.bin"}).succeeded() &&
          backend.memory.files["account.bin"] == banked && backend.memory.writes == writes_after_bank,
          "cold reopen/duplicate receipt cannot repeat settlement");
      write_file(prefix + "-backup.bin", backend.memory.files["original-backup.bin"]);
      write_file(prefix + "-banked.bin", banked);
      std::cout << "{\"passed\":true,\"scope\":\"historical native Android continuation fixture in isolated memory backend\",\"load_issue\":\"incompatible_continuation\",\"source_run_id\":"
          << before.checkpoint.run_id << ",\"source_active_ms\":" << before.checkpoint.active_milliseconds
          << ",\"source_credits\":" << before.credits << ",\"recorded_checkpoint_reward\":" << recorded
          << ",\"final_credits\":" << after.credits << ",\"completed_before\":" << before.counters.completed_runs
          << ",\"completed_after\":" << after.counters.completed_runs
          << ",\"payload_roundtrip_exact\":true,\"ordinary_mutations_protected\":true,\"backup_exact\":true,\"replacement_failure_preserved_original\":true,\"explicit_bank_once\":true,\"cold_owner_exact\":true,\"original_unchanged\":true}\n";
    }
    require(read_file(original_path) == original, "source historical artifact remains byte-exact");
    return 0;
  } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
