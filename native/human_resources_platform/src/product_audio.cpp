#include <human_resources/platform/product_audio.h>

#include <SDL3/SDL.h>

#include <array>

namespace human_resources::platform {
ProductAudio::~ProductAudio() {
  close();
}

bool ProductAudio::initialize() noexcept {
  if (stream_ != nullptr)
    return true;
  if (!initialized_) {
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO))
      return false;
    initialized_ = true;
  }
  const SDL_AudioSpec specification{SDL_AUDIO_F32, 2, feedback::Mixer::sample_rate};
  stream_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &specification, nullptr, nullptr);
  if (stream_ == nullptr)
    return false;
  apply_policy();
  return stream_ != nullptr;
}
void ProductAudio::close() noexcept {
  if (stream_ != nullptr)
    SDL_DestroyAudioStream(stream_);
  stream_ = nullptr;
  if (initialized_)
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
  initialized_ = false;
}
void ProductAudio::apply_policy() noexcept {
  mixer_.stop_all();
  if (stream_ == nullptr)
    return;
  if (!SDL_PauseAudioStreamDevice(stream_) || !SDL_ClearAudioStream(stream_)) {
    close();
    return;
  }
  if (enabled_ && !paused_ && !SDL_ResumeAudioStreamDevice(stream_))
    close();
}
void ProductAudio::set_enabled(const bool enabled) noexcept {
  if (enabled_ == enabled)
    return;
  enabled_ = enabled;
  apply_policy();
}
void ProductAudio::set_paused(const bool paused) noexcept {
  if (paused_ == paused)
    return;
  paused_ = paused;
  apply_policy();
}
void ProductAudio::set_tractor_active(const bool active, const float intensity) noexcept {
  mixer_.set_tractor_active(active && enabled_ && !paused_, intensity);
}
void ProductAudio::play(const feedback::Cue cue, const float pan) noexcept {
  if (stream_ != nullptr && enabled_ && !paused_)
    static_cast<void>(mixer_.play(cue, pan));
}
void ProductAudio::update() noexcept {
  if (stream_ == nullptr || !enabled_ || paused_)
    return;
  int queued = SDL_GetAudioStreamQueued(stream_);
  if (queued < 0) {
    close();
    return;
  }
  constexpr std::size_t block_frames = 256;
  constexpr int block_bytes = static_cast<int>(block_frames * 2U * sizeof(float));
  constexpr int target_bytes = block_bytes * 6; // 32ms, bounded after a stalled frame.
  std::array<float, block_frames * 2U> samples{};
  for (int block = 0; queued < target_bytes && block < 6; ++block) {
    mixer_.render(samples);
    if (!SDL_PutAudioStreamData(stream_, samples.data(), block_bytes)) {
      close();
      return;
    }
    queued += block_bytes;
  }
}
} // namespace human_resources::platform
