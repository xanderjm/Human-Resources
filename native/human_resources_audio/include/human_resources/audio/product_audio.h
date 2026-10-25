#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace human_resources::feedback {

enum class Cue : std::uint8_t { capture,
  laser,
  warning,
  hull_hit,
  upgrade,
  purchase,
  death,
  ui,
  count };

// Single-threaded, allocation-free PCM synthesis. Frames are stereo float at 48 kHz.
// The platform owns device buffering; the game only requests semantic cues.
class Mixer final {
public:
  static constexpr int sample_rate = 48000;
  static constexpr std::size_t voice_limit = 16;
  Mixer() noexcept;
  [[nodiscard]] bool play(Cue cue, float pan = 0.0F) noexcept;
  void set_tractor_active(bool active, float intensity = 1.0F) noexcept;
  void set_gain(float gain) noexcept;
  void stop_all() noexcept;
  void render(std::span<float> interleaved_stereo) noexcept;
  [[nodiscard]] std::size_t active_voices() const noexcept;
  [[nodiscard]] std::uint64_t rendered_frames() const noexcept {
    return clock_;
  }

private:
  struct Voice final {
    Cue cue{Cue::ui};
    float age{};
    float duration{};
    float phase{};
    float secondary_phase{};
    float noise{};
    float left{};
    float right{};
    bool active{};
  };
  [[nodiscard]] float wave(float phase) const noexcept;
  [[nodiscard]] float synthesize(Voice& voice) noexcept;
  std::array<Voice, voice_limit> voices_{};
  std::array<float, 2048> sine_{};
  std::array<std::uint64_t, static_cast<std::size_t>(Cue::count)> next_cue_frame_{};
  std::uint64_t clock_{};
  std::uint32_t random_{0x71cb2351U};
  float tractor_target_{};
  float tractor_level_{};
  float tractor_phase_{};
  float tractor_secondary_phase_{};
  float gain_{0.65F};
  float duck_{1.0F};
};

} // namespace human_resources::feedback
