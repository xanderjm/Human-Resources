#include <human_resources/ui/layout.h>

#include <utility>

namespace human_resources::ui::components {

Node screen(const NodeId id, const DesignTokens& tokens) {
  Node node;
  node.id = id;
  node.layout.kind = LayoutKind::column;
  node.layout.padding = {tokens.spacing[4], tokens.spacing[4], tokens.spacing[4], tokens.spacing[4]};
  node.layout.gap = tokens.spacing[3];
  node.layout.clip_content = true;
  node.visual = {.background = tokens.canvas, .draws_background = true};
  node.semantics.role = SemanticRole::group;
  return node;
}

Node panel(const NodeId id, const LayoutKind kind, const DesignTokens& tokens) {
  Node node;
  node.id = id;
  node.layout.kind = kind;
  node.layout.padding = {tokens.spacing[4], tokens.spacing[4], tokens.spacing[4], tokens.spacing[4]};
  node.layout.gap = tokens.spacing[3];
  node.layout.clip_content = true;
  node.visual = {.background = tokens.surface, .draws_background = true};
  node.semantics.role = SemanticRole::group;
  return node;
}

Node button(const NodeId id, std::string label, const DesignTokens& tokens) {
  Node node;
  node.id = id;
  node.layout.kind = LayoutKind::leaf;
  node.layout.width = Dimension::automatic();
  node.layout.height = Dimension::points(tokens.minimum_interaction_target.height);
  node.layout.min_width = tokens.minimum_interaction_target.width;
  node.layout.min_height = tokens.minimum_interaction_target.height;
  node.intrinsic_size = {120.0F, tokens.minimum_interaction_target.height};
  node.visual = {.background = tokens.action, .draws_background = true};
  node.semantics.role = SemanticRole::button;
  node.semantics.label = std::move(label);
  node.semantics.focusable = true;
  return node;
}

Node label(const NodeId id, std::string text, const DesignTokens& tokens) {
  Node node;
  node.id = id;
  node.layout.kind = LayoutKind::leaf;
  node.text = TextContent{.value = text, .font_size = tokens.typography.body};
  node.intrinsic_size = {80.0F, tokens.typography.body * 1.35F};
  node.semantics.role = SemanticRole::label;
  node.semantics.label = std::move(text);
  return node;
}

} // namespace human_resources::ui::components
