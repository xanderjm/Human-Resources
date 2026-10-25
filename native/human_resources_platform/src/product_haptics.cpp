#include <human_resources/platform/product_haptics.h>
#include "product_haptics_backend.h"

namespace human_resources::platform {
ProductHaptics::ProductHaptics() = default;
ProductHaptics::~ProductHaptics() {
  if (backend_)
    backend_->set_active(false);
}
bool ProductHaptics::initialize() noexcept {
  if (!backend_) {
    try {
      backend_ = detail::create_haptic_backend();
    } catch (...) {
      return false;
    }
    if (backend_)
      backend_->set_active(enabled_ && active_);
  }
  return available();
}
bool ProductHaptics::available() const noexcept {
  return backend_ && backend_->available();
}
void ProductHaptics::set_policy(const bool enabled, const bool active) noexcept {
  gate_.set_policy(enabled, active);
  if (backend_ && (enabled != enabled_ || active != active_))
    backend_->set_active(enabled && active);
  enabled_ = enabled;
  active_ = active;
}
void ProductHaptics::play(const feedback::HapticCue cue, const std::uint64_t now_ms) noexcept {
  if (available() && gate_.accept(cue, now_ms))
    backend_->play(cue);
}
} // namespace human_resources::platform
