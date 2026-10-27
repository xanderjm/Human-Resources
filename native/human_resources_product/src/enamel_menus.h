#pragma once
#include "ui_canvas.h"
namespace human_resources::product {
// Menu compositions share the same frames, text metrics and semantic actions.
// Returns false only for screens owned by another composition family.
[[nodiscard]] bool enamel_menu(detail::Canvas&, const View&);
}
