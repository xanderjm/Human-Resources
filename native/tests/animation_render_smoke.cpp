#include <SDL3/SDL.h>
#include <array>
#include <cmath>
#include <human_resources/renderer/style_renderer.h>
#include <iostream>
#include <numbers>

// Optional native GPU evidence driver, excluded from ordinary contract tests.
// Exercises the SAME renderer staging/draw path as the client; no Blender.
int main(int argc, char** argv) {
  if (argc != 3) {
    std::cerr << "usage: animation_render_smoke CONTENT_ROOT OUTPUT.rgba\n";
    return 2;
  }
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    std::cerr << SDL_GetError() << '\n';
    return 1;
  }
  auto* window = SDL_CreateWindow("Human Resources native animation smoke", 540, 960, 0);
  if (!window) {
    std::cerr << SDL_GetError() << '\n';
    SDL_Quit();
    return 1;
  }
  auto* output = SDL_IOFromFile(argv[2], "wb");
  bool ok = output != nullptr;
  {
    human_resources::renderer::StyleRenderer renderer;
    constexpr std::array<std::string_view, 9> ids{"cow", "pig", "sheep", "farmer-body", "resident-body", "soldier-body", "straw-hat", "nightcap", "ground-grass"};
    if (ok)
      ok = renderer.initialize(window, argv[1], 540, 960, ids, std::span(ids).first<6>());
    for (unsigned frame = 0; ok && frame < 180; ++frame) {
      SDL_Event event;
      while (SDL_PollEvent(&event)) {
      }
      using namespace human_resources;
      presentation::StyleFrame scene;
      scene.show_overlay = false;
      scene.active_delta_seconds = 1.F / 30.F;
      presentation::set_overhead_camera(scene, 9.F / 16.F, 8.F, {}, 27.F);
      scene.instances.push_back({.mesh = "ground-grass", .position = {0, -.035F, 0}, .scale = {20, 1, 20}, .texture_repeat = 3});
      const float t = static_cast<float>(frame) / 30.F;
      for (std::size_t n = 0; n < 6; ++n) {
        const float phase = t * .45F + static_cast<float>(n) * .4F;
        const float x = static_cast<float>(n % 2) * 3.4F - 1.7F + .65F * std::sin(phase);
        const float z = static_cast<float>(n / 2) * 4.F - 4.F + .8F * std::cos(phase);
        const float vx = .65F * .45F * std::cos(phase), vz = -.8F * .45F * std::sin(phase);
        const bool warning = n == 5 && t > 2 && t < 3;
        const bool capture = n == 3 && t > 3 && t < 4.6F;
        const auto id = static_cast<std::uint64_t>(n + 1);
        scene.instances.push_back({.mesh = ids[n], .position = {x, 0, z}, .rotation = {0, std::atan2(vx, vz), 0}, .stable_id = id, .animation = {.speed = std::hypot(vx, vz), .warning = warning, .capturing = capture, .enabled = true}});
        if (n == 3 || n == 4) {
          const auto rest = presentation::model_matrix({.position = {0, n == 3 ? 1.30F : 1.32F, n == 3 ? 0.F : -.04F}, .rotation = {0, n == 3 ? std::numbers::pi_v<float> : 0.F, 0}});
          scene.instances.push_back({.mesh = n == 3 ? "straw-hat" : "nightcap", .attachment_parent_id = id, .attachment_local = rest});
        }
      }
      ok = renderer.render(scene, output, false);
      if (ok && (frame == 0 || frame == 179)) {
        const auto stats = renderer.stats();
        std::cout << "native-animation frame=" << frame << " backend=" << renderer.backend() << " draws=" << stats.world_draws << " shadows=" << stats.shadow_draws << '\n';
      }
    }
    if (ok)
      ok = renderer.wait_idle();
  }
  if (output)
    SDL_CloseIO(output);
  if (!ok)
    std::cerr << "native-animation failed: " << SDL_GetError() << '\n';
  SDL_DestroyWindow(window);
  SDL_Quit();
  return ok ? 0 : 1;
}
