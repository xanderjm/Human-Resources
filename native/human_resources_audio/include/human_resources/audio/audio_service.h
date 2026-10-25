#pragma once

#include <cstdint>

namespace human_resources::audio {

enum class Bus : std::uint8_t {
  master,
  music,
  effects,
  interface,
  ambience,
  voice,
};

enum class Interruption : std::uint8_t {
  began,
  ended_should_resume,
  ended_should_remain_paused,
};

struct Cue final {
  std::uint32_t code{};
  Bus bus{Bus::effects};
  float gain{1.0F};
  float pan{};
  bool loop{};
};

// Product logic emits semantic cue codes. A platform implementation owns decoding,
// voices, devices, focus, routing, streaming, and actual asset selection.
class AudioService {
public:
  virtual ~AudioService() = default;
  [[nodiscard]] virtual bool available() const noexcept = 0;
  virtual void play(const Cue& cue) noexcept = 0;
  virtual void stop(std::uint32_t cue_code) noexcept = 0;
  virtual void stop_all() noexcept = 0;
  virtual void set_bus_gain(Bus bus, float gain) noexcept = 0;
  [[nodiscard]] virtual float bus_gain(Bus bus) const noexcept = 0;
  virtual void handle_interruption(Interruption interruption) noexcept = 0;
};

class NullAudioService final : public AudioService {
public:
  [[nodiscard]] bool available() const noexcept override {
    return false;
  }
  void play(const Cue& cue) noexcept override;
  void stop(std::uint32_t cue_code) noexcept override;
  void stop_all() noexcept override;
  void set_bus_gain(Bus bus, float gain) noexcept override;
  [[nodiscard]] float bus_gain(Bus bus) const noexcept override;
  void handle_interruption(Interruption interruption) noexcept override;
  [[nodiscard]] bool interrupted() const noexcept {
    return interrupted_;
  }
  [[nodiscard]] bool resume_allowed() const noexcept {
    return resume_allowed_;
  }

private:
  float master_gain_{1.0F};
  float music_gain_{1.0F};
  float effects_gain_{1.0F};
  float interface_gain_{1.0F};
  float ambience_gain_{1.0F};
  float voice_gain_{1.0F};
  bool interrupted_{};
  bool resume_allowed_{true};
};

enum class HapticCue : std::uint8_t { selection,
  confirmation,
  warning,
  error };

// Focused capability for a selected native adapter. Unsupported devices and
// user-disabled feedback report false; product behavior must still complete.
class HapticService {
public:
  virtual ~HapticService() = default;
  [[nodiscard]] virtual bool available() const noexcept = 0;
  [[nodiscard]] virtual bool perform(HapticCue cue) noexcept = 0;
};

class NullHapticService final : public HapticService {
public:
  [[nodiscard]] bool available() const noexcept override {
    return false;
  }
  [[nodiscard]] bool perform(HapticCue) noexcept override {
    return false;
  }
};

} // namespace human_resources::audio
