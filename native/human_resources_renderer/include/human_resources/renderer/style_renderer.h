#pragma once
#include <SDL3/SDL.h>
#include <human_resources/renderer/capture_extent.h>
#include <human_resources/presentation/style_scene.h>
#include <memory>
#include <span>
#include <string_view>
namespace human_resources::renderer {
struct StyleRenderStats final {
  unsigned world_draws{}, shadow_draws{}, visible_instances{}, shadow_instances{}, culled_instances{};
  std::uint64_t triangles{};
};
// Development viewer composed from the same native SDL_GPU backend and cooked
// HLSL path as the client. GPU lifetimes and readback stay in the renderer.
class StyleRenderer final {
public:
  StyleRenderer();
  ~StyleRenderer();
  StyleRenderer(const StyleRenderer&) = delete;
  StyleRenderer& operator=(const StyleRenderer&) = delete;
  // Required animated pairs load during startup. An empty animation span keeps
  // static development previews independent of HRA content.
  [[nodiscard]] bool initialize(SDL_Window* window, std::string_view content_root, unsigned width, unsigned height, std::span<const std::string_view> preload = {}, std::span<const std::string_view> animated_preload = {});
  // Normal UI is rasterized on the physical swapchain after the capped world.
  // Capture is raw RGBA8, never swapchain readback. Empty capture_pixels retains
  // world-sized capture output; explicit pixels must match the window aspect.
  // Capture targets/readback are allocated only when capture is requested.
  [[nodiscard]] bool render(const presentation::StyleFrame& frame, SDL_IOStream* capture = nullptr, bool present = true, CaptureExtent capture_pixels = {});
  [[nodiscard]] bool wait_idle();
  // Run boundary only. Waits submitted actor buffers, drops their pose/stream
  // caches and retains all prepared assets/pipelines for a clean same-ID spawn.
  void reset_actor_animations();
  [[nodiscard]] bool resize(unsigned width, unsigned height);
  [[nodiscard]] StyleRenderStats stats() const;
  [[nodiscard]] const char* backend() const;

private:
  struct State;
  std::unique_ptr<State> state_;
};
} // namespace human_resources::renderer
