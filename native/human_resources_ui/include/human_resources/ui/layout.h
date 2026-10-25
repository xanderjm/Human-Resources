#pragma once

#include <human_resources/ui/design_tokens.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace human_resources::ui {

using NodeId = std::uint32_t;
inline constexpr NodeId invalid_node_id = 0;

enum class LayoutKind : std::uint8_t {
  leaf,
  row,
  column,
  stack,
  grid,
  scroll_list,
};

enum class DimensionKind : std::uint8_t {
  automatic,
  points,
  percent,
  weight,
};

struct Dimension final {
  DimensionKind kind{DimensionKind::automatic};
  float value{};

  [[nodiscard]] static constexpr Dimension automatic() noexcept {
    return {};
  }
  [[nodiscard]] static constexpr Dimension points(const float value) noexcept {
    return {.kind = DimensionKind::points, .value = value};
  }
  [[nodiscard]] static constexpr Dimension percent(const float value) noexcept {
    return {.kind = DimensionKind::percent, .value = value};
  }
  [[nodiscard]] static constexpr Dimension weight(const float value = 1.0F) noexcept {
    return {.kind = DimensionKind::weight, .value = value};
  }
};

enum class Alignment : std::uint8_t {
  start,
  center,
  end,
  stretch,
};

enum class MainAlignment : std::uint8_t {
  start,
  center,
  end,
  space_between,
};

enum class TextDirection : std::uint8_t {
  left_to_right,
  right_to_left,
};

enum class SemanticRole : std::uint8_t {
  none,
  group,
  button,
  label,
  image,
  slider,
  toggle,
  list,
  list_item,
  dialog,
};

struct LayoutStyle final {
  LayoutKind kind{LayoutKind::leaf};
  Dimension width{};
  Dimension height{};
  float min_width{};
  float min_height{};
  float max_width{1.0e7F};
  float max_height{1.0e7F};
  float aspect_ratio{};
  Insets padding{};
  float gap{};
  MainAlignment main_alignment{MainAlignment::start};
  Alignment cross_alignment{Alignment::stretch};
  std::uint16_t grid_columns{1};
  float scroll_offset{};
  bool clip_content{};
};

struct VisualStyle final {
  Color background{};
  bool draws_background{};
  float opacity{1.0F};
};

struct Semantics final {
  SemanticRole role{SemanticRole::none};
  std::string label;
  std::string value;
  bool focusable{};
  bool disabled{};
  bool hidden{};
  // Product identity survives rebuilt/reordered trees; empty uses the stable NodeId.
  std::string stable_id;
  bool checked{};
  bool selected{};
};

struct TextContent final {
  std::string value;
  float font_size{16.0F};
  Insets insets{};
};

class TextMeasurer {
public:
  virtual ~TextMeasurer() = default;
  [[nodiscard]] virtual Size measure(std::string_view text, float font_size, float maximum_width,
      float text_scale) const noexcept = 0;
};

struct Node final {
  NodeId id{invalid_node_id};
  LayoutStyle layout;
  VisualStyle visual;
  Semantics semantics;
  std::optional<TextContent> text;
  Size intrinsic_size{};
  std::vector<Node> children;

  // Outputs owned by the layout pass.
  Rect frame{};
  Rect clip{};
};

struct LayoutContext final {
  TextDirection text_direction{TextDirection::left_to_right};
  float text_scale{1.0F};
  bool reduced_motion{};
  Size minimum_interaction_target{44.0F, 44.0F};
  const TextMeasurer* text_measurer{};
};

enum class LayoutIssueCode : std::uint8_t {
  invalid_viewport,
  invalid_safe_area,
  invalid_dimension,
  invalid_constraint,
  invalid_spacing,
  invalid_grid,
  invalid_visual,
  invalid_content,
  invalid_context,
  duplicate_node_id,
  missing_node_id,
  duplicate_semantic_id,
};

struct LayoutIssue final {
  LayoutIssueCode code{};
  NodeId node_id{};
};

enum class OverflowIssueCode : std::uint8_t {
  text_clipped,
  child_clipped,
};

struct OverflowIssue final {
  OverflowIssueCode code{};
  NodeId node_id{};
  Size required{};
  Size available{};
};

struct LayoutResult final {
  bool valid{};
  std::size_t node_count{};
  Rect safe_content{};
  std::vector<LayoutIssue> issues;
  // Clipping is a product policy decision, distinct from structural validity.
  std::vector<OverflowIssue> overflows;
};

struct DrawCommand final {
  NodeId node_id{};
  Rect rect{};
  Rect clip{};
  Color color{};
};

struct AccessibilityNode final {
  NodeId id{};
  std::string stable_id;
  NodeId parent_id{};
  SemanticRole role{SemanticRole::none};
  std::string label{};
  std::string value{};
  Rect frame{};
  bool focusable{};
  bool disabled{};
  bool checked{};
  bool selected{};
};

struct InteractionTargetViolation final {
  NodeId node_id{};
  Size actual{};
  Size required{};
};

[[nodiscard]] LayoutResult layout(Node& root, Rect viewport, Insets safe_area, const LayoutContext& context);
[[nodiscard]] std::optional<NodeId> hit_test(const Node& root, Point point) noexcept;
[[nodiscard]] std::vector<NodeId> focus_order(const Node& root);
[[nodiscard]] std::vector<AccessibilityNode> accessibility_tree(const Node& root);
[[nodiscard]] std::vector<InteractionTargetViolation> validate_interaction_targets(const Node& root, Size minimum_interaction_target);
[[nodiscard]] std::vector<DrawCommand> build_draw_list(const Node& root);

namespace components {

[[nodiscard]] Node screen(NodeId id, const DesignTokens& tokens);
[[nodiscard]] Node panel(NodeId id, LayoutKind kind, const DesignTokens& tokens);
[[nodiscard]] Node button(NodeId id, std::string label, const DesignTokens& tokens);
[[nodiscard]] Node label(NodeId id, std::string text, const DesignTokens& tokens);

} // namespace components

} // namespace human_resources::ui
