#pragma once
#include <cmath>
#include <human_resources/gameplay/gameplay.h>
#include <optional>

namespace human_resources::product {
struct PromotionFrame final {
  float entry_seconds{-1};
  const gameplay::UpgradeChoice* installed{};
  bool reduced_motion{};
  float installed_seconds{-1}; // Cosmetic receipt age; never advances gameplay.
};
// Disposable presentation timing. The Game applies and saves a selection before
// confirmation starts; the client collects controls while briefly holding advance.
class PromotionFlow final {
public:
  static constexpr float entry_duration = .25F, confirmation_duration = .12F, receipt_duration = .8F;
  void enter() {
    clear();
    entry_seconds_ = 0;
  }
  void confirm(const gameplay::UpgradeChoice& choice) {
    entry_seconds_ = -1;
    installed_ = choice;
    confirmation_seconds_ = 0;
  }
  void advance(float seconds) {
    if (!std::isfinite(seconds) || seconds <= 0)
      return;
    if (entry_seconds_ >= 0) {
      entry_seconds_ += seconds;
      if (entry_seconds_ >= entry_duration)
        entry_seconds_ = -1;
    }
    if (installed_) {
      confirmation_seconds_ += seconds;
      if (confirmation_seconds_ >= receipt_duration)
        installed_.reset();
    }
  }
  void clear() {
    entry_seconds_ = -1;
    installed_.reset();
    confirmation_seconds_ = 0;
  }
  [[nodiscard]] bool holds_combat() const {
    return installed_.has_value() && confirmation_seconds_ < confirmation_duration;
  }
  [[nodiscard]] PromotionFrame frame(bool reduced_motion = false) const {
    return {entry_seconds_, installed_ ? &*installed_ : nullptr, reduced_motion, installed_ ? confirmation_seconds_ : -1};
  }

private:
  float entry_seconds_{-1}, confirmation_seconds_{};
  std::optional<gameplay::UpgradeChoice> installed_;
};
} // namespace human_resources::product
