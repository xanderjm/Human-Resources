#include <human_resources/storage/storage.h>

#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>
#include <utility>

namespace human_resources::storage {
namespace {

constexpr std::array<std::byte, 4> magic{
    std::byte{'N'},
    std::byte{'G'},
    std::byte{'F'},
    std::byte{'S'},
};
constexpr std::size_t header_size = 24U;
constexpr std::size_t maximum_path_size = 240U;

bool allowed_path_character(const char value) noexcept {
  return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z') ||
         (value >= '0' && value <= '9') || value == '_' || value == '-' || value == '.' || value == '/';
}

void append_u32(std::vector<std::byte>& output, const std::uint32_t value) {
  for (std::uint32_t shift = 0; shift < 32U; shift += 8U)
    output.push_back(static_cast<std::byte>((value >> shift) & 0xffU));
}

void append_u64(std::vector<std::byte>& output, const std::uint64_t value) {
  for (std::uint32_t shift = 0; shift < 64U; shift += 8U)
    output.push_back(static_cast<std::byte>((value >> shift) & 0xffU));
}

std::uint32_t read_u32(const std::span<const std::byte> input, const std::size_t offset) noexcept {
  std::uint32_t value = 0U;
  for (std::uint32_t index = 0; index < 4U; ++index)
    value |= std::to_integer<std::uint32_t>(input[offset + index]) << (index * 8U);
  return value;
}

std::uint64_t read_u64(const std::span<const std::byte> input, const std::size_t offset) noexcept {
  std::uint64_t value = 0U;
  for (std::uint32_t index = 0; index < 8U; ++index)
    value |= std::to_integer<std::uint64_t>(input[offset + index]) << (index * 8U);
  return value;
}

} // namespace

PathValidation validate_relative_path(const std::string_view candidate) {
  if (candidate.empty())
    return {.issue = PathIssue::empty};
  if (candidate.size() > maximum_path_size)
    return {.issue = PathIssue::too_long};
  if (candidate.front() == '/' || candidate.front() == '\\' ||
      (candidate.size() >= 2U && candidate[1] == ':'))
    return {.issue = PathIssue::absolute};
  if (!std::all_of(candidate.begin(), candidate.end(), allowed_path_character))
    return {.issue = PathIssue::invalid_character};

  std::size_t segment_begin = 0U;
  while (segment_begin <= candidate.size()) {
    const std::size_t separator = candidate.find('/', segment_begin);
    const std::size_t segment_end = separator == std::string_view::npos ? candidate.size() : separator;
    const std::string_view segment = candidate.substr(segment_begin, segment_end - segment_begin);
    if (segment.empty())
      return {.issue = PathIssue::empty_segment};
    if (segment == "." || segment == "..")
      return {.issue = PathIssue::traversal};
    if (separator == std::string_view::npos)
      break;
    segment_begin = separator + 1U;
  }
  return {.path = RelativePath{std::string(candidate)}};
}

std::uint64_t content_digest(const std::span<const std::byte> bytes) noexcept {
  std::uint64_t digest = 14695981039346656037ULL;
  for (const std::byte value : bytes) {
    digest ^= std::to_integer<std::uint8_t>(value);
    digest *= 1099511628211ULL;
  }
  return digest;
}

std::vector<std::byte> encode_envelope(
    const std::uint32_t schema_version, const std::span<const std::byte> payload) {
  std::vector<std::byte> output;
  output.reserve(header_size + payload.size());
  output.insert(output.end(), magic.begin(), magic.end());
  append_u32(output, schema_version);
  append_u64(output, static_cast<std::uint64_t>(payload.size()));
  append_u64(output, content_digest(payload));
  output.insert(output.end(), payload.begin(), payload.end());
  return output;
}

DecodedEnvelope decode_envelope(const std::span<const std::byte> encoded,
    const std::uint32_t supported_schema, const std::size_t maximum_payload) {
  if (encoded.size() < header_size)
    return {.issue = EnvelopeIssue::truncated};
  if (!std::equal(magic.begin(), magic.end(), encoded.begin()))
    return {.issue = EnvelopeIssue::invalid_magic};
  const std::uint32_t schema = read_u32(encoded, 4U);
  if (schema != supported_schema)
    return {.issue = EnvelopeIssue::unsupported_schema, .schema_version = schema};
  const std::uint64_t payload_size = read_u64(encoded, 8U);
  if (payload_size > maximum_payload || payload_size > std::numeric_limits<std::size_t>::max())
    return {.issue = EnvelopeIssue::payload_too_large, .schema_version = schema};
  if (encoded.size() - header_size != static_cast<std::size_t>(payload_size))
    return {.issue = EnvelopeIssue::length_mismatch, .schema_version = schema};
  const auto payload = encoded.subspan(header_size);
  if (content_digest(payload) != read_u64(encoded, 16U))
    return {.issue = EnvelopeIssue::digest_mismatch, .schema_version = schema};
  return {
      .schema_version = schema,
      .payload = std::vector<std::byte>(payload.begin(), payload.end()),
  };
}

ProtectedDocument::ProtectedDocument(Backend& backend, const Scope scope, RelativePath path,
    const std::uint32_t schema, const std::size_t maximum_payload)
    : backend_(backend), scope_(scope), path_(std::move(path)), schema_(schema),
      maximum_payload_(maximum_payload), maximum_encoded_(maximum_payload) {
  if (!validate_relative_path(path_.value).valid() ||
      maximum_payload > std::numeric_limits<std::size_t>::max() - header_size)
    throw std::invalid_argument("invalid protected document path or byte limit");
  maximum_encoded_ += header_size;
}

DocumentLoad ProtectedDocument::load(const std::span<const std::uint32_t> readable_legacy_schemas) {
  writable_ = false;
  decoded_ = false;
  original_.clear();
  auto result = backend_.read(scope_, path_, maximum_encoded_);
  last_read_ = result.issue;
  if (!result.succeeded()) {
    writable_ = result.issue == ReadIssue::not_found;
    return {.read_issue = result.issue};
  }
  original_ = std::move(result.bytes);
  auto envelope = decode_envelope(original_, schema_, maximum_payload_);
  if (envelope.issue == EnvelopeIssue::unsupported_schema && envelope.schema_version < schema_ &&
      std::find(readable_legacy_schemas.begin(), readable_legacy_schemas.end(), envelope.schema_version) != readable_legacy_schemas.end())
    envelope = decode_envelope(original_, envelope.schema_version, maximum_payload_);
  decoded_ = envelope.valid();
  return {.read_issue = ReadIssue::none, .envelope = std::move(envelope)};
}

bool ProtectedDocument::accept_loaded_payload() noexcept {
  writable_ = decoded_;
  return writable_;
}

bool ProtectedDocument::unchanged() {
  const auto current = backend_.read(scope_, path_, maximum_encoded_);
  return (last_read_ == ReadIssue::not_found && current.issue == ReadIssue::not_found) ||
         (last_read_ == ReadIssue::none && current.succeeded() && current.bytes == original_);
}

WriteResult ProtectedDocument::save(const std::span<const std::byte> payload) {
  if (!writable_ || !unchanged()) {
    writable_ = false;
    return {.issue = WriteIssue::protected_data};
  }
  if (payload.size() > maximum_payload_)
    return {.issue = WriteIssue::quota};
  auto encoded = encode_envelope(schema_, payload);
  const auto result = backend_.write_atomically(scope_, path_, encoded);
  if (result.succeeded()) {
    original_ = std::move(encoded);
    last_read_ = ReadIssue::none;
  }
  return result;
}

WriteResult ProtectedDocument::preserve_for_reset(const RelativePath& backup_path) {
  writable_ = false;
  if (last_read_ != ReadIssue::none || !validate_relative_path(backup_path.value).valid() ||
      backup_path.value == path_.value || !unchanged() ||
      backend_.read(scope_, backup_path, maximum_encoded_).issue != ReadIssue::not_found)
    return {.issue = WriteIssue::protected_data};
  const auto written = backend_.write_atomically(scope_, backup_path, original_);
  if (!written.succeeded())
    return written;
  const auto backup = backend_.read(scope_, backup_path, maximum_encoded_);
  if (!backup.succeeded() || backup.bytes != original_ || !unchanged())
    return {.issue = WriteIssue::io_failure};
  writable_ = true;
  return {};
}

WriteResult ProtectedDocument::preserve_original(const RelativePath& backup_path) {
  writable_ = false;
  if (last_read_ != ReadIssue::none || !validate_relative_path(backup_path.value).valid() ||
      backup_path.value == path_.value || !unchanged())
    return {.issue = WriteIssue::protected_data};
  const auto existing = backend_.read(scope_, backup_path, maximum_encoded_);
  if (existing.issue == ReadIssue::not_found) {
    const auto written = backend_.write_atomically(scope_, backup_path, original_);
    if (!written.succeeded())
      return written;
  } else if (!existing.succeeded() || existing.bytes != original_) {
    return {.issue = WriteIssue::protected_data};
  }
  const auto verified = backend_.read(scope_, backup_path, maximum_encoded_);
  if (!verified.succeeded() || verified.bytes != original_ || !unchanged())
    return {.issue = WriteIssue::io_failure};
  writable_ = true;
  return {};
}

} // namespace human_resources::storage
