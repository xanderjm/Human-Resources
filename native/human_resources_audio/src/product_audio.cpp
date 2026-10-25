#include <human_resources/audio/product_audio.h>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace human_resources::feedback {
namespace {
constexpr float step = 1.0F / static_cast<float>(Mixer::sample_rate);
constexpr float tau = 2.0F * std::numbers::pi_v<float>;
struct Tone final {
  float duration;
  float frequency;
  float end_frequency;
  float gain;
  float cooldown;
};
constexpr std::array<Tone, static_cast<std::size_t>(Cue::count)> tones{{
    {0.36F, 420.0F, 880.0F, 0.15F, 0.07F}, // A soft upward capture resolution.
    {0.15F, 740.0F, 190.0F, 0.10F, 0.065F},
    {0.44F, 230.0F, 205.0F, 0.12F, 1.2F},
    {0.26F, 90.0F, 45.0F, 0.24F, 0.12F},
    {0.9F, 523.25F, 523.25F, 0.15F, 0.25F},
    {0.58F, 440.0F, 440.0F, 0.15F, 0.10F},
    {1.2F, 200.0F, 49.0F, 0.22F, 0.8F},
    {0.11F, 540.0F, 470.0F, 0.07F, 0.04F},
    {0.23F, 520.0F, 1320.0F, 0.19F, 0.09F}, // Close-pass intake: a quick rising zip.
    {0.62F, 220.0F, 440.0F, 0.20F, 0.4F},   // Earned boost ignition, not a looping alarm.
    {0.18F, 820.0F, 160.0F, 0.19F, 0.15F},  // Phase launch and counterattack.
}};
bool priority_cue(const Cue cue) noexcept {
  return cue == Cue::death || cue == Cue::upgrade || cue == Cue::hull_hit ||
         cue == Cue::harvest_overdrive || cue == Cue::phase_dash;
}
float bounded(const float value, const float fallback = 0.0F) noexcept {
  return std::isfinite(value) ? std::clamp(value, 0.0F, 1.0F) : fallback;
}
float advance(float& phase, const float frequency) noexcept {
  phase += frequency * step;
  phase -= std::floor(phase);
  return phase;
}
float smooth(const float value) noexcept {
  return value * value * (3.0F - 2.0F * value);
}
} // namespace

CueSelection select_gameplay_cues(const GameplayFeedback feedback) noexcept {
  CueSelection cues{};
  const auto mark = [&](const Cue cue) { cues[static_cast<std::size_t>(cue)] = true; };
  if (feedback.harvest_overdrive)
    mark(Cue::harvest_overdrive);
  else if (feedback.sweep_capture)
    mark(Cue::sweep_capture);
  else if (feedback.capture)
    mark(Cue::capture);
  if (feedback.phase_dash)
    mark(Cue::phase_dash);
  else if (feedback.laser)
    mark(Cue::laser);
  if (feedback.hit)
    mark(Cue::hull_hit);
  if (feedback.warning)
    mark(Cue::warning);
  return cues;
}

Mixer::Mixer() noexcept {
  for (std::size_t index = 0; index < sine_.size(); ++index) {
    sine_[index] = std::sin(tau * static_cast<float>(index) / static_cast<float>(sine_.size()));
  }
}

bool Mixer::play(const Cue cue, const float pan) noexcept {
  const auto index = static_cast<std::size_t>(cue);
  if (index >= tones.size() || clock_ < next_cue_frame_[index])
    return false;
  const bool priority = priority_cue(cue);
  // Four reserved voices keep important feedback audible during dense capture/fire.
  const auto last = priority ? voices_.end() : voices_.begin() + 12;
  const auto free = std::find_if(voices_.begin(), last, [](const Voice& voice) { return !voice.active; });
  if (free == last)
    return false;
  const float position = std::isfinite(pan) ? std::clamp(pan, -1.0F, 1.0F) : 0.0F;
  const auto& tone = tones[index];
  *free = Voice{cue, 0.0F, tone.duration, 0.0F, 0.0F, 0.0F,
      std::sqrt((1.0F - position) * 0.5F), std::sqrt((1.0F + position) * 0.5F), true};
  next_cue_frame_[index] = clock_ + static_cast<std::uint64_t>(tone.cooldown * sample_rate);
  if (priority)
    duck_ = 0.38F;
  return true;
}

