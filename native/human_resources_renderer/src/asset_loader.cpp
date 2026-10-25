#include "asset_loader.h"

#include <SDL3/SDL.h>

#include <array>
#include <filesystem>
#include <string>

namespace human_resources::renderer {

std::vector<std::byte> load_runtime_asset(const std::string_view relative_path,
    human_resources::diagnostics::Logger& logger) {
  const std::filesystem::path relative{"assets"};
  const auto asset_path = relative / relative_path;
  const char* base = SDL_GetBasePath();
  const auto full_path = base == nullptr ? asset_path : std::filesystem::path(base) / asset_path;

  std::size_t size = 0;
  void* memory = SDL_LoadFile(full_path.string().c_str(), &size);
  if (memory == nullptr && full_path != asset_path) {
    memory = SDL_LoadFile(asset_path.string().c_str(), &size);
  }
  if (memory == nullptr) {
    const std::array fields{
        human_resources::diagnostics::Field::text_value("path", relative_path),
        human_resources::diagnostics::Field::text_value("sdl_error", SDL_GetError()),
    };
    (void)logger.try_record({.severity = human_resources::diagnostics::Severity::error,
        .subsystem = human_resources::diagnostics::Subsystem::asset,
        .code = 1001U,
        .message = "runtime asset load failed",
        .fields = fields});
    return {};
  }
  const auto* begin = static_cast<const std::byte*>(memory);
  std::vector<std::byte> bytes(begin, begin + size);
  SDL_free(memory);
  return bytes;
}

} // namespace human_resources::renderer
