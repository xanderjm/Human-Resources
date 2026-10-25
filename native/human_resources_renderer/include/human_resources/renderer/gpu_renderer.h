#pragma once

#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>

#include <human_resources/diagnostics/diagnostics.h>
#include <human_resources/ui/layout.h>

#include <span>
#include <string_view>

namespace human_resources::renderer {

class GpuRenderer final {
public:
  explicit GpuRenderer(human_resources::diagnostics::Logger& logger) noexcept;
  GpuRenderer(const GpuRenderer&) = delete;
  GpuRenderer& operator=(const GpuRenderer&) = delete;
  ~GpuRenderer();

  [[nodiscard]] bool initialize(SDL_Window* window);
  [[nodiscard]] bool render(human_resources::ui::Size logical_size,
      std::span<const human_resources::ui::DrawCommand> commands);
  [[nodiscard]] std::string_view backend_name() const noexcept;
  [[nodiscard]] std::string_view device_name() const noexcept;
  [[nodiscard]] std::string_view driver_version() const noexcept;
  void shutdown();

private:
  [[nodiscard]] SDL_GPUShader* load_shader(bool vertex) const;
  void log_error(std::uint32_t code, const char* message) const noexcept;
  void log_shader_error(std::uint32_t code, const char* message,
      std::string_view path, bool vertex) const noexcept;

  human_resources::diagnostics::Logger& logger_;
  SDL_Window* window_{};
  SDL_GPUDevice* device_{};
  SDL_GPUGraphicsPipeline* pipeline_{};
  bool window_claimed_{};
};

} // namespace human_resources::renderer
