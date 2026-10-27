#pragma once

#include <human_resources/input/input.h>

#include <array>
#include <cstddef>
#include <optional>

namespace human_resources::product {

// Recognizes a short fast stroke of a captured pointer, using logical points
// and native event times. It never delays or modifies joystick movement.
class FlickGesture final {
public:
  static constexpr float minimum_distance = 38.0F;
  static constexpr float minimum_speed = 800.0F; // logical points per second
  static constexpr std::uint64_t window_ns = 120'000'000U;
  static constexpr std::uint64_t release_grace_ns = 50'000'000U;

  void start(const input::Action& action) noexcept;
  [[nodiscard]] std::optional<input::Point> sample(const input::Action& action) noexcept;
  [[nodiscard]] std::optional<input::Point> finish(const input::Action& action) noexcept;
  void cancel() noexcept;
  // Stable through a fast stroke; slow samples, pauses and rearming start a new
  // reference. Kept through finish/cancel so a completed flick can carry its ID.
  [[nodiscard]] std::uint64_t stroke() const noexcept {
    return stroke_;
  }

private:
  [[nodiscard]] std::optional<input::Point> direction() const noexcept;
  void begin_stroke() noexcept;
  struct Sample final {
    input::Point position{};
    std::uint64_t time_ns{};
  };
  std::array<Sample, 32> samples_{};
  std::size_t count_{};
  std::uint64_t last_motion_ns_{};
  std::uint64_t quiet_since_ns_{};
  bool latched_{};
  std::uint64_t stroke_{};
};

} // namespace human_resources::product
