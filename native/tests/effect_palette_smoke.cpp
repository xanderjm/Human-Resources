#include <SDL3/SDL.h>
#include <array>
#include <human_resources/renderer/style_renderer.h>
#include <iostream>

// Optional GPU colour-contract fixture. Uses the production renderer, its
// unchanged MSAA/shadow passes and the cooked white cube. No gameplay overrides.
int main(int argc, char** argv) {
  if (argc != 3) {
    std::cerr << "usage: effect_palette_smoke CONTENT_ROOT OUTPUT.rgba\n";
    return 2;
  }
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    std::cerr << SDL_GetError() << '\n';
    return 1;
  }
  auto* window = SDL_CreateWindow("Human Resources effect palette", 512, 384, SDL_WINDOW_HIDDEN);
  auto* output = window ? SDL_IOFromFile(argv[2], "wb") : nullptr;
  bool ok = output != nullptr;
  {
    using namespace human_resources;
    renderer::StyleRenderer renderer;
    constexpr std::array<std::string_view, 1> ids{"cube"};
    if (ok)
      ok = renderer.initialize(window, argv[1], 512, 384, ids);
    presentation::StyleFrame frame;
    frame.show_overlay = false;
    frame.camera = presentation::model_matrix({});
    frame.light = frame.camera;
    constexpr std::array<std::array<float, 4>, 6> colors{{
        {.5F, 1, .76F, 1}, {.29F, .96F, .71F, 1}, {1, .28F, .23F, 1},
        {1, .24F, .13F, 1}, {.55F, .93F, 1, 1}, {.02F, .04F, .07F, 1}}};
    for (unsigned row = 0; row < 3; ++row)
      for (unsigned column = 0; column < colors.size(); ++column) {
        auto color = colors[column];
        if (row == 1)
          color[3] = .5F;
        frame.instances.push_back({.mesh = "cube",
            .position = {-1.F + (static_cast<float>(column) + .5F) / 3.F, 2.F / 3.F * (1.F - static_cast<float>(row)), .5F},
            .scale = {.12F, .24F, .02F}, .tint = color, .effect = row != 2, .casts_shadow = false});
      }
    if (ok)
      ok = renderer.render(frame, output, false) && renderer.wait_idle();
    if (ok) {
      const auto stats = renderer.stats();
      std::cout << "backend=" << renderer.backend() << " world_draws=" << stats.world_draws
                << " shadow_draws=" << stats.shadow_draws << " triangles=" << stats.triangles << '\n';
    }
  }
  if (output)
    SDL_CloseIO(output);
  if (!ok)
    std::cerr << SDL_GetError() << '\n';
  if (window)
    SDL_DestroyWindow(window);
  SDL_Quit();
  return ok ? 0 : 1;
}
