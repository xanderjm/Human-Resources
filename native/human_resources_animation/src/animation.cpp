#include "pose_math.h"
#include <algorithm>
#include <cmath>
#include <human_resources/animation/animation.h>

namespace human_resources::animation {
std::size_t Asset::find_clip(std::string_view name) const {
  for (std::size_t i = 0; i < clips_.size(); ++i)
    if (clips_[i].name == name)
      return i;
  return clips_.size();
}
std::size_t Asset::find_node(std::string_view name) const {
  for (std::size_t i = 0; i < nodes_.size(); ++i)
    if (nodes_[i].name == name)
      return i;
  return nodes_.size();
}
bool Player::bind(const Asset& asset, std::string_view initial) {
  const auto index = asset.find_clip(initial);
  if (asset.nodes().empty() || index == asset.clips().size())
    return false;
  Player next;
  next.asset_ = &asset;
  next.clip_ = index;
  if (!next.evaluate())
    return false;
  *this = next;
  return true;
}
bool Player::play(std::string_view name, float fade, bool restart) {
  if (!asset_ || !std::isfinite(fade) || fade < 0 || fade > 5)
    return false;
  const auto index = asset_->find_clip(name);
  if (index == asset_->clips().size())
    return false;
  if (index == clip_ && !restart)
    return true;
  fade_source_ = pose_.local;
  clip_ = index;
  time_ = fade_time_ = 0;
  fade_seconds_ = fade;
  // A new fade begins with the exact currently visible pose, including any
  // interrupted fade. Avoid even a quaternion renormalization at this boundary.
  return fade > 0 ? true : evaluate();
}
bool Player::seek(float seconds) {
  if (!asset_ || !std::isfinite(seconds) || seconds < 0)
    return false;
  const auto& clip = asset_->clips()[clip_];
  time_ = clip.loop ? std::fmod(seconds, clip.seconds) : std::min(seconds, clip.seconds);
  fade_time_ = fade_seconds_ = 0;
  return evaluate();
}
bool Player::advance(float seconds, float playback_rate) {
  if (!asset_ || !std::isfinite(seconds) || seconds < 0 || seconds > .25F ||
      !std::isfinite(playback_rate) || playback_rate < 0 || playback_rate > 256)
    return false;
  if (seconds == 0)
    return true;
  const auto& clip = asset_->clips()[clip_];
  const float elapsed = seconds * playback_rate;
  time_ = clip.loop ? std::fmod(time_ + elapsed, clip.seconds) : std::min(time_ + elapsed, clip.seconds);
  fade_time_ = std::min(fade_time_ + seconds, fade_seconds_);
  return evaluate();
}
bool Player::evaluate() {
  const auto& clip = asset_->clips()[clip_];
  const float frame = std::clamp(time_ / clip.seconds, 0.F, 1.F) * static_cast<float>(clip.frame_count - 1);
  const auto first = static_cast<std::size_t>(frame);
  const auto last = std::min(first + 1, static_cast<std::size_t>(clip.frame_count - 1));
  const float fraction = frame - static_cast<float>(first);
  float fade = fade_seconds_ > 0 ? std::clamp(fade_time_ / fade_seconds_, 0.F, 1.F) : 1;
  fade = fade * fade * (3 - 2 * fade);
  const auto count = asset_->nodes().size();
  for (std::size_t n = 0; n < count; ++n) {
    const auto sampled = detail::interpolate(clip.frames[first * count + n], clip.frames[last * count + n], fraction);
    pose_.local[n] = fade < 1 ? detail::interpolate(fade_source_[n], sampled, fade) : sampled;
  }
  return detail::compose(*asset_, pose_);
}
const Matrix* Player::node_transform(std::string_view name) const {
  if (!asset_)
    return nullptr;
  const auto index = asset_->find_node(name);
  return index < pose_.node_count ? &pose_.world[index] : nullptr;
}
std::string_view Player::clip_name() const {
  return asset_ ? asset_->clips()[clip_].name : std::string_view{};
}
bool Player::finished() const {
  return asset_ && !asset_->clips()[clip_].loop && time_ >= asset_->clips()[clip_].seconds;
}
bool skin_vertices(const Asset& asset, const Pose& pose, std::span<DeformedVertex> output) {
  if (output.size() != asset.vertices().size() || pose.node_count != asset.nodes().size() ||
      pose.joint_count != asset.joints().size() || pose.joint_count == 0)
    return false;
  for (std::size_t i = 0; i < output.size(); ++i) {
    const auto& vertex = asset.vertices()[i];
    DeformedVertex result{};
    for (std::size_t k = 0; k < 4; ++k) {
      const float w = vertex.weights[k];
      if (w == 0)
        continue;
      const auto& m = pose.palette[vertex.joints[k]];
      const auto& normal = pose.normal_palette[vertex.joints[k]];
      for (std::size_t row = 0; row < 3; ++row) {
        result.position[row] += w * (m[row] * vertex.position[0] + m[4 + row] * vertex.position[1] + m[8 + row] * vertex.position[2] + m[12 + row]);
        result.normal[row] += w * (normal[row] * vertex.normal[0] + normal[3 + row] * vertex.normal[1] + normal[6 + row] * vertex.normal[2]);
      }
    }
    const float length = std::sqrt(result.normal[0] * result.normal[0] + result.normal[1] * result.normal[1] + result.normal[2] * result.normal[2]);
    if (length > 1e-12F)
      for (float& v : result.normal)
        v /= length;
    else
      result.normal = vertex.normal;
    output[i] = result;
  }
  return true;
}
} // namespace human_resources::animation