void Mixer::set_tractor_active(const bool active, const float intensity) noexcept {
  tractor_target_ = active ? bounded(intensity) : 0.0F;
}
void Mixer::set_gain(const float gain) noexcept {
  gain_ = bounded(gain);
}
void Mixer::stop_all() noexcept {
  for (auto& voice : voices_)
    voice.active = false;
  next_cue_frame_.fill(0);
  tractor_target_ = tractor_level_ = 0.0F;
  duck_ = 1.0F;
}
std::size_t Mixer::active_voices() const noexcept {
  return static_cast<std::size_t>(std::count_if(voices_.begin(), voices_.end(), [](const Voice& v) { return v.active; }));
}
float Mixer::wave(const float phase) const noexcept {
  const float position = (phase - std::floor(phase)) * static_cast<float>(sine_.size());
  const auto index = static_cast<std::size_t>(position);
  const float fraction = position - static_cast<float>(index);
  return sine_[index] + (sine_[(index + 1U) % sine_.size()] - sine_[index]) * fraction;
}

float Mixer::synthesize(Voice& voice) noexcept {
  const auto& tone = tones[static_cast<std::size_t>(voice.cue)];
  const float progress = std::min(1.0F, voice.age / voice.duration);
  float frequency = tone.frequency + (tone.end_frequency - tone.frequency) * smooth(progress);
  if (voice.cue == Cue::upgrade || voice.cue == Cue::purchase) {
    // A short three-note major arpeggio with a ringing upper partial.
    constexpr std::array<float, 3> intervals{1.0F, 1.25F, 1.5F};
    frequency *= intervals[std::min(std::size_t{2}, static_cast<std::size_t>(progress * 4.0F))];
  }
  if (voice.cue == Cue::harvest_overdrive) {
    constexpr std::array<float, 4> intervals{1.0F, 1.25F, 1.5F, 2.0F};
    frequency *= intervals[std::min(std::size_t{3}, static_cast<std::size_t>(progress * 6.0F))];
  }
  const float phase = advance(voice.phase, frequency);
  const float secondary = advance(voice.secondary_phase, frequency * 2.003F);
  random_ ^= random_ << 13U;
  random_ ^= random_ >> 17U;
  random_ ^= random_ << 5U;
  const float white = static_cast<float>(random_ & 0xffffU) / 32767.5F - 1.0F;
  voice.noise += 0.075F * (white - voice.noise);
  float sound = wave(phase) + 0.22F * wave(secondary) + 0.07F * wave(phase * 3.0F);
  if (voice.cue == Cue::hull_hit || voice.cue == Cue::death)
    sound = sound * 0.6F + voice.noise * 1.6F;
  if (voice.cue == Cue::laser)
    sound = sound * 0.7F + voice.noise * 0.4F;
  if (voice.cue == Cue::phase_dash)
    sound = sound * .65F + voice.noise * 1.1F;
  if (voice.cue == Cue::harvest_overdrive)
    sound = sound * .8F + .25F * wave(phase * .5F);
  const float attack = smooth(std::min(1.0F, voice.age / 0.009F));
  const float release = smooth(std::min(1.0F, (voice.duration - voice.age) / 0.085F));
  const float decay = 1.0F - 0.68F * progress;
  float envelope = attack * release * decay;
  if (voice.cue == Cue::warning)
    envelope *= 0.65F + 0.35F * wave(progress * 2.0F);
  voice.age += step;
  if (voice.age >= voice.duration)
    voice.active = false;
  return sound * envelope * tone.gain;
}

void Mixer::render(const std::span<float> output) noexcept {
  std::fill(output.begin(), output.end(), 0.0F);
  for (std::size_t frame = 0; frame < output.size() / 2U; ++frame) {
    tractor_level_ += (tractor_target_ - tractor_level_) * (step / 0.075F);
    duck_ += (1.0F - duck_) * (step / 0.3F);
    const float base = advance(tractor_phase_, 82.0F);
    const float upper = advance(tractor_secondary_phase_, 123.2F);
    const float hum = (wave(base) + 0.3F * wave(upper) + 0.12F * wave(base * 3.0F)) *
                      tractor_level_ * duck_ * 0.038F;
    float left = hum;
    float right = hum;
    for (auto& voice : voices_) {
      if (!voice.active)
        continue;
      // Real dash/boost/damage cues clear space in dense capture/fire feedback;
      // their own attack is never ducked by the background voices they displace.
      const float value = synthesize(voice) * (priority_cue(voice.cue) ? 1.F : duck_);
      left += value * voice.left;
      right += value * voice.right;
    }
    // Smooth bounded saturation prevents hard clipping during dense feedback.
    left *= gain_;
    right *= gain_;
    output[frame * 2U] = left / (1.0F + std::abs(left));
    output[frame * 2U + 1U] = right / (1.0F + std::abs(right));
    ++clock_;
  }
}
} // namespace human_resources::feedback
