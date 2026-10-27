#include <human_resources/product/flick_gesture.h>

#include <algorithm>
#include <cmath>

namespace human_resources::product {
namespace {

bool valid(const input::Action& action) noexcept {
  return action.has_position && action.timestamp_ns != 0U &&
         std::isfinite(action.position.x) && std::isfinite(action.position.y) && std::isfinite(action.value);
}

} // namespace

void FlickGesture::cancel() noexcept {
  count_ = 0U;
  last_motion_ns_ = 0U;
  quiet_since_ns_ = 0U;
  latched_ = false;
}

void FlickGesture::begin_stroke() noexcept {
  if (++stroke_ == 0)
    ++stroke_;
}

void FlickGesture::start(const input::Action& action) noexcept {
  cancel();
  if (valid(action)) {
    begin_stroke();
    samples_[0] = {action.position, action.timestamp_ns};
    count_ = 1U;
  }
}

std::optional<input::Point> FlickGesture::sample(const input::Action& action) noexcept {
  if (count_ == 0U)
    return std::nullopt;
  const auto previous = samples_[count_ - 1U];
  if (!valid(action) || action.timestamp_ns < previous.time_ns) {
    cancel();
    return std::nullopt;
  }
  const float distance = std::hypot(action.position.x - previous.position.x, action.position.y - previous.position.y);
  if (!std::isfinite(distance) || (action.timestamp_ns == previous.time_ns && distance > 0.0F)) {
    cancel();
    return std::nullopt;
  }
  if (action.timestamp_ns == previous.time_ns)
    return std::nullopt;
  const auto duration = action.timestamp_ns - previous.time_ns;
  constexpr float rearm_speed = 200.0F;
  if (latched_) {
    constexpr std::uint64_t rearm_ns = 80'000'000U;
    if (distance * 1.0e9F / static_cast<float>(duration) < rearm_speed) {
      if (quiet_since_ns_ == 0U)
        quiet_since_ns_ = previous.time_ns;
    } else {
      quiet_since_ns_ = 0U;
    }
    // A stationary finger may produce no events. A full history-window gap
    // starts a fresh stroke; otherwise require sustained slow movement.
    if (duration >= window_ns ||
        (quiet_since_ns_ != 0U && action.timestamp_ns - quiet_since_ns_ >= rearm_ns)) {
      latched_ = false;
      begin_stroke();
      quiet_since_ns_ = 0U;
      samples_[0] = {action.position, action.timestamp_ns};
      count_ = 1U;
      last_motion_ns_ = 0U;
      return std::nullopt;
    }
  }
  // Before the first accepted flick, ordinary slow steering or a quiet gap
  // refreshes its facing reference. Fast samples keep the reference that was
  // established before those samples started rotating the character.
  if (!latched_ && (duration >= window_ns || distance * 1.0e9F / static_cast<float>(duration) < rearm_speed))
    begin_stroke();
  if (distance >= 1.0F)
    last_motion_ns_ = action.timestamp_ns;
  if (count_ == samples_.size()) {
    std::move(samples_.begin() + 1, samples_.end(), samples_.begin());
    --count_;
  }
  samples_[count_++] = {action.position, action.timestamp_ns};
  const auto result = latched_ ? std::nullopt : direction();
  if (result)
    latched_ = true;
  return result;
}

std::optional<input::Point> FlickGesture::finish(const input::Action& action) noexcept {
  const auto result = sample(action);
  cancel();
  return result;
}

std::optional<input::Point> FlickGesture::direction() const noexcept {
  std::optional<input::Point> result;
  if (count_ >= 2U && last_motion_ns_ != 0U && samples_[count_ - 1U].time_ns - last_motion_ns_ <= release_grace_ns) {
    const auto end = samples_[count_ - 1U];
    float path = 0.0F;
    for (std::size_t index = count_ - 1U; index > 0U; --index) {
      const auto from = samples_[index - 1U];
      const auto to = samples_[index];
      const auto duration = end.time_ns - from.time_ns;
      if (duration > window_ns)
        break;
      path += std::hypot(to.position.x - from.position.x, to.position.y - from.position.y);
      const input::Point delta{end.position.x - from.position.x, end.position.y - from.position.y};
      const float distance = std::hypot(delta.x, delta.y);
      // Reject tiny lift jitter, slow drags and curved/backtracking strokes.
      if (duration >= 10'000'000U && distance >= minimum_distance && distance >= path * 0.85F &&
          distance * 1.0e9F / static_cast<float>(duration) >= minimum_speed) {
        result = input::Point{delta.x / distance, delta.y / distance};
        break;
      }
    }
  }
  return result;
}

} // namespace human_resources::product
