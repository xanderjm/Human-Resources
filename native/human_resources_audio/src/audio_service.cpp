#include <human_resources/audio/audio_service.h>

#include <algorithm>
#include <cmath>

namespace human_resources::audio {
namespace {

float sanitize_gain(const float gain) noexcept {
  return std::isfinite(gain) ? std::clamp(gain, 0.0F, 1.0F) : 1.0F;
}

} // namespace

void NullAudioService::play(const Cue&) noexcept {}

void NullAudioService::stop(const std::uint32_t) noexcept {}

void NullAudioService::stop_all() noexcept {}

void NullAudioService::set_bus_gain(const Bus bus, const float gain) noexcept {
  const float value = sanitize_gain(gain);
  switch (bus) {
  case Bus::master:
    master_gain_ = value;
    return;
  case Bus::music:
    music_gain_ = value;
    return;
  case Bus::effects:
    effects_gain_ = value;
    return;
  case Bus::interface:
    interface_gain_ = value;
    return;
  case Bus::ambience:
    ambience_gain_ = value;
    return;
  case Bus::voice:
    voice_gain_ = value;
    return;
  }
}

float NullAudioService::bus_gain(const Bus bus) const noexcept {
  switch (bus) {
  case Bus::master:
    return master_gain_;
  case Bus::music:
    return music_gain_;
  case Bus::effects:
    return effects_gain_;
  case Bus::interface:
    return interface_gain_;
  case Bus::ambience:
    return ambience_gain_;
  case Bus::voice:
    return voice_gain_;
  }
  return 0.0F;
}

void NullAudioService::handle_interruption(const Interruption interruption) noexcept {
  switch (interruption) {
  case Interruption::began:
    interrupted_ = true;
    resume_allowed_ = false;
    return;
  case Interruption::ended_should_resume:
    interrupted_ = false;
    resume_allowed_ = true;
    return;
  case Interruption::ended_should_remain_paused:
    interrupted_ = false;
    resume_allowed_ = false;
    return;
  }
}

} // namespace human_resources::audio
