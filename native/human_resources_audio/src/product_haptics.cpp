#include <human_resources/audio/product_haptics.h>

namespace human_resources::feedback {
void HapticGate::set_policy(const bool enabled, const bool active) noexcept {
  enabled_ = enabled;
  active_ = active;
}
bool HapticGate::accept(const HapticCue cue, const std::uint64_t now_ms) noexcept {
  const auto index = static_cast<unsigned>(cue);
  constexpr std::array<std::uint64_t, 5> intervals{300, 180, 180, 300, 1000};
  if (!enabled_ || !active_ || index >= intervals.size())
    return false;
  if (played_ && now_ms < last_any_)
    return false;
  if (seen_[index] && now_ms - last_[index] < intervals[index])
    return false;
  // Urgent damage/death may replace a capture pulse from the same frame.
  const unsigned priority = index + 1;
  if (played_ && now_ms - last_any_ < 100 && priority <= last_priority_)
    return false;
  seen_[index] = true;
  last_[index] = last_any_ = now_ms;
  last_priority_ = priority;
  played_ = true;
  return true;
}
} // namespace human_resources::feedback
