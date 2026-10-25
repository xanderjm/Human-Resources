#pragma once
#include <array>
#include <cstddef>
#include <human_resources/gameplay/gameplay.h>
#include <human_resources/presentation/style_scene.h>
#include <span>

namespace human_resources::product {
using SceneBeatKind = gameplay::PresentationEventKind;
struct SceneBeat final {
  SceneBeatKind kind{};
  gameplay::Vec2 position;
  std::uint32_t actor_id{};
  float age{}, radius{};
};
struct SceneFeedback final {
  bool capture{}, laser{}, warning{}, hit{}, shield_hit{}, hull_hit{};
};
struct CapturePose final {
  presentation::Vec3 position, rotation;
  float scale{1};
};
// Reconstructible fixed-capacity presentation history. Only explicit gameplay
// outcomes produce beats; clocks freeze with active time and sequence is consumed
// once. No rule decisions, geometry-based hit inference or persistent data.
class SceneEffects final {
public:
  static constexpr std::size_t actor_capacity = 128, beat_capacity = 48;
  void reset();
  void update(const gameplay::Snapshot& run);
  [[nodiscard]] std::span<const SceneBeat> beats() const {
    return {beats_.data(), beat_count_};
  }
  [[nodiscard]] float wheel_angle(std::uint32_t actor_id) const;
  // Called once before scene actors. Retire absent/retyped IDs before admitting
  // replacements; history is reconstructible and never survives reset/restore.
  void begin_capture_frame(const gameplay::Snapshot& run);
  [[nodiscard]] CapturePose capture_pose(const gameplay::EntityView& actor,
      CapturePose captured, CapturePose ground, double seconds, float station);
  [[nodiscard]] static float capture_release_seconds() { return gameplay::capture_return_seconds(); }
  // Fresh outcomes for the most recent update only. Dense same-frame outcomes
  // coalesce per cue; persistent flash geometry never retriggers sound.
  [[nodiscard]] SceneFeedback feedback() const {
    return feedback_;
  }

private:
  void emit(const gameplay::PresentationEvent& event, double now);
  std::array<gameplay::EntityView, actor_capacity> previous_{};
  std::array<float, actor_capacity> wheel_angles_{};
  std::array<SceneBeat, beat_capacity> beats_{};
  std::size_t previous_count_{}, beat_count_{};
  std::uint64_t last_sequence_{};
  double seconds_{};
  bool initialized_{};
  SceneFeedback feedback_;
  struct CaptureHistory final {
    std::uint32_t id{};
    gameplay::EntityKind kind{};
    gameplay::InfantryRole role{};
    CapturePose last, start, ground;
    double seconds{}, transition_seconds{};
    float station{};
    float capture_yaw{};
    bool capturing{}, releasing{}, reacquiring{};
  };
  std::array<CaptureHistory, actor_capacity> captures_{};
  double capture_seconds_{};
  bool capture_initialized_{};
};
} // namespace human_resources::product
