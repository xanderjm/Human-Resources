#pragma once
#include <human_resources/product/presentation.h>
#include <optional>
namespace human_resources::product {
[[nodiscard]] std::optional<Interface> enamel_hud(const View&,const GameplayFraming&);
}
