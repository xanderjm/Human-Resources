#include <human_resources/ui/layout.h>

namespace human_resources::ui {
namespace {

constexpr float epsilon = 0.0001F;

std::optional<NodeId> hit_test_node(const Node& node, const Point point) noexcept {
  if (node.semantics.hidden || !node.clip.contains(point) || !node.frame.contains(point)) {
    return std::nullopt;
  }
  for (auto iterator = node.children.rbegin(); iterator != node.children.rend(); ++iterator) {
    if (const auto child = hit_test_node(*iterator, point); child.has_value())
      return child;
  }
  if (!node.semantics.disabled &&
      (node.semantics.focusable || node.semantics.role == SemanticRole::button ||
          node.semantics.role == SemanticRole::toggle || node.semantics.role == SemanticRole::slider)) {
    return node.id;
  }
  return std::nullopt;
}

void append_focus_order(const Node& node, std::vector<NodeId>& output) {
  if (node.semantics.hidden)
    return;
  if (node.semantics.focusable && !node.semantics.disabled)
    output.push_back(node.id);
  for (const Node& child : node.children)
    append_focus_order(child, output);
}

void append_accessibility(const Node& node, const NodeId semantic_parent, std::vector<AccessibilityNode>& output) {
  if (node.semantics.hidden)
    return;
  NodeId child_parent = semantic_parent;
  if (node.semantics.role != SemanticRole::none) {
    output.push_back({.id = node.id,
        .stable_id = node.semantics.stable_id.empty() ? std::to_string(node.id) : node.semantics.stable_id,
        .parent_id = semantic_parent,
        .role = node.semantics.role,
        .label = node.semantics.label,
        .value = node.semantics.value,
        .frame = intersect(node.frame, node.clip),
        .focusable = node.semantics.focusable,
        .disabled = node.semantics.disabled,
        .checked = node.semantics.checked,
        .selected = node.semantics.selected});
    child_parent = node.id;
  }
  for (const Node& child : node.children)
    append_accessibility(child, child_parent, output);
}

void append_interaction_violations(
    const Node& node, const Size required, std::vector<InteractionTargetViolation>& output) {
  if (node.semantics.hidden)
    return;
  if ((node.semantics.focusable || node.semantics.role == SemanticRole::button ||
          node.semantics.role == SemanticRole::toggle || node.semantics.role == SemanticRole::slider) &&
      (node.frame.width + epsilon < required.width || node.frame.height + epsilon < required.height)) {
    output.push_back({.node_id = node.id, .actual = {node.frame.width, node.frame.height}, .required = required});
  }
  for (const Node& child : node.children)
    append_interaction_violations(child, required, output);
}

} // namespace

std::optional<NodeId> hit_test(const Node& root, const Point point) noexcept {
  return hit_test_node(root, point);
}

std::vector<NodeId> focus_order(const Node& root) {
  std::vector<NodeId> output;
  append_focus_order(root, output);
  return output;
}

std::vector<AccessibilityNode> accessibility_tree(const Node& root) {
  std::vector<AccessibilityNode> output;
  append_accessibility(root, invalid_node_id, output);
  return output;
}

std::vector<InteractionTargetViolation> validate_interaction_targets(const Node& root, const Size minimum_interaction_target) {
  std::vector<InteractionTargetViolation> output;
  append_interaction_violations(root, minimum_interaction_target, output);
  return output;
}

} // namespace human_resources::ui
