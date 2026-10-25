#pragma once

#include <array>
#include <cstdint>

namespace human_resources::feedback {
enum class HapticCue : std::uint8_t { capture, shield_hit, hull_hit, upgrade, death, count };

// Allocation-free, single-threaded policy. Time is monotonic milliseconds;
// rejected/paused requests are discarded, never deferred to a later screen.
class HapticGate final {
public:
  void set_policy(bool enabled, bool active) noexcept;
  [[nodiscard]] bool accept(HapticCue cue, std::uint64_t now_ms) noexcept;

private:
  std::array<std::uint64_t, static_cast<unsigned>(HapticCue::count)> last_{};
  std::array<bool, static_cast<unsigned>(HapticCue::count)> seen_{};
  std::uint64_t last_any_{};
  unsigned last_priority_{};
  bool enabled_{true}, active_{}, played_{};
};
} // namespace human_resources::feedback
