// Read-only qualification helper. Uses the shipping validators and codecs;
// cannot mutate a save, migrate it or settle a receipt.
#include <human_resources/gameplay/gameplay.h>
#include <human_resources/progression/progression.h>
#include <human_resources/storage/storage.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

namespace st = human_resources::storage;
namespace pr = human_resources::progression;
namespace gp = human_resources::gameplay;
namespace {
class ReadOnly final : public st::Backend {
public:
  std::vector<std::byte> bytes;
  st::ReadResult read(st::Scope scope, const st::RelativePath& path, std::size_t maximum) override {
    if (scope != st::Scope::saves || path.value != "account.bin") return {.issue = st::ReadIssue::not_found, .bytes = {}};
    if (bytes.size() > maximum) return {.issue = st::ReadIssue::too_large, .bytes = {}};
    return {.issue = st::ReadIssue::none, .bytes = bytes};
  }
  st::WriteResult write_atomically(st::Scope, const st::RelativePath&, std::span<const std::byte>) override {
    throw std::runtime_error("Inspector attempted a forbidden save mutation");
  }
};
void write_quoted(std::string_view value) {
  std::cout << '"';
  for (const unsigned char c : value) {
    if (c == '"' || c == '\\') std::cout << '\\' << c;
    else if (c < 32) std::cout << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<unsigned>(c) << std::dec;
    else std::cout << c;
  }
  std::cout << '"';
}
template <class T> void array(const T& values) {
  std::cout << '[';
  bool first = true;
  for (const auto value : values) { if (!first) std::cout << ','; first = false; std::cout << +value; }
  std::cout << ']';
}
}
int main(int argc, char** argv) {
  try {
    if (argc != 2) throw std::runtime_error("Usage: account-inspect READ_ONLY_ACCOUNT_BIN");
    std::ifstream file(argv[1], std::ios::binary | std::ios::ate);
    if (!file || file.tellg() < 24 || file.tellg() > 65560) throw std::runtime_error("Save missing or outside protected size bound");
    const auto size = static_cast<std::size_t>(file.tellg());
    ReadOnly backend; backend.bytes.resize(size); file.seekg(0);
    if (!file.read(reinterpret_cast<char*>(backend.bytes.data()), static_cast<std::streamsize>(size))) throw std::runtime_error("Could not read complete save");
    auto envelope = st::decode_envelope(backend.bytes, 2, 65536);
    if (envelope.issue == st::EnvelopeIssue::unsupported_schema && envelope.schema_version == 1)
      envelope = st::decode_envelope(backend.bytes, 1, 65536);
    if (!envelope.valid()) throw std::runtime_error("Protected envelope rejected, issue=" + std::to_string(static_cast<unsigned>(envelope.issue)));
    pr::AccountStore store(backend);
    const auto loaded = store.load(pr::CheckpointRecovery::preserve_active);
    if (loaded.issue != pr::LoadIssue::loaded) throw std::runtime_error("Native account/continuation rejected, issue=" + std::to_string(static_cast<unsigned>(loaded.issue)));
    const auto& a = store.account();
    std::cout << std::setprecision(17) << "{\n\"envelope_schema\":" << envelope.schema_version
      << ",\"encoded_bytes\":" << size << ",\"native_account_valid\":true,\"credits\":" << a.credits
      << ",\"ranks\":"; array(a.ranks);
    std::cout << ",\"unlocked_blueprints\":" << a.unlocked_blueprints << ",\"completed_runs\":" << a.counters.completed_runs
      << ",\"captured_total\":" << a.counters.captures << ",\"banked_base_credits\":" << a.counters.base_credits
      << ",\"next_run_id\":" << a.next_run_id << ",\"checkpoint_run_id\":" << a.checkpoint.run_id
      << ",\"checkpoint_active_ms\":" << a.checkpoint.active_milliseconds << ",\"checkpoint_score\":" << a.checkpoint.score
      << ",\"checkpoint_captures\":" << a.checkpoint.captures << ",\"revive_used\":" << (a.revive_used ? "true" : "false")
      << ",\"results_run_id\":" << a.results.receipt.run_id << ",\"results_base_credits\":" << a.results.base_credits
      << ",\"results_bonus_credits\":" << a.results.bonus_credits << ",\"location\":" << +a.continuation.location
      << ",\"continuation_bytes\":" << a.continuation.game_state.size() << ",\"recorded_run_credits\":" << a.continuation.earned_base_credits;
    if (!a.continuation.game_state.empty()) {
      gp::Game game;
      if (game.restore_state(a.continuation.game_state) != gp::SaveStateIssue::none || game.save_state() != a.continuation.game_state)
        throw std::runtime_error("Native Game roundtrip did not preserve complete authority");
      const auto& s = game.snapshot();
      std::cout << ",\"native_game_roundtrip_exact\":true,\"state_digest\":\"" << std::hex << std::setw(16) << std::setfill('0') << st::content_digest(a.continuation.game_state) << std::dec
        << "\",\"phase\":" << static_cast<unsigned>(s.phase) << ",\"level\":" << s.stats.level
        << ",\"active_seconds\":" << s.stats.active_seconds << ",\"score\":" << s.stats.score
        << ",\"captures\":" << s.stats.captures << ",\"runner_distance\":" << s.runner_distance
        << ",\"hull\":" << s.hull << ",\"shield\":" << s.shield << ",\"rerolls_remaining\":" << s.rerolls_remaining
        << ",\"entities\":" << s.entities.size() << ",\"projectiles\":" << s.projectiles.size() << ",\"modules\":";
      array(s.modules); std::cout << ",\"research\":"; array(s.research);
      std::cout << ",\"evolution_bits\":" << s.evolution_bits << ",\"choices\":[";
      if (s.phase == gp::Phase::choosing_upgrade) {
        bool first = true;
        for (const auto& choice : s.choices) {
          if (!first) std::cout << ','; first = false;
          std::cout << "{\"hardware\":" << (choice.hardware ? "true" : "false") << ",\"module\":" << static_cast<unsigned>(choice.module)
            << ",\"research\":" << static_cast<unsigned>(choice.research) << ",\"rarity\":" << static_cast<unsigned>(choice.rarity)
            << ",\"rank\":" << choice.next_rank << ",\"title\":"; write_quoted(choice.title); std::cout << '}';
        }
      }
      std::cout << ']';
    }
    std::cout << "}\n";
    return 0;
  } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
