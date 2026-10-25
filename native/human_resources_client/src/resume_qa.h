#pragma once
#include <cstdint>
#include <human_resources/product/presentation.h>
#include <string>
#include <string_view>
#include <vector>

namespace human_resources::client {
// Explicit diagnostic commands only; save creation/loading/checkpointing remain
// with ordinary composition. The Android adapter ignores these flags in release.
class ResumeQa final {
public:
  [[nodiscard]] bool configure(std::string_view mode, std::string_view slot);
  [[nodiscard]] bool active() const noexcept {
    return mode_ != Mode::none;
  }
  [[nodiscard]] std::string_view slot() const noexcept {
    return slot_;
  }
  [[nodiscard]] product::Command next_command(product::Screen screen, bool save_error, bool restored_run);
  [[nodiscard]] float movement_x(double active_seconds) const noexcept;
  [[nodiscard]] bool take_hold_marker(product::Screen screen) noexcept;

private:
  enum class Mode { none,
    prepare_choice,
    inspect,
    resume,
    bank };
  Mode mode_{Mode::none};
  std::string slot_;
  bool dispatched_{}, held_{};
};
[[nodiscard]] std::vector<std::string> resume_log_lines(const gameplay::Snapshot& snapshot,
    std::uint64_t run_id, std::uint8_t location, std::uint64_t stored_digest);
} // namespace human_resources::client
