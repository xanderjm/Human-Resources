#include "test_support.h"

#include <human_resources/audio/product_audio.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <limits>
#include <string>
#include <vector>

namespace {
using human_resources::feedback::Cue;
using human_resources::feedback::Mixer;
using test_support::require;
float peak(const std::span<const float> samples) {
  float value{};
  for (const float sample : samples) {
    require(std::isfinite(sample), "PCM samples must be finite");
    value = std::max(value, std::abs(sample));
  }
  return value;
}
void cue_envelopes() {
  for (std::size_t index = 0; index < static_cast<std::size_t>(Cue::count); ++index) {
    Mixer mixer;
    require(mixer.play(static_cast<Cue>(index)), "first semantic cue accepted");
    std::vector<float> samples(static_cast<std::size_t>(Mixer::sample_rate) * 3U);
    mixer.render(samples);
    require(peak(samples) > 0.005F, "every cue produces audible PCM");
    require(peak(samples) < 0.9F, "individual cue has headroom");
    require(samples[0] == 0.0F && samples[1] == 0.0F, "attack starts at zero");
    require(peak(std::span(samples).last(4096)) == 0.0F, "cue release ends in silence");
    float maximum_step{};
    for (std::size_t sample = 2; sample < samples.size(); sample += 2) {
      maximum_step = std::max(maximum_step, std::abs(samples[sample] - samples[sample - 2]));
    }
    require(maximum_step < 0.04F, "cue changes stay smooth without hard waveform discontinuities");
    require(mixer.active_voices() == 0, "finished cue releases its voice");
  }
}
void mixing_policy() {
  Mixer mixer;
  std::array<float, 512> samples{};
  require(!mixer.play(Cue::count), "invalid semantic cue rejected");
  require(!mixer.play(static_cast<Cue>(255)), "out of range semantic cue rejected");
  mixer.set_tractor_active(true, std::numeric_limits<float>::infinity());
  mixer.render(samples);
  require(peak(samples) == 0.0F, "non-finite tractor gain becomes silence");
  mixer.set_tractor_active(true);
  for (int block = 0; block < 2000; ++block) {
    for (std::size_t cue = 0; cue < static_cast<std::size_t>(Cue::count); ++cue) {
      static_cast<void>(mixer.play(static_cast<Cue>(cue), std::numeric_limits<float>::quiet_NaN()));
    }
    mixer.render(samples);
    require(mixer.active_voices() <= Mixer::voice_limit, "dense combat respects voice cap");
    require(peak(samples) < 1.0F, "dense combat cannot clip");
  }
  mixer.stop_all();
  mixer.render(samples);
  require(peak(samples) == 0.0F, "stop clears voices and continuous hum");
  require(mixer.play(Cue::capture), "stop permits a new cue without stale cooldown");
  mixer.set_gain(std::numeric_limits<float>::quiet_NaN());
  mixer.render(samples);
  require(peak(samples) == 0.0F, "invalid master gain mutes");
}
void panning_and_throttle() {
  Mixer mixer;
  require(mixer.play(Cue::laser, -1.0F), "laser accepted");
  require(!mixer.play(Cue::laser), "same-frame burst throttled");
  std::array<float, 8192> samples{};
  mixer.render(samples);
  float left_peak{};
  for (std::size_t frame = 0; frame < samples.size() / 2U; ++frame) {
    left_peak = std::max(left_peak, std::abs(samples[frame * 2U]));
    require(samples[frame * 2U + 1U] == 0.0F, "left pan leaves right silent");
  }
  require(left_peak > 0.005F, "left pan is audible");
  require(mixer.play(Cue::laser, 4.0F), "cooldown ends and pan clamps");
  mixer.stop_all();
  mixer.set_tractor_active(true);
  for (int block = 0; block < 20; ++block)
    mixer.render(samples);
  require(peak(samples) > 0.01F, "tractor fades up");
  mixer.set_tractor_active(false);
  for (int block = 0; block < 20; ++block)
    mixer.render(samples);
  require(peak(samples) < 0.000001F, "tractor release fades away");
  std::array<float, 3> odd{1.0F, 1.0F, 1.0F};
  mixer.stop_all();
  mixer.render(odd);
  require(peak(odd) == 0.0F, "odd destination remainder is cleared safely");
}
void real_outcome_coalescing() {
  using human_resources::feedback::select_gameplay_cues;
  const auto empty = select_gameplay_cues({});
  require(std::count(empty.begin(), empty.end(), true) == 0, "no outcomes cannot invent sound");
  const auto capture = select_gameplay_cues({.capture = true});
  require(capture[static_cast<unsigned>(Cue::capture)], "ordinary capture keeps its soft resolution");
  const auto sweep = select_gameplay_cues({.capture = true, .sweep_capture = true});
  require(sweep[static_cast<unsigned>(Cue::sweep_capture)] && std::count(sweep.begin(), sweep.end(), true) == 1,
      "skill capture replaces the ordinary chime with one intake zip");
  const auto dense = select_gameplay_cues({.capture = true, .laser = true, .warning = true, .hit = true, .sweep_capture = true, .harvest_overdrive = true, .phase_dash = true});
  require(dense[static_cast<unsigned>(Cue::harvest_overdrive)] && dense[static_cast<unsigned>(Cue::phase_dash)] &&
              dense[static_cast<unsigned>(Cue::warning)] && dense[static_cast<unsigned>(Cue::hull_hit)] &&
              std::count(dense.begin(), dense.end(), true) == 4,
      "boost owns the capture payoff and dash owns its counter-volley while actual warning/damage remain audible");
}
void priority_reserve_under_pressure() {
  Mixer mixer;
  std::array<float, 128> samples{};
  bool filled = false;
  for (unsigned block = 0; block < 1500 && !filled; ++block) {
    for (const Cue cue : {Cue::capture, Cue::laser, Cue::warning, Cue::purchase, Cue::ui, Cue::sweep_capture})
      static_cast<void>(mixer.play(cue));
    filled = mixer.active_voices() == 12;
    if (!filled)
      mixer.render(samples);
  }
  require(filled, "dense ordinary capture/fire workload fills its twelve-voice allowance");
  require(mixer.play(Cue::harvest_overdrive) && mixer.play(Cue::phase_dash) &&
              mixer.play(Cue::hull_hit) && mixer.play(Cue::death),
      "four reserved voices preserve real urgent outcomes");
  require(mixer.active_voices() == Mixer::voice_limit && !mixer.play(Cue::upgrade),
      "even priority feedback cannot exceed sixteen voices");
  for (unsigned block = 0; block < 1000; ++block) {
    mixer.render(samples);
    require(peak(samples) < 1.F, "full sixteen-voice tail remains finite and below clipping");
  }
}
void write_u32(std::ofstream& file, const std::uint32_t value, const unsigned bytes = 4) {
  for (unsigned byte = 0; byte < bytes; ++byte)
    file.put(static_cast<char>((value >> (8U * byte)) & 255U));
}
void diagnostic_wav(const std::string& path) {
  constexpr std::size_t seconds = 15;
  std::vector<float> pcm(seconds * static_cast<std::size_t>(Mixer::sample_rate) * 2U);
  Mixer mixer;
  constexpr std::size_t block = 480;
  for (std::size_t frame = 0; frame < pcm.size() / 2U; frame += block) {
    const auto moment = frame / block;
    if (moment == 30)
      mixer.set_tractor_active(true);
    if (moment == 90 || moment == 160 || moment == 220)
      static_cast<void>(mixer.play(Cue::capture));
    if (moment == 250 || moment == 285)
      static_cast<void>(mixer.play(Cue::sweep_capture));
    if (moment == 310 || moment == 340 || moment == 370)
      static_cast<void>(mixer.play(Cue::laser));
    if (moment == 440)
      static_cast<void>(mixer.play(Cue::warning));
    if (moment == 540)
      static_cast<void>(mixer.play(Cue::hull_hit));
    if (moment == 640) {
      mixer.set_tractor_active(false);
      static_cast<void>(mixer.play(Cue::upgrade));
    }
    if (moment == 850)
      static_cast<void>(mixer.play(Cue::purchase));
    if (moment == 800)
      static_cast<void>(mixer.play(Cue::harvest_overdrive));
    if (moment == 960)
      static_cast<void>(mixer.play(Cue::phase_dash));
    if (moment == 1000)
      static_cast<void>(mixer.play(Cue::ui));
    if (moment == 1100)
      static_cast<void>(mixer.play(Cue::death));
    mixer.render(std::span(pcm).subspan(frame * 2U, block * 2U));
  }
  std::ofstream file(path, std::ios::binary);
  require(file.is_open(), "diagnostic output opens");
  file.write("RIFF", 4);
  write_u32(file, static_cast<std::uint32_t>(36U + pcm.size() * 2U));
  file.write("WAVEfmt ", 8);
  write_u32(file, 16);
  write_u32(file, 1, 2);
  write_u32(file, 2, 2);
  write_u32(file, Mixer::sample_rate);
  write_u32(file, Mixer::sample_rate * 4U);
  write_u32(file, 4, 2);
  write_u32(file, 16, 2);
  file.write("data", 4);
  write_u32(file, static_cast<std::uint32_t>(pcm.size() * 2U));
  for (const float sample : pcm)
    write_u32(file, static_cast<std::uint16_t>(static_cast<std::int16_t>(sample * 32767.0F)), 2);
  require(file.good(), "diagnostic WAV writes fully");
}
} // namespace
int main(const int argc, char** argv) {
  cue_envelopes();
  mixing_policy();
  panning_and_throttle();
  real_outcome_coalescing();
  priority_reserve_under_pressure();
  if (argc > 1)
    diagnostic_wav(argv[1]);
  std::cout << "Audio feedback contracts passed\n";
}
