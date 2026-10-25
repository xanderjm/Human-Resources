#include <human_resources/ui/layout.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_set>

namespace human_resources::ui {
namespace {

constexpr float epsilon = 0.0001F;

float clamp_size(const float value, const float minimum, const float maximum) noexcept {
  return std::clamp(std::max(0.0F, value), std::max(0.0F, minimum), std::max(minimum, maximum));
}

bool valid_dimension(const Dimension dimension) noexcept {
  if (!std::isfinite(dimension.value)) {
    return false;
  }
  switch (dimension.kind) {
  case DimensionKind::automatic:
    return dimension.value == 0.0F;
  case DimensionKind::points:
    return dimension.value >= 0.0F;
  case DimensionKind::percent:
    return dimension.value >= 0.0F && dimension.value <= 1.0F;
  case DimensionKind::weight:
    return dimension.value > 0.0F;
  }
  return false;
}

float resolve_dimension(const Dimension dimension, const float available, const float natural, const float minimum,
    const float maximum) noexcept {
  float value = natural;
  switch (dimension.kind) {
  case DimensionKind::automatic:
    value = natural;
    break;
  case DimensionKind::points:
    value = dimension.value;
    break;
  case DimensionKind::percent:
    value = available * dimension.value;
    break;
  case DimensionKind::weight:
    value = natural;
    break;
  }
  return clamp_size(value, minimum, maximum);
}

Size natural_size(const Node& node, Size available, const LayoutContext& context);

Size leaf_natural_size(const Node& node, const Size available, const LayoutContext& context) {
  if (node.text.has_value() && context.text_measurer != nullptr) {
    const auto padding = node.text->insets;
    const auto measured = context.text_measurer->measure(node.text->value, node.text->font_size,
        std::max(0.0F, available.width - padding.left - padding.right), context.text_scale);
    return {measured.width + padding.left + padding.right,
        measured.height + padding.top + padding.bottom};
  }
  return node.intrinsic_size;
}

Size container_natural_size(const Node& node, const Size available, const LayoutContext& context) {
  const float horizontal_padding = node.layout.padding.left + node.layout.padding.right;
  const float vertical_padding = node.layout.padding.top + node.layout.padding.bottom;
  const Size inner_available{
      .width = std::max(0.0F, available.width - horizontal_padding),
      .height = std::max(0.0F, available.height - vertical_padding),
  };
  if (node.children.empty()) {
    return {horizontal_padding, vertical_padding};
  }

  float width = 0.0F;
  float height = 0.0F;
  if (node.layout.kind == LayoutKind::row) {
    for (const Node& child : node.children) {
      const Size child_size = natural_size(child, inner_available, context);
      width += child_size.width;
      height = std::max(height, child_size.height);
    }
    width += node.layout.gap * static_cast<float>(node.children.size() - 1U);
  } else if (node.layout.kind == LayoutKind::column || node.layout.kind == LayoutKind::scroll_list) {
    for (const Node& child : node.children) {
      const Size child_size = natural_size(child, inner_available, context);
      width = std::max(width, child_size.width);
      height += child_size.height;
    }
    height += node.layout.gap * static_cast<float>(node.children.size() - 1U);
  } else if (node.layout.kind == LayoutKind::stack) {
    for (const Node& child : node.children) {
      const Size child_size = natural_size(child, inner_available, context);
      width = std::max(width, child_size.width);
      height = std::max(height, child_size.height);
    }
  } else if (node.layout.kind == LayoutKind::grid) {
    const std::size_t columns = std::max<std::size_t>(1U, node.layout.grid_columns);
    std::vector<float> row_heights((node.children.size() + columns - 1U) / columns, 0.0F);
    const float gaps = node.layout.gap * static_cast<float>(columns - 1U);
    const float cell_width = std::max(0.0F, inner_available.width - gaps) / static_cast<float>(columns);
    float maximum_cell_width = 0.0F;
    for (std::size_t index = 0; index < node.children.size(); ++index) {
      const Size child_size = natural_size(node.children[index], {cell_width, inner_available.height}, context);
      maximum_cell_width = std::max(maximum_cell_width, child_size.width);
      row_heights[index / columns] = std::max(row_heights[index / columns], child_size.height);
    }
    width = maximum_cell_width * static_cast<float>(columns) + node.layout.gap * static_cast<float>(columns - 1U);
    for (const float row_height : row_heights) {
      height += row_height;
    }
    if (!row_heights.empty()) {
      height += node.layout.gap * static_cast<float>(row_heights.size() - 1U);
    }
  }
  return {.width = width + horizontal_padding, .height = height + vertical_padding};
}

Size natural_size(const Node& node, const Size available, const LayoutContext& context) {
  const float measured_width = resolve_dimension(node.layout.width, available.width,
      available.width, node.layout.min_width, node.layout.max_width);
  const Size constrained{measured_width, available.height};
  const Size natural = node.layout.kind == LayoutKind::leaf ? leaf_natural_size(node, constrained, context)
                                                            : container_natural_size(node, constrained, context);
  float width = resolve_dimension(node.layout.width, available.width, natural.width, node.layout.min_width,
      node.layout.max_width);
  float height = resolve_dimension(node.layout.height, available.height, natural.height, node.layout.min_height,
      node.layout.max_height);
  if (node.layout.aspect_ratio > 0.0F) {
    if (node.layout.width.kind != DimensionKind::automatic && node.layout.height.kind == DimensionKind::automatic) {
      height = clamp_size(width / node.layout.aspect_ratio, node.layout.min_height, node.layout.max_height);
    } else if (node.layout.height.kind != DimensionKind::automatic &&
               node.layout.width.kind == DimensionKind::automatic) {
      width = clamp_size(height * node.layout.aspect_ratio, node.layout.min_width, node.layout.max_width);
    }
  }
  return {width, height};
}

void validate_node(const Node& node, std::unordered_set<NodeId>& identifiers,
    std::unordered_set<std::string>& semantic_ids, LayoutResult& result) {
  ++result.node_count;
  if (node.id == invalid_node_id) {
    result.issues.push_back({.code = LayoutIssueCode::missing_node_id, .node_id = node.id});
  } else if (!identifiers.insert(node.id).second) {
    result.issues.push_back({.code = LayoutIssueCode::duplicate_node_id, .node_id = node.id});
  }
  if (node.semantics.role != SemanticRole::none) {
    const std::string identity = node.semantics.stable_id.empty()
                                     ? std::to_string(node.id)
                                     : node.semantics.stable_id;
    if (!semantic_ids.insert(identity).second)
      result.issues.push_back({.code = LayoutIssueCode::duplicate_semantic_id, .node_id = node.id});
  }
  if (node.text.has_value()) {
    const auto insets = node.text->insets;
    for (const float value : {insets.left, insets.top, insets.right, insets.bottom}) {
      if (!std::isfinite(value) || value < 0.0F)
        result.issues.push_back({.code = LayoutIssueCode::invalid_spacing, .node_id = node.id});
    }
  }
  if (!valid_dimension(node.layout.width) || !valid_dimension(node.layout.height)) {
    result.issues.push_back({.code = LayoutIssueCode::invalid_dimension, .node_id = node.id});
  }
  if (!std::isfinite(node.layout.min_width) || !std::isfinite(node.layout.min_height) ||
      !std::isfinite(node.layout.max_width) || !std::isfinite(node.layout.max_height) || node.layout.min_width < 0.0F ||
      node.layout.min_height < 0.0F || node.layout.max_width < node.layout.min_width ||
      node.layout.max_height < node.layout.min_height || !std::isfinite(node.layout.aspect_ratio) ||
      node.layout.aspect_ratio < 0.0F) {
    result.issues.push_back({.code = LayoutIssueCode::invalid_constraint, .node_id = node.id});
  }
  const Insets padding = node.layout.padding;
  if (!std::isfinite(node.layout.gap) || node.layout.gap < 0.0F || !std::isfinite(node.layout.scroll_offset) ||
      node.layout.scroll_offset < 0.0F || !std::isfinite(padding.left) || !std::isfinite(padding.top) ||
      !std::isfinite(padding.right) || !std::isfinite(padding.bottom) || padding.left < 0.0F || padding.top < 0.0F ||
      padding.right < 0.0F || padding.bottom < 0.0F) {
    result.issues.push_back({.code = LayoutIssueCode::invalid_spacing, .node_id = node.id});
  }
  if (node.layout.kind == LayoutKind::grid && node.layout.grid_columns == 0U) {
    result.issues.push_back({.code = LayoutIssueCode::invalid_grid, .node_id = node.id});
  }
  const Color color = node.visual.background;
  if (!std::isfinite(color.red) || !std::isfinite(color.green) || !std::isfinite(color.blue) ||
      !std::isfinite(color.alpha) || !std::isfinite(node.visual.opacity) || color.red < 0.0F || color.red > 1.0F ||
      color.green < 0.0F || color.green > 1.0F || color.blue < 0.0F || color.blue > 1.0F || color.alpha < 0.0F ||
      color.alpha > 1.0F || node.visual.opacity < 0.0F || node.visual.opacity > 1.0F) {
    result.issues.push_back({.code = LayoutIssueCode::invalid_visual, .node_id = node.id});
  }
  if (!std::isfinite(node.intrinsic_size.width) || !std::isfinite(node.intrinsic_size.height) ||
      node.intrinsic_size.width < 0.0F || node.intrinsic_size.height < 0.0F ||
      (node.text.has_value() && (!std::isfinite(node.text->font_size) || node.text->font_size <= 0.0F))) {
    result.issues.push_back({.code = LayoutIssueCode::invalid_content, .node_id = node.id});
  }
  for (const Node& child : node.children) {
    validate_node(child, identifiers, semantic_ids, result);
  }
}

float cross_position(const float start, const float available, const float child, const Alignment alignment,
    const bool mirrored) noexcept {
  Alignment effective = alignment;
  if (mirrored) {
    if (alignment == Alignment::start)
      effective = Alignment::end;
    if (alignment == Alignment::end)
      effective = Alignment::start;
  }
  switch (effective) {
  case Alignment::start:
  case Alignment::stretch:
    return start;
  case Alignment::center:
    return start + (available - child) * 0.5F;
  case Alignment::end:
    return start + available - child;
  }
  return start;
}

struct LinearItem final {
  Node* node{};
  Size natural{};
  float main{};
  float cross{};
  float minimum_main{};
  float maximum_main{};
  float weight{};
  bool flexible{};
};

void distribute_flexible_space(std::vector<LinearItem>& items, float remaining) {
  std::vector<bool> active(items.size(), false);
  for (std::size_t index = 0; index < items.size(); ++index) {
    active[index] = items[index].flexible;
  }
  while (remaining > epsilon) {
    float total_weight = 0.0F;
    for (std::size_t index = 0; index < items.size(); ++index) {
      if (active[index])
        total_weight += items[index].weight;
    }
    if (total_weight <= epsilon)
      break;

    bool saturated = false;
    for (std::size_t index = 0; index < items.size(); ++index) {
      if (!active[index])
        continue;
      LinearItem& item = items[index];
      const float share = remaining * item.weight / total_weight;
      const float capacity = std::max(0.0F, item.maximum_main - item.main);
      if (share + epsilon >= capacity) {
        item.main += capacity;
        remaining -= capacity;
        active[index] = false;
        saturated = true;
      }
    }
    if (saturated)
      continue;
    for (std::size_t index = 0; index < items.size(); ++index) {
      if (active[index]) {
        items[index].main += remaining * items[index].weight / total_weight;
      }
    }
    remaining = 0.0F;
  }
}

void arrange_node(Node& node, Rect rect, Rect inherited_clip, const LayoutContext& context);

void arrange_linear(Node& node, const Rect inner, const Rect child_clip, const LayoutContext& context,
    const bool horizontal, const bool scroll) {
  if (node.children.empty())
    return;
  std::vector<LinearItem> items;
  items.reserve(node.children.size());
  const Size available{inner.width, inner.height};
  float occupied = 0.0F;
  for (Node& child : node.children) {
    const Size desired = natural_size(child, available, context);
    const Dimension main_dimension = horizontal ? child.layout.width : child.layout.height;
    const float natural_main = horizontal ? desired.width : desired.height;
    const float minimum_main = horizontal ? child.layout.min_width : child.layout.min_height;
    const float maximum_main = horizontal ? child.layout.max_width : child.layout.max_height;
    const bool flexible = main_dimension.kind == DimensionKind::weight;
    const float main = flexible ? clamp_size(minimum_main, minimum_main, maximum_main)
                                : resolve_dimension(main_dimension, horizontal ? inner.width : inner.height,
                                      natural_main, minimum_main, maximum_main);
    const Dimension cross_dimension = horizontal ? child.layout.height : child.layout.width;
    const float natural_cross = horizontal ? desired.height : desired.width;
    const float minimum_cross = horizontal ? child.layout.min_height : child.layout.min_width;
    const float maximum_cross = horizontal ? child.layout.max_height : child.layout.max_width;
    const float cross_available = horizontal ? inner.height : inner.width;
    float cross = resolve_dimension(cross_dimension, cross_available, natural_cross, minimum_cross, maximum_cross);
    if (node.layout.cross_alignment == Alignment::stretch && cross_dimension.kind == DimensionKind::automatic) {
      cross = clamp_size(cross_available, minimum_cross, maximum_cross);
    }
    items.push_back({.node = &child,
        .natural = desired,
        .main = main,
        .cross = cross,
        .minimum_main = minimum_main,
        .maximum_main = maximum_main,
        .weight = flexible ? main_dimension.value : 0.0F,
        .flexible = flexible});
    occupied += main;
  }

  const float normal_gap = node.layout.gap;
  const float gaps = normal_gap * static_cast<float>(items.size() - 1U);
  const float available_main = horizontal ? inner.width : inner.height;
  distribute_flexible_space(items, std::max(0.0F, available_main - occupied - gaps));
  if (horizontal) {
    for (auto& item : items) {
      if (item.node->layout.height.kind == DimensionKind::automatic &&
          node.layout.cross_alignment != Alignment::stretch) {
        const Size measured = item.node->layout.kind == LayoutKind::leaf
                                  ? leaf_natural_size(*item.node, {item.main, inner.height}, context)
                                  : container_natural_size(*item.node, {item.main, inner.height}, context);
        item.cross = clamp_size(measured.height, item.node->layout.min_height, item.node->layout.max_height);
      }
    }
  }
  occupied = gaps;
  for (const LinearItem& item : items)
    occupied += item.main;
  const float free = scroll ? 0.0F : std::max(0.0F, available_main - occupied);
  float leading = 0.0F;
  float gap = normal_gap;
  switch (node.layout.main_alignment) {
  case MainAlignment::start:
    break;
  case MainAlignment::center:
    leading = free * 0.5F;
    break;
  case MainAlignment::end:
    leading = free;
    break;
  case MainAlignment::space_between:
    if (items.size() > 1U)
      gap += free / static_cast<float>(items.size() - 1U);
    break;
  }
  if (scroll)
    leading -= node.layout.scroll_offset;

  const bool reverse = horizontal && context.text_direction == TextDirection::right_to_left;
  float cursor = reverse ? inner.right() - leading : (horizontal ? inner.x : inner.y) + leading;
  for (LinearItem& item : items) {
    const float cross_available = horizontal ? inner.height : inner.width;
    const bool mirror_cross = !horizontal && context.text_direction == TextDirection::right_to_left;
    const float cross_start = horizontal ? inner.y : inner.x;
    const float positioned_cross =
        cross_position(cross_start, cross_available, item.cross, node.layout.cross_alignment, mirror_cross);
    Rect child_rect;
    if (horizontal) {
      const float main_start = reverse ? cursor - item.main : cursor;
      child_rect = {.x = main_start, .y = positioned_cross, .width = item.main, .height = item.cross};
      cursor += reverse ? -(item.main + gap) : item.main + gap;
    } else {
      child_rect = {.x = positioned_cross, .y = cursor, .width = item.cross, .height = item.main};
      cursor += item.main + gap;
    }
    arrange_node(*item.node, child_rect, child_clip, context);
  }
}

void arrange_stack(Node& node, const Rect inner, const Rect child_clip, const LayoutContext& context) {
  const Size available{inner.width, inner.height};
  for (Node& child : node.children) {
    const Size desired = natural_size(child, available, context);
    float width = desired.width;
    float height = desired.height;
    if (node.layout.cross_alignment == Alignment::stretch && child.layout.width.kind == DimensionKind::automatic) {
      width = clamp_size(inner.width, child.layout.min_width, child.layout.max_width);
    }
    const float x = cross_position(inner.x, inner.width, width, node.layout.cross_alignment,
        context.text_direction == TextDirection::right_to_left);
    const float y = cross_position(inner.y, inner.height, height, Alignment::center, false);
    arrange_node(child, {.x = x, .y = y, .width = width, .height = height}, child_clip, context);
  }
}

void arrange_grid(Node& node, const Rect inner, const Rect child_clip, const LayoutContext& context) {
  if (node.children.empty())
    return;
  const std::size_t columns = std::max<std::size_t>(1U, node.layout.grid_columns);
  const float total_gap = node.layout.gap * static_cast<float>(columns - 1U);
  const float cell_width = std::max(0.0F, inner.width - total_gap) / static_cast<float>(columns);
  const std::size_t row_count = (node.children.size() + columns - 1U) / columns;
  std::vector<float> row_heights(row_count, 0.0F);
  for (std::size_t index = 0; index < node.children.size(); ++index) {
    const Size desired = natural_size(node.children[index], {cell_width, inner.height}, context);
    row_heights[index / columns] = std::max(row_heights[index / columns], desired.height);
  }
  float y = inner.y;
  for (std::size_t row = 0; row < row_count; ++row) {
    for (std::size_t logical_column = 0; logical_column < columns; ++logical_column) {
      const std::size_t index = row * columns + logical_column;
      if (index >= node.children.size())
        break;
      const std::size_t visual_column =
          context.text_direction == TextDirection::right_to_left ? columns - logical_column - 1U : logical_column;
      const float x = inner.x + static_cast<float>(visual_column) * (cell_width + node.layout.gap);
      arrange_node(node.children[index], {.x = x, .y = y, .width = cell_width, .height = row_heights[row]}, child_clip,
          context);
    }
    y += row_heights[row] + node.layout.gap;
  }
}

void arrange_node(Node& node, const Rect rect, const Rect inherited_clip, const LayoutContext& context) {
  node.frame = {.x = rect.x, .y = rect.y, .width = std::max(0.0F, rect.width), .height = std::max(0.0F, rect.height)};
  node.clip = intersect(node.frame, inherited_clip);
  if (node.layout.kind == LayoutKind::leaf)
    return;

  const Rect inner = inset(node.frame, node.layout.padding);
  const bool clips_children = node.layout.clip_content || node.layout.kind == LayoutKind::scroll_list;
  const Rect child_clip = clips_children ? intersect(inherited_clip, inner) : inherited_clip;
  switch (node.layout.kind) {
  case LayoutKind::leaf:
    break;
  case LayoutKind::row:
    arrange_linear(node, inner, child_clip, context, true, false);
    break;
  case LayoutKind::column:
    arrange_linear(node, inner, child_clip, context, false, false);
    break;
  case LayoutKind::scroll_list:
    arrange_linear(node, inner, child_clip, context, false, true);
    break;
  case LayoutKind::stack:
    arrange_stack(node, inner, child_clip, context);
    break;
  case LayoutKind::grid:
    arrange_grid(node, inner, child_clip, context);
    break;
  }
}

void append_overflow_issues(
    const Node& node, const LayoutContext& context, LayoutResult& result) {
  if (node.semantics.hidden)
    return;
  // Nested percentage layouts and rectangle intersections differ by a few
  // float ULPs. This is below a rendered pixel; real clipping still fails.
  constexpr float measurement_tolerance = 0.01F;
  if (node.text.has_value() && context.text_measurer != nullptr &&
      node.frame.width > 0.0F && node.frame.height > 0.0F) {
    const auto text_frame = inset(node.frame, node.text->insets);
    const auto required = context.text_measurer->measure(node.text->value,
        node.text->font_size, text_frame.width, context.text_scale);
    if (required.width > text_frame.width + measurement_tolerance ||
        required.height > text_frame.height + measurement_tolerance) {
      result.overflows.push_back({.code = OverflowIssueCode::text_clipped,
          .node_id = node.id,
          .required = required,
          .available = {text_frame.width, text_frame.height}});
    }
  }

  if (node.layout.clip_content && node.layout.kind != LayoutKind::scroll_list) {
    const auto available = inset(node.frame, node.layout.padding);
    for (const auto& child : node.children) {
      if (child.semantics.hidden) {
        continue;
      }
      const auto visible = intersect(child.frame, available);
      if (visible.width + measurement_tolerance < child.frame.width ||
          visible.height + measurement_tolerance < child.frame.height) {
        result.overflows.push_back({.code = OverflowIssueCode::child_clipped,
            .node_id = child.id,
            .required = {child.frame.width, child.frame.height},
            .available = {visible.width, visible.height}});
      }
    }
  }
  for (const auto& child : node.children) {
    append_overflow_issues(child, context, result);
  }
}

} // namespace

LayoutResult layout(Node& root, const Rect viewport, const Insets safe_area, const LayoutContext& context) {
  LayoutResult result;
  if (!viewport.finite() || viewport.width <= 0.0F || viewport.height <= 0.0F) {
    result.issues.push_back({.code = LayoutIssueCode::invalid_viewport, .node_id = root.id});
  }
  if (!std::isfinite(safe_area.left) || !std::isfinite(safe_area.top) || !std::isfinite(safe_area.right) ||
      !std::isfinite(safe_area.bottom) || safe_area.left < 0.0F || safe_area.top < 0.0F || safe_area.right < 0.0F ||
      safe_area.bottom < 0.0F || safe_area.left + safe_area.right > viewport.width ||
      safe_area.top + safe_area.bottom > viewport.height) {
    result.issues.push_back({.code = LayoutIssueCode::invalid_safe_area, .node_id = root.id});
  }
  if (!std::isfinite(context.text_scale) || context.text_scale <= 0.0F || !std::isfinite(context.minimum_interaction_target.width) ||
      !std::isfinite(context.minimum_interaction_target.height) || context.minimum_interaction_target.width < 0.0F ||
      context.minimum_interaction_target.height < 0.0F) {
    result.issues.push_back({.code = LayoutIssueCode::invalid_context, .node_id = root.id});
  }
  std::unordered_set<NodeId> identifiers;
  std::unordered_set<std::string> semantic_ids;
  validate_node(root, identifiers, semantic_ids, result);
  result.valid = result.issues.empty();
  if (!result.valid)
    return result;

  result.safe_content = inset(viewport, safe_area);
  arrange_node(root, result.safe_content, result.safe_content, context);
  append_overflow_issues(root, context, result);
  return result;
}

} // namespace human_resources::ui
