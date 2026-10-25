#include <human_resources/storage/storage.h>

#include "test_support.h"

#include <array>
#include <cstddef>
#include <map>
#include <vector>

namespace {

using test_support::require;

void paths_are_scoped_and_traversal_safe() {
  using namespace human_resources::storage;
  require(validate_relative_path("profiles/player-1/save.bin").valid(), "portable relative save path accepted");
  require(validate_relative_path("../save.bin").issue == PathIssue::traversal, "parent traversal rejected");
  require(validate_relative_path("/tmp/save.bin").issue == PathIssue::absolute, "absolute path rejected");
  require(validate_relative_path("profiles//save.bin").issue == PathIssue::empty_segment,
      "empty path segment rejected");
  require(validate_relative_path("save file.bin").issue == PathIssue::invalid_character,
      "unportable path character rejected");
}

void envelopes_bound_and_verify_payloads() {
  using namespace human_resources::storage;
  const std::array payload{std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4}};
  auto encoded = encode_envelope(7U, payload);
  const auto decoded = decode_envelope(encoded, 7U, 64U);
  require(decoded.valid() && decoded.payload.size() == payload.size(), "valid envelope round trips");
  require(decode_envelope(encoded, 8U, 64U).issue == EnvelopeIssue::unsupported_schema,
      "unsupported schema rejected before payload use");
  require(decode_envelope(encoded, 7U, 2U).issue == EnvelopeIssue::payload_too_large,
      "payload allocation is bounded");
  encoded.back() ^= std::byte{0xff};
  require(decode_envelope(encoded, 7U, 64U).issue == EnvelopeIssue::digest_mismatch,
      "corrupt payload rejected");
}

class MemoryBackend final : public human_resources::storage::Backend {
public:
  using ReadIssue = human_resources::storage::ReadIssue;
  using WriteIssue = human_resources::storage::WriteIssue;
  std::map<std::string, std::vector<std::byte>> files;
  ReadIssue read_failure{ReadIssue::none};
  WriteIssue write_failure{WriteIssue::none};
  bool corrupt_writes{};
  std::size_t writes{};
  human_resources::storage::ReadResult read(human_resources::storage::Scope,
      const human_resources::storage::RelativePath& path, const std::size_t limit) override {
    if (read_failure != ReadIssue::none)
      return {.issue = read_failure};
    const auto found = files.find(path.value);
    if (found == files.end())
      return {.issue = ReadIssue::not_found};
    if (found->second.size() > limit)
      return {.issue = ReadIssue::too_large};
    return {.bytes = found->second};
  }
  human_resources::storage::WriteResult write_atomically(human_resources::storage::Scope,
      const human_resources::storage::RelativePath& path, const std::span<const std::byte> bytes) override {
    ++writes;
    if (write_failure != WriteIssue::none)
      return {.issue = write_failure};
    files[path.value] = {bytes.begin(), bytes.end()};
    if (corrupt_writes)
      files[path.value].push_back(std::byte{0xff});
    return {};
  }
};

void documents_preserve_unreadable_unknown_and_changed_data() {
  using namespace human_resources::storage;
  MemoryBackend backend;
  const RelativePath path{"document.bin"};
  const std::array payload{std::byte{4}, std::byte{5}};
  ProtectedDocument document(backend, Scope::saves, path, 3U, 64U);
  require(document.save(payload).issue == WriteIssue::protected_data && backend.writes == 0U,
      "saving before loading cannot overwrite existing data");
  require(document.load().read_issue == ReadIssue::not_found && document.writable(),
      "confirmed absence enables first save");
  require(document.save(payload).succeeded(), "first save succeeds");
  require(document.load().envelope.valid() && !document.writable(),
      "a decoded envelope still requires product payload validation");
  require(document.accept_loaded_payload() && document.save(payload).succeeded(),
      "accepted product payload allows saving");

  for (const auto issue : {ReadIssue::unavailable, ReadIssue::too_large, ReadIssue::io_failure}) {
    backend.read_failure = issue;
    require(document.load().read_issue == issue && !document.writable(), "failed load is protected");
    require(document.save(payload).issue == WriteIssue::protected_data, "failed loads never become defaults");
  }
  backend.read_failure = ReadIssue::none;
  backend.files[path.value] = encode_envelope(4U, payload);
  const auto future = document.load();
  require(future.envelope.issue == EnvelopeIssue::unsupported_schema && !document.accept_loaded_payload(),
      "future schemas stay protected");
  const auto original = backend.files[path.value];
  backend.write_failure = WriteIssue::quota;
  require(!document.preserve_for_reset({"backup.bin"}).succeeded() && !document.writable(),
      "failed backup cannot permit reset");
  backend.write_failure = WriteIssue::none;
  backend.corrupt_writes = true;
  require(!document.preserve_for_reset({"bad-backup.bin"}).succeeded() && !document.writable(),
      "backup readback must verify original bytes");
  backend.corrupt_writes = false;
  require(document.preserve_for_reset({"backup.bin"}).succeeded(), "verified backup permits explicit reset");
  require(backend.files["backup.bin"] == original && backend.files[path.value] == original,
      "recovery preserves original before reset");
  require(document.save(payload).succeeded(), "explicit recovery can now save supported data");
  backend.files[path.value].push_back(std::byte{99});
  require(document.save(payload).issue == WriteIssue::protected_data,
      "changed data is protected until reloaded");
  require(!document.preserve_for_reset({"backup.bin"}).succeeded(), "existing backup cannot be overwritten");
}
void compatible_migration_and_idempotent_recovery_are_explicit() {
  using namespace human_resources::storage;
  MemoryBackend backend;
  const std::array payload{std::byte{4}, std::byte{5}};
  backend.files["account.bin"] = encode_envelope(1, payload);
  const auto original = backend.files["account.bin"];
  ProtectedDocument document(backend, Scope::saves, {"account.bin"}, 2, 64);
  require(document.load().envelope.issue == EnvelopeIssue::unsupported_schema,
      "legacy schemas are protected unless explicitly whitelisted");
  constexpr std::array<std::uint32_t, 2> readable{1, 3};
  require(document.load(readable).envelope.valid() && document.accept_loaded_payload(),
      "product can explicitly validate a supported older payload");
  require(document.preserve_original({"backup.bin"}).succeeded() &&
      document.preserve_original({"backup.bin"}).succeeded() && backend.files["backup.bin"] == original,
      "identical backup may be verified after an interrupted recovery without overwriting it");
  require(document.save(payload).succeeded() && decode_envelope(backend.files["account.bin"], 2, 64).valid(),
      "successful migration writes the current envelope schema atomically");
  require(!document.preserve_original({"backup.bin"}).succeeded() && backend.files["backup.bin"] == original,
      "a different existing backup is never overwritten");
  backend.files["account.bin"] = encode_envelope(3, payload);
  require(document.load(readable).envelope.issue == EnvelopeIssue::unsupported_schema && !document.writable(),
      "whitelist cannot admit future schemas");
}

} // namespace

int main() {
  paths_are_scoped_and_traversal_safe();
  envelopes_bound_and_verify_payloads();
  documents_preserve_unreadable_unknown_and_changed_data();
  compatible_migration_and_idempotent_recovery_are_explicit();
  return 0;
}
