#pragma once

#include <human_resources/audio/product_audio.h>

struct SDL_AudioStream;

namespace human_resources::platform {

// Main-thread owner. update() queues at most 32ms of PCM with no audio callback.
// Background/pause/disable clears stale sounds. Failure is optional and silent.
class ProductAudio final {
public:
  ProductAudio() = default;
  ~ProductAudio();
  ProductAudio(const ProductAudio&) = delete;
  ProductAudio& operator=(const ProductAudio&) = delete;
  [[nodiscard]] bool initialize() noexcept;
  [[nodiscard]] bool available() const noexcept {
    return stream_ != nullptr;
  }
  void set_enabled(bool enabled) noexcept;
  void set_paused(bool paused) noexcept;
  void set_tractor_active(bool active, float intensity = 1.0F) noexcept;
  void play(feedback::Cue cue, float pan = 0.0F) noexcept;
  void update() noexcept;

private:
  void apply_policy() noexcept;
  void close() noexcept;
  feedback::Mixer mixer_{};
  SDL_AudioStream* stream_{};
  bool initialized_{};
  bool enabled_{true};
  bool paused_{};
};
} // namespace human_resources::platform
