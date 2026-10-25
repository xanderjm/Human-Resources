#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace human_resources::storage {

enum class Scope : std::uint8_t {
  saves,
  settings,
  cache,
  temporary,
};

enum class PathIssue : std::uint8_t {
  none,
  empty,
  absolute,
  traversal,
  empty_segment,
  invalid_character,
  too_long,
};

struct RelativePath final {
  std::string value;
};

struct PathValidation final {
  PathIssue issue{PathIssue::none};
  RelativePath path;

  [[nodiscard]] bool valid() const noexcept {
    return issue == PathIssue::none;
  }
};

[[nodiscard]] PathValidation validate_relative_path(std::string_view candidate);

enum class EnvelopeIssue : std::uint8_t {
  none,
  truncated,
  invalid_magic,
  unsupported_schema,
  payload_too_large,
  length_mismatch,
  digest_mismatch,
};

struct DecodedEnvelope final {
  EnvelopeIssue issue{EnvelopeIssue::none};
  std::uint32_t schema_version{};
  std::vector<std::byte> payload;

  [[nodiscard]] bool valid() const noexcept {
    return issue == EnvelopeIssue::none;
  }
};

[[nodiscard]] std::uint64_t content_digest(std::span<const std::byte> bytes) noexcept;
[[nodiscard]] std::vector<std::byte> encode_envelope(
    std::uint32_t schema_version, std::span<const std::byte> payload);
[[nodiscard]] DecodedEnvelope decode_envelope(
    std::span<const std::byte> encoded, std::uint32_t supported_schema, std::size_t maximum_payload);

enum class WriteIssue : std::uint8_t {
  none,
  unavailable,
  quota,
  permission,
  io_failure,
  protected_data,
};

struct WriteResult final {
  WriteIssue issue{WriteIssue::none};

  [[nodiscard]] bool succeeded() const noexcept {
    return issue == WriteIssue::none;
  }
};

enum class ReadIssue : std::uint8_t {
  none,
  not_found,
  unavailable,
  too_large,
  io_failure,
};

struct ReadResult final {
  ReadIssue issue{ReadIssue::none};
  std::vector<std::byte> bytes;

  [[nodiscard]] bool succeeded() const noexcept {
    return issue == ReadIssue::none;
  }
};

class Backend {
public:
  virtual ~Backend() = default;
  [[nodiscard]] virtual ReadResult read(
      Scope scope, const RelativePath& path, std::size_t maximum_bytes) = 0;
  virtual WriteResult write_atomically(
      Scope scope, const RelativePath& path, std::span<const std::byte> bytes) = 0;
};

struct DocumentLoad final {
  ReadIssue read_issue{ReadIssue::unavailable};
  DecodedEnvelope envelope;
};

// One writer owns a document. The product must validate its decoded payload
// before accepting it. Failed/future loads cannot silently become a fresh save.
class ProtectedDocument final {
public:
  ProtectedDocument(Backend& backend, Scope scope, RelativePath path,
      std::uint32_t schema, std::size_t maximum_payload);
  [[nodiscard]] DocumentLoad load(std::span<const std::uint32_t> readable_legacy_schemas = {});
  [[nodiscard]] bool accept_loaded_payload() noexcept;
  [[nodiscard]] bool writable() const noexcept {
    return writable_;
  }
  [[nodiscard]] WriteResult save(std::span<const std::byte> payload);
  // Explicit product recovery: preserve and verify the original bytes under a
  // new backup name before allowing reset. An unreadable file cannot be reset.
  [[nodiscard]] WriteResult preserve_for_reset(const RelativePath& backup_path);
  // Compatible recovery may retry an identical verified backup after process
  // termination. Never replaces an existing backup containing different bytes.
  [[nodiscard]] WriteResult preserve_original(const RelativePath& backup_path);

private:
  [[nodiscard]] bool unchanged();
  Backend& backend_;
  Scope scope_;
  RelativePath path_;
  std::uint32_t schema_;
  std::size_t maximum_payload_;
  std::size_t maximum_encoded_;
  ReadIssue last_read_{ReadIssue::unavailable};
  std::vector<std::byte> original_;
  bool decoded_{};
  bool writable_{};
};

} // namespace human_resources::storage
