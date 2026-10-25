#include <human_resources/ui/layout.h>

#include <algorithm>

namespace human_resources::ui {
namespace {

void append_draw_commands(const Node& node, std::vector<DrawCommand>& output) {
  if (node.semantics.hidden)
    return;
  if (node.visual.draws_background && node.clip.width > 0.0F && node.clip.height > 0.0F && node.visual.opacity > 0.0F) {
    Color color = node.visual.background;
    color.alpha *= std::clamp(node.visual.opacity, 0.0F, 1.0F);
    output.push_back({.node_id = node.id, .rect = node.frame, .clip = node.clip, .color = color});
  }
  for (const Node& child : node.children)
    append_draw_commands(child, output);
}

} // namespace

std::vector<DrawCommand> build_draw_list(const Node& root) {
  std::vector<DrawCommand> output;
  append_draw_commands(root, output);
  return output;
}

} // namespace human_resources::ui
