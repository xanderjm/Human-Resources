#pragma once

#include <human_resources/ui/layout.h>

#include <mutex>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace human_resources::accessibility {

enum class ActionKind : std::uint8_t { activate,
  increment,
  decrement };

struct Action final {
  ui::NodeId node_id{};
  ActionKind kind{ActionKind::activate};
};

struct Preferences final {
  float text_scale{1.0F};
  bool reduced_motion{};
  bool native_bridge_available{};
};

// One session per native surface. Native callbacks queue durable identities;
// actions resolve against the current tree on the product thread, never against
// a frame-local ID captured by an earlier native accessibility element.
class Session final {
public:
  [[nodiscard]] bool publish(std::span<const ui::AccessibilityNode> nodes);
  [[nodiscard]] std::vector<ui::AccessibilityNode> snapshot() const;
  [[nodiscard]] bool request(std::string_view identity, ActionKind kind);
  [[nodiscard]] std::vector<Action> drain_actions();
  void close();

private:
  struct Request final {
    std::string identity;
    ActionKind kind{};
  };
  [[nodiscard]] ui::NodeId resolve(std::string_view identity, ActionKind kind) const;
  mutable std::mutex mutex_;
  std::vector<ui::AccessibilityNode> nodes_;
  std::vector<Request> pending_;
  bool closed_{};
};

// ASCII-only wire representation for narrow JNI/native bridges. Text and keys
// are hex-encoded UTF-8, so delimiter, locale and modified-UTF-8 rules cannot
// change identity or split user text into additional elements.
[[nodiscard]] std::string serialize(std::span<const ui::AccessibilityNode> nodes);

} // namespace human_resources::accessibility
