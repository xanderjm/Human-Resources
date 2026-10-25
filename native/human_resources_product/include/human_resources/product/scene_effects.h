#pragma once
#include <array>
#include <cstddef>
#include <human_resources/gameplay/gameplay.h>
#include <human_resources/presentation/style_scene.h>
#include <human_resources/product/damage_feedback.h>
#include <span>

namespace human_resources::product {
using SceneBeatKind = gameplay::PresentationEventKind;
struct SceneBeat final {
  SceneBeatKind kind{};
  gameplay::Vec2 position;
  std::uint32_t actor_id{};
  float age{}, radius{};
  gameplay::Vec2 destination; // Frozen observed player endpoint for real dashes.
};
struct SceneFeedback final {
  bool capture{}, laser{}, warning{}, hit{}, shield_hit{}, hull_hit{};
  bool sweep_capture{}, harvest_overdrive{}, phase_dash{};
};
struct CapturePose final {
  presentation::Vec3 position, rotation;
  float scale{1};
};
enum class CaptureMotion { animated,
  reduced };
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
  // Derived active-time attitude; never changes the actual player position.
  [[nodiscard]] gameplay::Vec2 flight_velocity() const {
    return flight_velocity_;
  }
  [[nodiscard]] float capture_response() const;
  [[nodiscard]] gameplay::Vec2 dash_response() const;
  [[nodiscard]] float rotor_angle() const {
    return rotor_angle_;
  }
  [[nodiscard]] DamageFeedback damage_feedback() const;
  // Called once before scene actors. Retire absent/retyped IDs before admitting
  // replacements; history is reconstructible and never survives reset/restore.
  void begin_capture_frame(const gameplay::Snapshot& run);
  // A fast committed collection can finish its visible intake after authority
  // removes the actor. Live bodies and these confirmed finishers share one path.
  [[nodiscard]] float capture_progress(const gameplay::EntityView& actor) const;
  [[nodiscard]] static float capture_lift_progress(float authority_progress) {
    return authority_progress >= 1 ? 1 : authority_progress * presentation::capture_lift_fraction;
  }
  [[nodiscard]] std::span<const gameplay::EntityView> completed_captures() const {
    return {completed_captures_.data(), completed_count_};
  }
  [[nodiscard]] CapturePose capture_pose(const gameplay::EntityView& actor,
      CapturePose captured, CapturePose ground, double seconds, float station,
      CaptureMotion motion = CaptureMotion::animated);
  [[nodiscard]] static float capture_release_seconds() { return gameplay::capture_return_seconds(); }
  // Fresh outcomes for the most recent update only. Dense same-frame outcomes
  // coalesce per cue; persistent flash geometry never retriggers sound.
  [[nodiscard]] SceneFeedback feedback() const {
    return feedback_;
  }

private:
  void emit(const gameplay::PresentationEvent& event, double now, gameplay::Vec2 player);
  std::array<gameplay::EntityView, actor_capacity> previous_{};
  std::array<float, actor_capacity> wheel_angles_{};
  std::array<SceneBeat, beat_capacity> beats_{};
  std::size_t previous_count_{}, beat_count_{};
  std::uint64_t last_sequence_{};
  double seconds_{};
  bool initialized_{};
  gameplay::Vec2 flight_velocity_{};
  float rotor_angle_{}, rotor_speed_{1.8F};
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
    gameplay::EntityView actor;
    float display_progress{}, finish_progress{};
    bool completed{};
  };
  std::array<CaptureHistory, actor_capacity> captures_{};
  double capture_seconds_{};
  bool capture_initialized_{};
  std::array<gameplay::EntityView, actor_capacity> completed_captures_{};
  std::size_t completed_count_{};
};
} // namespace human_resources::product
