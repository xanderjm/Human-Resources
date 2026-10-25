#include <human_resources/assets/assets.h>

#include "test_support.h"

#include <array>
#include <cstddef>
#include <string>
#include <vector>

namespace {

using test_support::require;

human_resources::assets::Record record(const char* id, const char* path, const std::uint64_t digest) {
  return {
      .id = {id},
      .package_path = path,
      .size = 4U,
      .digest = digest,
  };
}

void identifiers_are_stable_and_portable() {
  using namespace human_resources::assets;
  require(validate_asset_id("ui/buttons/confirm").valid(), "namespaced asset ID accepted");
  require(validate_asset_id("UI/Confirm").issue == IdIssue::invalid_character, "case-variant ID rejected");
  require(validate_asset_id("ui/../secret").issue == IdIssue::invalid_segment, "traversal-like ID rejected");
}

void catalogs_are_sorted_unique_and_digest_owned() {
  using namespace human_resources::assets;
  Catalog valid({record("audio/confirm", "audio/confirm.bin", 11U), record("ui/panel", "images/panel.bin", 12U)});
  require(valid.validation().valid(), "valid sorted catalog accepted");
  require(valid.find("ui/panel") != nullptr, "catalog resolves stable asset ID");
  require(valid.find("ui/missing") == nullptr, "missing asset ID fails explicitly");

  const std::vector unsorted{record("ui/panel", "images/panel.bin", 12U),
      record("audio/confirm", "audio/confirm.bin", 11U)};
  require(validate_catalog(unsorted).issue == CatalogIssue::unsorted, "unsorted generated catalog rejected");
  const std::vector invalid{record("ui/panel", "../source.png", 12U)};
  require(validate_catalog(invalid).issue == CatalogIssue::invalid_path, "unsafe package path rejected");
  const std::vector<std::string> unsafe_paths{"/absolute.bin", "C:/absolute.bin", "C:relative.bin", "images\\panel.bin",
      "images//panel.bin", "./panel.bin", "images/./panel.bin", "images/.", std::string("panel.bin\0extra", 15U)};
  for (const std::string& path : unsafe_paths) {
    const std::array records{Record{.id = {"ui/panel"}, .package_path = path, .digest = 12U}};
    require(validate_catalog(records).issue == CatalogIssue::invalid_path, "platform-specific and ambiguous package paths rejected");
  }
}

void content_digests_are_stable() {
  using namespace human_resources::assets;
  const std::array bytes{std::byte{1}, std::byte{2}, std::byte{3}};
  require(content_digest(bytes) == 15035938162879559083ULL, "asset digest has a stable test vector");
}

} // namespace

int main() {
  identifiers_are_stable_and_portable();
  catalogs_are_sorted_unique_and_digest_owned();
  content_digests_are_stable();
  return 0;
}
