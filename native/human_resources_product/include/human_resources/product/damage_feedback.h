#pragma once
#include <algorithm>
#include <array>
#include <human_resources/gameplay/gameplay.h>
#include <string_view>
namespace human_resources::product {
// Read-only committed damage presentation. Negative age means no recent hit;
// hull takes precedence over shield when both resolve at the same active time.
struct DamageFeedback final {
  float age{-1};
  bool hull{};
  gameplay::DamageCause cause{};
};
// One short white -> yellow -> red decay, never a repeating strobe.
[[nodiscard]] inline std::array<float, 4> impact_flash(float strength) {
  strength = std::clamp(strength, 0.F, 1.F);
  const float white = std::clamp((strength - .7F) / .2F, 0.F, 1.F);
  const float yellow = std::clamp((strength - .3F) / .4F, 0.F, 1.F);
  return {1.F, .035F + .965F * yellow, .015F + .985F * white,
      std::min(1.F, strength * 2.F)};
}
// Visible and accessible copy shares the actual committed damage provenance.
[[nodiscard]] inline std::string_view damage_label(DamageFeedback damage) {
  switch (damage.cause) {
  case gameplay::DamageCause::contact: return damage.hull ? "HULL: COLLISION" : "SHIELD: COLLISION";
  case gameplay::DamageCause::projectile: return damage.hull ? "HULL: SHOT" : "SHIELD: SHOT";
  default: return damage.hull ? "HULL HIT" : "SHIELD HIT";
  }
}
[[nodiscard]] inline std::string_view damage_announcement(DamageFeedback damage) {
  switch (damage.cause) {
  case gameplay::DamageCause::contact: return damage.hull ? "Collision damaged hull. " : "Collision hit shield. ";
  case gameplay::DamageCause::projectile: return damage.hull ? "Shot damaged hull. " : "Shot hit shield. ";
  default: return damage.hull ? "Hull hit. " : "Shield hit. ";
  }
}
} // namespace human_resources::product
