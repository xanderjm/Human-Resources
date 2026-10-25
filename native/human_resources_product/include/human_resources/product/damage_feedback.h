#pragma once
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
