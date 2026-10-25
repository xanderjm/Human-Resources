#include "test_support.h"

#include <human_resources/ui/layout.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <limits>
#include <string_view>

namespace {

using test_support::require;
using test_support::require_near;
using namespace human_resources::ui;

class FixedTextMeasurer final : public TextMeasurer {
public:
  Size measure(const std::string_view text, const float font_size, const float maximum_width,
      const float text_scale) const noexcept override {
    const float glyph_width = font_size * text_scale * 0.5F;
    const float unwrapped = glyph_width * static_cast<float>(text.size());
    const float width = std::min(unwrapped, maximum_width);
    const float lines = maximum_width > 0.0F ? std::ceil(unwrapped / maximum_width) : 1.0F;
    return {width, std::max(1.0F, lines) * font_size * text_scale * 1.25F};
  }
};

Node leaf(const NodeId id, const Dimension width, const Dimension height, const Size intrinsic = {}) {
  Node node;
  node.id = id;
  node.layout.width = width;
  node.layout.height = height;
  node.intrinsic_size = intrinsic;
  return node;
}

void weighted_row_and_rtl_are_exact() {
  Node root;
  root.id = 1U;
  root.layout.kind = LayoutKind::row;
  root.layout.padding = {10.0F, 10.0F, 10.0F, 10.0F};
  root.layout.gap = 10.0F;
  root.children.push_back(leaf(2U, Dimension::points(60.0F), Dimension::automatic(), {60.0F, 20.0F}));
  root.children.push_back(leaf(3U, Dimension::weight(1.0F), Dimension::automatic(), {0.0F, 20.0F}));
  root.children.push_back(leaf(4U, Dimension::weight(2.0F), Dimension::automatic(), {0.0F, 20.0F}));

  LayoutContext context;
  auto result = layout(root, {0.0F, 0.0F, 300.0F, 100.0F}, {}, context);
  require(result.valid, "weighted LTR row validates");
  require_near(root.children[0].frame.x, 10.0F, 0.01F, "fixed child starts after padding");
  require_near(root.children[0].frame.width, 60.0F, 0.01F, "fixed child preserves width");
  require_near(root.children[1].frame.x, 80.0F, 0.01F, "first flex child follows gap");
  require_near(root.children[1].frame.width, 200.0F / 3.0F, 0.01F, "first flex child gets one share");
  require_near(root.children[2].frame.width, 400.0F / 3.0F, 0.01F, "second flex child gets two shares");
  require_near(root.children[2].frame.right(), 290.0F, 0.01F, "row ends at inner edge");
  require_near(root.children[1].frame.height, 80.0F, 0.01F, "automatic cross size stretches");

  context.text_direction = TextDirection::right_to_left;
  result = layout(root, {0.0F, 0.0F, 300.0F, 100.0F}, {}, context);
  require(result.valid, "weighted RTL row validates");
  require_near(root.children[0].frame.right(), 290.0F, 0.01F, "first logical child mirrors right");
  require_near(root.children[2].frame.x, 10.0F, 0.01F, "last logical child mirrors left");
}

void safe_area_and_text_measurement_are_first_class() {
  FixedTextMeasurer measurer;
  DesignTokens tokens;
  Node root = components::screen(10U, tokens);
  Node label = components::label(11U, "A deliberately long localized heading", tokens);
  label.layout.width = Dimension::percent(1.0F);
  root.children.push_back(std::move(label));
  LayoutContext context{.text_scale = 1.5F, .text_measurer = &measurer};
  const auto result = layout(root, {0.0F, 0.0F, 390.0F, 844.0F}, {12.0F, 47.0F, 12.0F, 34.0F}, context);
  require(result.valid, "safe-area text layout validates");
  require_near(root.frame.x, 12.0F, 0.01F, "safe left inset is applied");
  require_near(root.frame.y, 47.0F, 0.01F, "safe top inset is applied");
  require_near(root.frame.width, 366.0F, 0.01F, "safe horizontal insets are applied");
  require(root.children[0].frame.height > tokens.typography.body,
      "text measurer and text scale determine intrinsic height");
}

void grid_scroll_hit_testing_and_clipping_are_bounded() {
  Node grid;
  grid.id = 20U;
  grid.layout.kind = LayoutKind::grid;
  grid.layout.grid_columns = 2U;
  grid.layout.gap = 10.0F;
  for (NodeId id = 21U; id <= 24U; ++id) {
    grid.children.push_back(leaf(id, Dimension::automatic(), Dimension::points(44.0F)));
  }
  LayoutContext rtl{.text_direction = TextDirection::right_to_left};
  auto result = layout(grid, {0.0F, 0.0F, 200.0F, 100.0F}, {}, rtl);
  require(result.valid, "RTL grid validates");
  require_near(grid.children[0].frame.x, 105.0F, 0.01F, "first grid cell mirrors");
  require_near(grid.children[1].frame.x, 0.0F, 0.01F, "second grid cell mirrors");

  DesignTokens tokens;
  Node scroll;
  scroll.id = 30U;
  scroll.layout.kind = LayoutKind::scroll_list;
  scroll.layout.scroll_offset = 50.0F;
  Node first = components::button(31U, "First", tokens);
  first.layout.height = Dimension::points(60.0F);
  Node second = components::button(32U, "Second", tokens);
  second.layout.height = Dimension::points(60.0F);
  scroll.children.push_back(std::move(first));
  scroll.children.push_back(std::move(second));
  result = layout(scroll, {0.0F, 0.0F, 100.0F, 100.0F}, {}, {});
  require(result.valid, "scroll list validates");
  require_near(scroll.children[0].frame.y, -50.0F, 0.01F, "scroll offset moves content");
  require(scroll.children[0].clip.y == 0.0F, "scroll content clip begins at viewport");
  require(hit_test(scroll, {20.0F, 20.0F}) == 32U, "hit testing chooses visible frontmost scrolled button");
  require(!hit_test(scroll, {20.0F, 120.0F}).has_value(), "hit testing rejects clipped point");
}

void semantics_touch_targets_and_validation_fail_closed() {
  DesignTokens tokens;
  Node root = components::screen(40U, tokens);
  Node button = components::button(41U, "Continue", tokens);
  button.layout.width = Dimension::points(30.0F);
  button.layout.height = Dimension::points(30.0F);
  button.layout.min_width = 0.0F;
  button.layout.min_height = 0.0F;
  root.children.push_back(std::move(button));
  auto result = layout(root, {0.0F, 0.0F, 200.0F, 100.0F}, {}, {});
  require(result.valid, "undersized target is valid geometry before policy audit");
  const auto violations = validate_interaction_targets(root, tokens.minimum_interaction_target);
  require(violations.size() == 1U && violations[0].node_id == 41U, "touch audit identifies undersized action");
  const auto semantics = accessibility_tree(root);
  require(semantics.size() == 2U, "screen and button appear in semantic tree");
  require(semantics[1].parent_id == 40U && semantics[1].label == "Continue",
      "semantic hierarchy and label are retained");
  require(focus_order(root) == std::vector<NodeId>{41U}, "focus order follows document order");

  root.children.push_back(leaf(41U, Dimension::points(20.0F), Dimension::points(20.0F)));
  result = layout(root, {0.0F, 0.0F, 200.0F, 100.0F}, {}, {});
  require(!result.valid, "duplicate node identifiers fail layout validation");
  require(std::any_of(result.issues.begin(), result.issues.end(),
              [](const LayoutIssue& issue) { return issue.code == LayoutIssueCode::duplicate_node_id; }),
      "duplicate identifier produces a precise issue");

  LayoutContext invalid_context;
  invalid_context.text_scale = std::numeric_limits<float>::quiet_NaN();
  result = layout(root, {0.0F, 0.0F, 200.0F, 100.0F}, {}, invalid_context);
  require(std::any_of(result.issues.begin(), result.issues.end(),
              [](const LayoutIssue& issue) { return issue.code == LayoutIssueCode::invalid_context; }),
      "non-finite accessibility scale fails validation");
  require(effective_motion_duration(tokens.motion.standard_ms, true) == 0U,
      "reduced motion resolves animation immediately");
}

void constrained_text_and_grid_measurement_preserve_height() {
  FixedTextMeasurer measurer;
  LayoutContext context{.text_measurer = &measurer};
  Node root;
  root.id = 60U;
  root.layout.kind = LayoutKind::column;
  Node text = leaf(61U, Dimension::points(60.0F), Dimension::automatic());
  text.text = TextContent{.value = "abcdefghijklmnopqrst", .font_size = 10.0F, .insets = {10.0F, 4.0F, 10.0F, 6.0F}};
  root.children.push_back(text);
  auto result = layout(root, {0.0F, 0.0F, 300.0F, 500.0F}, {}, context);
  require(result.valid && result.overflows.empty(), "explicit-width text fits its measured frame");
  require_near(root.children[0].frame.height, 47.5F, 0.01F, "text wraps inside its width minus insets");

  Node grid;
  grid.id = 62U;
  grid.layout.kind = LayoutKind::grid;
  grid.layout.grid_columns = 2U;
  grid.layout.gap = 10.0F;
  text.id = 63U;
  text.layout.width = Dimension::automatic();
  grid.children.push_back(text);
  text.id = 64U;
  grid.children.push_back(text);
  root.children = {grid, leaf(65U, Dimension::points(50.0F), Dimension::points(20.0F))};
  result = layout(root, {0.0F, 0.0F, 150.0F, 500.0F}, {}, context);
  require(result.valid && result.overflows.empty(), "nested grid text remains unclipped");
  require(root.children[1].frame.y >= root.children[0].children[0].frame.bottom(),
      "grid intrinsic height uses actual cell width before placing following content");
  root.children[0].children[0].layout.height = Dimension::points(5.0F);
  result = layout(root, {0.0F, 0.0F, 150.0F, 500.0F}, {}, context);
  // Both cells use the tallest row height; force both short to exercise clipping.
  root.children[0].children[1].layout.height = Dimension::points(5.0F);
  result = layout(root, {0.0F, 0.0F, 150.0F, 500.0F}, {}, context);
  require(result.valid && !result.overflows.empty(), "text overflow is reported separately from invalid geometry");
}

void semantic_identity_survives_rebuilt_node_ids() {
  DesignTokens tokens;
  auto button = components::button(71U, "Save", tokens);
  button.semantics.stable_id = "document/save";
  require(layout(button, {0.0F, 0.0F, 100.0F, 50.0F}, {}, {}).valid, "semantic fixture lays out");
  const auto first = accessibility_tree(button);
  button.id = 72U;
  const auto rebuilt = accessibility_tree(button);
  require(first[0].stable_id == rebuilt[0].stable_id && first[0].id != rebuilt[0].id,
      "semantic identity is independent of frame-local node allocation");
  auto root = components::screen(70U, tokens);
  root.children = {button};
  button.id = 73U;
  root.children.push_back(button);
  const auto duplicate = layout(root, {0.0F, 0.0F, 100.0F, 200.0F}, {}, {});
  require(!duplicate.valid && std::any_of(duplicate.issues.begin(), duplicate.issues.end(),
                                  [](const auto& issue) { return issue.code == LayoutIssueCode::duplicate_semantic_id; }),
      "duplicate semantic identities fail before publication");
}

} // namespace

int main() {
  weighted_row_and_rtl_are_exact();
  safe_area_and_text_measurement_are_first_class();
  grid_scroll_hit_testing_and_clipping_are_bounded();
  semantics_touch_targets_and_validation_fail_closed();
  constrained_text_and_grid_measurement_preserve_height();
  semantic_identity_survives_rebuilt_node_ids();
  std::cout << "UI contracts passed\n";
  return 0;
}
