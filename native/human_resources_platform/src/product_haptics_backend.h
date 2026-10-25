#pragma once

#include <human_resources/audio/product_haptics.h>
#include <memory>

namespace human_resources::platform::detail {
class HapticBackend {
public:
  virtual ~HapticBackend() = default;
  [[nodiscard]] virtual bool available() const noexcept = 0;
  virtual void set_active(bool active) noexcept = 0;
  virtual void play(feedback::HapticCue cue) noexcept = 0;
};
[[nodiscard]] std::unique_ptr<HapticBackend> create_haptic_backend();
} // namespace human_resources::platform::detail
