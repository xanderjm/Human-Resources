#pragma once
#include <human_resources/product/presentation.h>
#include <optional>

namespace human_resources::product {
// Reference portrait compositions. Constrained content uses the measured
// reader from the same component library; visual acceptance is tracked apart.
[[nodiscard]] std::optional<Interface> enamel_interface(const View&);
}
