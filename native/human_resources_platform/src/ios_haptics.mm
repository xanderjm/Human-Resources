#include "product_haptics_backend.h"

#import <CoreHaptics/CoreHaptics.h>
#import <UIKit/UIKit.h>

namespace human_resources::platform::detail {
namespace {
class IosHaptics final : public HapticBackend {
public:
  IosHaptics() : supported_([CHHapticEngine capabilitiesForHardware].supportsHaptics) {
    if (supported_ && [NSThread isMainThread]) {
      light_ = [[UIImpactFeedbackGenerator alloc] initWithStyle:UIImpactFeedbackStyleLight];
      medium_ = [[UIImpactFeedbackGenerator alloc] initWithStyle:UIImpactFeedbackStyleMedium];
      heavy_ = [[UIImpactFeedbackGenerator alloc] initWithStyle:UIImpactFeedbackStyleHeavy];
    }
  }
  bool available() const noexcept override { return supported_ && light_ != nil; }
  void set_active(const bool active) noexcept override { active_ = active; }
  void play(const feedback::HapticCue cue) noexcept override {
    if (!active_ || !available() || ![NSThread isMainThread] ||
        UIApplication.sharedApplication.applicationState != UIApplicationStateActive)
      return;
    switch (cue) {
    case feedback::HapticCue::capture: [light_ impactOccurredWithIntensity:0.45]; break;
    case feedback::HapticCue::shield_hit: [light_ impactOccurredWithIntensity:0.7]; break;
    case feedback::HapticCue::hull_hit: [medium_ impactOccurredWithIntensity:0.8]; break;
    case feedback::HapticCue::upgrade: [medium_ impactOccurredWithIntensity:0.6]; break;
    case feedback::HapticCue::death: [heavy_ impactOccurredWithIntensity:0.9]; break;
    case feedback::HapticCue::count: break;
    }
  }

private:
  UIImpactFeedbackGenerator* __strong light_{};
  UIImpactFeedbackGenerator* __strong medium_{};
  UIImpactFeedbackGenerator* __strong heavy_{};
  bool supported_{}, active_{};
};
} // namespace
std::unique_ptr<HapticBackend> create_haptic_backend() { return std::make_unique<IosHaptics>(); }
} // namespace human_resources::platform::detail
