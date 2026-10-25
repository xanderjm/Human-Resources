#include "product_haptics_backend.h"

namespace human_resources::platform::detail {
std::unique_ptr<HapticBackend> create_haptic_backend() { return {}; }
} // namespace human_resources::platform::detail
