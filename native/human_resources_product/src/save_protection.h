#pragma once
#include <human_resources/product/presentation.h>

namespace human_resources::product {
[[nodiscard]] inline std::string_view protection_notice(const View& view) {
  if(view.can_reset_save)return "A fresh account starts only after the backup succeeds.";
  if(view.can_bank_checkpoint)return "Saved credits bank only after a verified backup succeeds.";
  return "Your original progress stays protected while you retry.";
}
} // namespace human_resources::product
