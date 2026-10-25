#pragma once

#include <human_resources/diagnostics/diagnostics.h>

#include <cstddef>
#include <string_view>
#include <vector>

namespace human_resources::renderer {

[[nodiscard]] std::vector<std::byte> load_runtime_asset(std::string_view relative_path,
    human_resources::diagnostics::Logger& logger);

} // namespace human_resources::renderer
