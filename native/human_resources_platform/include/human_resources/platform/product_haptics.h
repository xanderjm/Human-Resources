#pragma once

#include <human_resources/audio/product_haptics.h>
#include <memory>

namespace human_resources::platform {
namespace detail { class HapticBackend; }

// Main-thread optional mobile feedback. initialize() runs after SDL/window
// setup. No request is queued by this owner; platform UI dispatch is bounded.
class ProductHaptics final {
public:
  ProductHaptics();
  ~ProductHaptics();
  ProductHaptics(const ProductHaptics&) = delete;
  ProductHaptics& operator=(const ProductHaptics&) = delete;
  [[nodiscard]] bool initialize() noexcept;
  [[nodiscard]] bool available() const noexcept;
  void set_policy(bool enabled, bool active) noexcept;
  void play(feedback::HapticCue cue, std::uint64_t now_ms) noexcept;

private:
  std::unique_ptr<detail::HapticBackend> backend_;
  feedback::HapticGate gate_;
  bool enabled_{true}, active_{};
};
} // namespace human_resources::platform
