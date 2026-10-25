#include "test_support.h"

#include <human_resources/audio/audio_service.h>
#include <human_resources/ui/layout.h>

#include <cmath>
#include <iostream>
#include <limits>

namespace {

using test_support::require;
using test_support::require_near;

void null_audio_owns_stable_policy_without_a_runtime_dependency() {
  human_resources::audio::NullAudioService audio;
  require(!audio.available(), "no-audio fallback reports its missing playback capability");
  human_resources::audio::NullHapticService haptics;
  require(!haptics.available() && !haptics.perform(human_resources::audio::HapticCue::confirmation),
      "no-haptic fallback reports unavailable without blocking the product action");
  audio.set_bus_gain(human_resources::audio::Bus::music, 2.0F);
  require_near(audio.bus_gain(human_resources::audio::Bus::music), 1.0F, 0.001F, "audio gain clamps high values");
  audio.set_bus_gain(human_resources::audio::Bus::effects, -1.0F);
  require_near(audio.bus_gain(human_resources::audio::Bus::effects), 0.0F, 0.001F, "audio gain clamps low values");
  audio.set_bus_gain(human_resources::audio::Bus::interface, std::numeric_limits<float>::quiet_NaN());
  require(std::isfinite(audio.bus_gain(human_resources::audio::Bus::interface)),
      "audio gain rejects non-finite configuration");
  audio.play({.code = 99U, .bus = human_resources::audio::Bus::interface});
  audio.handle_interruption(human_resources::audio::Interruption::began);
  require(audio.interrupted() && !audio.resume_allowed(), "audio interruption state is retained");
  audio.handle_interruption(human_resources::audio::Interruption::ended_should_resume);
  require(!audio.interrupted() && audio.resume_allowed(), "audio resume policy is explicit");
  audio.stop_all();
}

void ui_actions_are_semantic_and_renderer_independent() {
  const human_resources::ui::DesignTokens tokens;
  auto root = human_resources::ui::components::screen(1U, tokens);
  root.children.push_back(human_resources::ui::components::button(2U, "Play", tokens));
  const auto result = human_resources::ui::layout(root, {0.0F, 0.0F, 320.0F, 180.0F}, {}, {});
  require(result.valid, "presentation model lays out without SDL or a renderer");
  require(human_resources::ui::hit_test(root, {40.0F, 40.0F}) == 2U,
      "UI exposes a semantic node identifier instead of mutating gameplay");
  require(human_resources::ui::build_draw_list(root).size() == 2U,
      "renderer consumes value draw commands rather than UI ownership");
}

} // namespace

int main() {
  null_audio_owns_stable_policy_without_a_runtime_dependency();
  ui_actions_are_semantic_and_renderer_independent();
  std::cout << "presentation boundary contracts passed\n";
  return 0;
}
