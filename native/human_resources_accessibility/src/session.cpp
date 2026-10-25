#include <human_resources/accessibility/session.h>

#include <algorithm>
#include <unordered_set>

namespace human_resources::accessibility {

bool Session::publish(const std::span<const ui::AccessibilityNode> nodes) {
  const std::scoped_lock lock(mutex_);
  if (closed_)
    return false;
  std::unordered_set<std::string> identities;
  std::unordered_set<ui::NodeId> ids;
  for (const auto& node : nodes) {
    if (node.id == ui::invalid_node_id || node.stable_id.empty() || !node.frame.finite() ||
        node.frame.width < 0.0F || node.frame.height < 0.0F ||
        !identities.insert(node.stable_id).second || !ids.insert(node.id).second) {
      nodes_.clear();
      pending_.clear();
      return false;
    }
  }
  nodes_.assign(nodes.begin(), nodes.end());
  return true;
}

std::vector<ui::AccessibilityNode> Session::snapshot() const {
  const std::scoped_lock lock(mutex_);
  return nodes_;
}

ui::NodeId Session::resolve(const std::string_view identity, const ActionKind kind) const {
  const auto found = std::find_if(nodes_.begin(), nodes_.end(), [&](const auto& node) {
    return node.stable_id == identity;
  });
  if (closed_ || found == nodes_.end() || found->disabled || !found->focusable ||
      found->frame.width <= 0.0F || found->frame.height <= 0.0F)
    return ui::invalid_node_id;
  const bool activation = kind == ActionKind::activate &&
                          (found->role == ui::SemanticRole::button || found->role == ui::SemanticRole::toggle);
  const bool adjustment = (kind == ActionKind::increment || kind == ActionKind::decrement) &&
                          found->role == ui::SemanticRole::slider;
  return activation || adjustment ? found->id : ui::invalid_node_id;
}

bool Session::request(const std::string_view identity, const ActionKind kind) {
  const std::scoped_lock lock(mutex_);
  constexpr std::size_t maximum_pending_actions = 64U;
  if (pending_.size() >= maximum_pending_actions || resolve(identity, kind) == ui::invalid_node_id)
    return false;
  pending_.push_back({std::string(identity), kind});
  return true;
}

std::vector<Action> Session::drain_actions() {
  const std::scoped_lock lock(mutex_);
  std::vector<Action> actions;
  for (const auto& request : pending_) {
    const auto id = resolve(request.identity, request.kind);
    if (id != ui::invalid_node_id)
      actions.push_back({id, request.kind});
  }
  pending_.clear();
  return actions;
}

void Session::close() {
  const std::scoped_lock lock(mutex_);
  closed_ = true;
  nodes_.clear();
  pending_.clear();
}

} // namespace human_resources::accessibility
