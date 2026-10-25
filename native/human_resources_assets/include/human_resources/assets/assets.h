#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace human_resources::assets {

struct AssetId final {
  std::string value;
};

enum class IdIssue : std::uint8_t {
  none,
  empty,
  too_long,
  invalid_character,
  invalid_segment,
};

struct IdValidation final {
  IdIssue issue{IdIssue::none};
  AssetId id{};

  [[nodiscard]] bool valid() const noexcept {
    return issue == IdIssue::none;
  }
};

[[nodiscard]] IdValidation validate_asset_id(std::string_view candidate);
[[nodiscard]] std::uint64_t content_digest(std::span<const std::byte> bytes) noexcept;

struct Record final {
  AssetId id;
  std::string package_path;
  std::uint64_t size{};
  std::uint64_t digest{};
};

enum class CatalogIssue : std::uint8_t {
  none,
  invalid_id,
  invalid_path,
  invalid_digest,
  duplicate_id,
  unsorted,
};

struct CatalogValidation final {
  CatalogIssue issue{CatalogIssue::none};
  std::size_t index{};

  [[nodiscard]] bool valid() const noexcept {
    return issue == CatalogIssue::none;
  }
};

[[nodiscard]] CatalogValidation validate_catalog(std::span<const Record> records);

class Catalog final {
public:
  explicit Catalog(std::vector<Record> records);

  [[nodiscard]] const CatalogValidation& validation() const noexcept {
    return validation_;
  }
  [[nodiscard]] const Record* find(std::string_view id) const noexcept;

private:
  std::vector<Record> records_;
  CatalogValidation validation_;
};

enum class LoadIssue : std::uint8_t {
  none,
  not_found,
  unavailable,
  size_mismatch,
  digest_mismatch,
  io_failure,
};

struct LoadResult final {
  LoadIssue issue{LoadIssue::none};
  std::vector<std::byte> bytes{};

  [[nodiscard]] bool succeeded() const noexcept {
    return issue == LoadIssue::none;
  }
};

class Source {
public:
  virtual ~Source() = default;
  [[nodiscard]] virtual LoadResult load(const Record& record) = 0;
};

} // namespace human_resources::assets
