#pragma once
#include <human_resources/storage/storage.h>
#include <map>
#include <string>
#include <vector>

namespace progression_tests {
class MemoryBackend final : public human_resources::storage::Backend {
public:
  std::map<std::string, std::vector<std::byte>> files;
  human_resources::storage::ReadIssue read_failure{human_resources::storage::ReadIssue::none};
  human_resources::storage::WriteIssue write_failure{human_resources::storage::WriteIssue::none};
  std::size_t writes{};
  human_resources::storage::ReadResult read(human_resources::storage::Scope,
      const human_resources::storage::RelativePath& path, const std::size_t limit) override {
    using namespace human_resources::storage;
    if (read_failure != ReadIssue::none) return {.issue = read_failure};
    const auto file = files.find(path.value);
    if (file == files.end()) return {.issue = ReadIssue::not_found};
    if (file->second.size() > limit) return {.issue = ReadIssue::too_large};
    return {.bytes = file->second};
  }
  human_resources::storage::WriteResult write_atomically(human_resources::storage::Scope,
      const human_resources::storage::RelativePath& path, std::span<const std::byte> bytes) override {
    ++writes;
    if (write_failure != human_resources::storage::WriteIssue::none) return {.issue = write_failure};
    files[path.value] = {bytes.begin(), bytes.end()};
    return {};
  }
};
}
