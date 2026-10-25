#include <human_resources/assets/assets.h>

#include <algorithm>
#include <utility>

namespace human_resources::assets {
namespace {

constexpr std::size_t maximum_id_size = 160U;

bool allowed_id_character(const char value) noexcept {
  return (value >= 'a' && value <= 'z') || (value >= '0' && value <= '9') || value == '_' || value == '-' ||
         value == '.' || value == '/';
}

bool valid_package_path(const std::string_view value) noexcept {
  if (value.empty() || value.front() == '/' || value.back() == '/')
    return false;
  return value.find("..") == std::string_view::npos && value.find('\\') == std::string_view::npos &&
         value.find(':') == std::string_view::npos && value.find('\0') == std::string_view::npos &&
         value.find("//") == std::string_view::npos && value != "." && !value.starts_with("./") &&
         value.find("/./") == std::string_view::npos && !value.ends_with("/.");
}

} // namespace

IdValidation validate_asset_id(const std::string_view candidate) {
  if (candidate.empty())
    return {.issue = IdIssue::empty};
  if (candidate.size() > maximum_id_size)
    return {.issue = IdIssue::too_long};
  if (!std::all_of(candidate.begin(), candidate.end(), allowed_id_character))
    return {.issue = IdIssue::invalid_character};
  if (candidate.front() == '/' || candidate.back() == '/' || candidate.find("//") != std::string_view::npos ||
      candidate.find("../") != std::string_view::npos || candidate == "." || candidate == "..")
    return {.issue = IdIssue::invalid_segment};
  return {.id = AssetId{std::string(candidate)}};
}

std::uint64_t content_digest(const std::span<const std::byte> bytes) noexcept {
  std::uint64_t digest = 14695981039346656037ULL;
  for (const std::byte value : bytes) {
    digest ^= std::to_integer<std::uint8_t>(value);
    digest *= 1099511628211ULL;
  }
  return digest;
}

CatalogValidation validate_catalog(const std::span<const Record> records) {
  for (std::size_t index = 0; index < records.size(); ++index) {
    const Record& record = records[index];
    if (!validate_asset_id(record.id.value).valid())
      return {.issue = CatalogIssue::invalid_id, .index = index};
    if (!valid_package_path(record.package_path))
      return {.issue = CatalogIssue::invalid_path, .index = index};
    if (record.digest == 0U)
      return {.issue = CatalogIssue::invalid_digest, .index = index};
    if (index == 0U)
      continue;
    if (records[index - 1U].id.value == record.id.value)
      return {.issue = CatalogIssue::duplicate_id, .index = index};
    if (records[index - 1U].id.value > record.id.value)
      return {.issue = CatalogIssue::unsorted, .index = index};
  }
  return {};
}

Catalog::Catalog(std::vector<Record> records)
    : records_(std::move(records)), validation_(validate_catalog(records_)) {}

const Record* Catalog::find(const std::string_view id) const noexcept {
  if (!validation_.valid())
    return nullptr;
  const auto found = std::lower_bound(records_.begin(), records_.end(), id,
      [](const Record& record, const std::string_view candidate) { return record.id.value < candidate; });
  return found != records_.end() && found->id.value == id ? &*found : nullptr;
}

} // namespace human_resources::assets
