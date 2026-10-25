#include <human_resources/accessibility/session.h>

#include "test_support.h"

#include <array>
#include <limits>

namespace {
using test_support::require;
using namespace human_resources;

void activations_follow_current_identity_and_state() {
  accessibility::Session session;
  std::array nodes{ui::AccessibilityNode{.id = 1U, .stable_id = "document/save", .role = ui::SemanticRole::button, .label = "Save", .frame = {0.0F, 0.0F, 48.0F, 48.0F}, .focusable = true}};
  require(session.publish(nodes), "valid semantic snapshot accepted");
  require(session.request("document/save", accessibility::ActionKind::activate), "native activation queued");
  nodes[0].id = 42U;
  require(session.publish(nodes), "rebuilt numeric IDs accepted");
  auto actions = session.drain_actions();
  require(actions.size() == 1U && actions[0].node_id == 42U, "queued identity resolves against current tree");
  require(session.request("document/save", accessibility::ActionKind::activate), "second activation queued");
  nodes[0].disabled = true;
  require(session.publish(nodes), "disabled state published");
  require(session.drain_actions().empty(), "action disabled after callback is rejected before mutation");
  require(!session.request("document/save", accessibility::ActionKind::activate), "disabled control rejects callbacks");
  nodes[0].disabled = false;
  require(session.publish(nodes), "control enabled again");
  require(session.request("document/save", accessibility::ActionKind::activate), "third activation queued");
  nodes[0].stable_id = "document/delete";
  require(session.publish(nodes), "frame-local ID reused for a different control");
  require(session.drain_actions().empty(), "stale activation cannot target the replacement control");
  require(!session.request("document/delete", accessibility::ActionKind::increment), "button is not an adjustable value");
  session.close();
  require(!session.publish(nodes) && !session.request("document/delete", accessibility::ActionKind::activate),
      "callbacks after surface teardown are inert");
}

void invalid_snapshots_and_callback_bursts_are_bounded() {
  accessibility::Session session;
  std::array nodes{ui::AccessibilityNode{.id = 1U, .stable_id = "slider", .role = ui::SemanticRole::slider, .frame = {0.0F, 0.0F, 48.0F, 48.0F}, .focusable = true}};
  require(session.publish(nodes), "slider snapshot accepted");
  for (std::size_t index = 0; index < 64U; ++index)
    require(session.request("slider", accessibility::ActionKind::increment), "bounded slider callback accepted");
  require(!session.request("slider", accessibility::ActionKind::increment), "native callback queue is bounded");
  require(session.drain_actions().size() == 64U, "accepted callbacks are delivered once");
  nodes[0].frame.x = std::numeric_limits<float>::quiet_NaN();
  require(!session.publish(nodes) && session.snapshot().empty(), "non-finite native frames rejected");
  nodes[0].frame.x = 0.0F;
  std::array duplicate{nodes[0], nodes[0]};
  duplicate[1].id = 2U;
  require(!session.publish(duplicate), "duplicate native identity rejected");
  nodes[0].label = "one\ttwo\nthree";
  const auto encoded = accessibility::serialize(nodes);
  require(encoded.find("one") == std::string::npos && encoded.find('\n') == std::string::npos,
      "native wire encoding cannot split label content into nodes");
}
} // namespace

int main() {
  activations_follow_current_identity_and_state();
  invalid_snapshots_and_callback_bursts_are_bounded();
}
