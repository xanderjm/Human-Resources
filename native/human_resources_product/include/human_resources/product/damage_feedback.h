#pragma once
namespace human_resources::product {
// Read-only committed damage presentation. Negative age means no recent hit;
// hull takes precedence over shield when both resolve at the same active time.
struct DamageFeedback final {
  float age{-1};
  bool hull{};
};
} // namespace human_resources::product
