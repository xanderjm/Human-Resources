#include <human_resources/animation/animation.h>
#include "pose_math.h"

namespace human_resources::animation {
bool normalize_capture_root(const Asset& asset, const Pose& base, Pose& output) {
  const auto root = asset.find_node("root");
  if (base.node_count != asset.nodes().size() || root >= asset.nodes().size()) return false;
  Pose candidate = base;
  candidate.local[root].translation = asset.nodes()[root].rest.translation;
  if (!detail::compose(asset, candidate)) return false;
  output = candidate;
  return true;
}
} // namespace human_resources::animation
