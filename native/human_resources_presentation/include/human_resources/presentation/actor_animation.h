#pragma once
#include <array>
#include <cstdint>
#include <optional>
namespace human_resources::presentation {
inline constexpr float capture_lift_fraction = .65F;
// Presentation-only input. Root placement and gameplay remain with their owners.
struct ActorAnimation final {
  float speed{};
  bool warning{}, capturing{}, enabled{};
  std::uint64_t fire_sequence{};
  float fire_age_seconds{};
  std::array<float, 3> aim_target{}; // Actor-local metres; used only by equipped infantry.
  bool recovering{}; // Restored authored fire pose; does not invent a shot event.
  // Product scene owns capture/release world transport. Remove only the authored
  // capture clip's root translation; local joint poses and rotations survive.
  bool product_capture_transport{};
  // Presented intake progress: live authority plus a short confirmed collection
  // finish. The same fraction owns source lift/struggle and body transport.
  float capture_progress{};
  bool fleeing{}; // Real civilian escape gait, independent of military warnings.
  bool preparing{}; // Actual infantry weapon raise; no committed warning yet.
  // Real elapsed raise/aim time, including the warning. Absent for legacy or
  // synthetic inputs; upper-body sampling matches after load. Transient turning
  // feet start from the canonical authored pose without saved visual history.
  std::optional<float> aim_pose_seconds{};
  // Actual physical infantry yaw. Renderer history may articulate only feet;
  // this never changes root orientation or implies world travel.
  std::optional<float> body_heading{};
  bool backward{}; // Actual infantry retreat; reverse the existing looping gait.
};
} // namespace human_resources::presentation
