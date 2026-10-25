#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <human_resources/presentation/scenery.h>
#include <human_resources/presentation/style_scene.h>
#include <human_resources/renderer/preview_viewport.h>
#include <human_resources/renderer/style_renderer.h>
#include <string>
#include <string_view>
#include <vector>

namespace {
struct Options final {
  std::string content, capture, profile;
  unsigned width{720}, height{1280}, frames{}, fps{60};
  unsigned window_width{390}, window_height{844};
  double start{};
  int scenery{-1};
  unsigned seed{514};
  bool offscreen{};
  bool resize_test{};
  bool paced{};
};
bool number(std::string_view value, unsigned& output) {
  auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), output);
  return error == std::errc{} && end == value.data() + value.size();
}
bool parse(int argc, char** argv, Options& o) {
  for (int i = 1; i < argc; ++i) {
    const std::string_view arg = argv[i];
    if (i + 1 >= argc)
      return false;
    const std::string_view value = argv[++i];
    if (arg == "--scenery") {
      if (value == "farmland")
        o.scenery = 0;
      else if (value == "suburbia")
        o.scenery = 1;
      else if (value == "desert")
        o.scenery = 2;
      else
        return false;
    } else if (arg == "--seed") {
      if (!number(value, o.seed))
        return false;
    } else if (arg == "--offscreen" || arg == "--resize-test" || arg == "--paced") {
      unsigned value_number{};
      if (!number(value, value_number) || value_number > 1)
        return false;
      (arg == "--offscreen" ? o.offscreen : arg == "--resize-test" ? o.resize_test
                                                                   : o.paced) = value_number == 1;
    } else if (arg == "--profile")
      o.profile = value;
    else if (arg == "--content")
      o.content = value;
    else if (arg == "--capture")
      o.capture = value;
    else if (arg == "--width") {
      if (!number(value, o.width))
        return false;
    } else if (arg == "--height") {
      if (!number(value, o.height))
        return false;
    } else if (arg == "--window-width") {
      if (!number(value, o.window_width))
        return false;
    } else if (arg == "--window-height") {
      if (!number(value, o.window_height))
        return false;
    } else if (arg == "--frames") {
      if (!number(value, o.frames))
        return false;
    } else if (arg == "--fps") {
      if (!number(value, o.fps))
        return false;
    } else if (arg == "--time") {
      unsigned n = 0;
      if (!number(value, n))
        return false;
      o.start = static_cast<double>(n);
    } else
      return false;
  }
  return o.width >= 320 && o.width <= 2560 && o.height >= 320 && o.height <= 2560 && o.width % 4 == 0 && o.height % 4 == 0 && o.window_width >= 240 && o.window_width <= 2560 && o.window_height >= 240 && o.window_height <= 2560 && o.fps > 0 && o.fps <= 120 && o.frames <= 36000;
}
int run(const Options& o) {
  const bool live = !o.offscreen && o.capture.empty();
  auto window_extent = human_resources::renderer::PreviewExtent{o.window_width, o.window_height};
  SDL_Rect usable{};
  if (live && SDL_GetDisplayUsableBounds(SDL_GetPrimaryDisplay(), &usable) && usable.w > 64 && usable.h > 64)
    window_extent = human_resources::renderer::fit_preview_extent(window_extent.width, window_extent.height, static_cast<unsigned>(usable.w - 32), static_cast<unsigned>(usable.h - 48));
  SDL_Window* window = SDL_CreateWindow("Human Resources — scenery", static_cast<int>(window_extent.width), static_cast<int>(window_extent.height), SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY | (!live ? SDL_WINDOW_HIDDEN : 0));
  if (!window)
    return 1;
  bool ok = true;
  SDL_SetWindowMinimumSize(window, 240, 240);
  unsigned render_width = o.width, render_height = o.height;
  const auto update_extent = [&]() {
    int w{}, h{};
    if (!SDL_GetWindowSizeInPixels(window, &w, &h) || w <= 0 || h <= 0)
      return false;
    const auto extent = human_resources::renderer::fit_preview_extent(static_cast<unsigned>(w), static_cast<unsigned>(h), 1280, 1280);
    render_width = extent.width;
    render_height = extent.height;
    return true;
  };
  if (live && !update_extent()) {
    SDL_DestroyWindow(window);
    return 1;
  }
  SDL_IOStream* capture = nullptr;
  if (!o.capture.empty()) {
    capture = SDL_IOFromFile(o.capture.c_str(), "wb");
    if (!capture) {
      SDL_DestroyWindow(window);
      return 1;
    }
  }
  {
    human_resources::renderer::StyleRenderer renderer;
    const auto initialization_start = SDL_GetTicksNS();
    ok = renderer.initialize(window, o.content, render_width, render_height, o.scenery < 0 ? std::span<const std::string_view>{} : human_resources::presentation::scenery_materials(static_cast<human_resources::presentation::SceneryTheme>(o.scenery)));
    SDL_Log("style initialization_ms=%.3f", static_cast<double>(SDL_GetTicksNS() - initialization_start) / 1.e6);
    SDL_Log("preview viewport window=%ux%u render=%ux%u live=%u", window_extent.width, window_extent.height, render_width, render_height, live ? 1U : 0U);
    const Uint64 start = SDL_GetTicksNS();
    Uint64 previous = start;
    Uint64 next_frame = start;
    double seconds = o.start;
    unsigned frames = 0;
    std::vector<double> generation_ms, render_ms, interval_ms;
    Uint64 previous_frame{};
    Uint64 measured_start{};
    human_resources::renderer::StyleRenderStats totals{};
    unsigned resize_mask{};
    bool running = true;
    while (ok && running) {
      SDL_Event event;
      while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT)
          running = false;
        if (event.type == SDL_EVENT_KEY_DOWN && event.key.scancode == SDL_SCANCODE_ESCAPE)
          running = false;
      }
      if (!running)
        break;
      if (live) {
        if (o.resize_test && (frames == 60 || frames == 120 || frames == 180)) {
          const unsigned stage = frames / 60;
          const int w = stage == 1 ? 844 : stage == 2 ? 600
                                                      : 390;
          const int h = stage == 1 ? 390 : stage == 2 ? 600
                                                      : 844;
          if (!SDL_SetWindowSize(window, w, h)) {
            ok = false;
            break;
          }
        }
        const unsigned old_width = render_width, old_height = render_height;
        if (!update_extent() || !renderer.resize(render_width, render_height)) {
          ok = false;
          break;
        }
        if (render_width != old_width || render_height != old_height)
          SDL_Log("preview resize render=%ux%u aspect=%.6f", render_width, render_height, static_cast<double>(render_width) / render_height);
        if (o.resize_test) {
          int w{}, h{};
          SDL_GetWindowSize(window, &w, &h);
          if (w == 844 && h == 390)
            resize_mask |= 1;
          if (w == 600 && h == 600)
            resize_mask |= 2;
          if (w == 390 && h == 844)
            resize_mask |= 4;
        }
      }
      const Uint64 now = SDL_GetTicksNS();
      if (frames >= 30 && previous_frame)
        interval_ms.push_back(static_cast<double>(now - previous_frame) / 1.e6);
      previous_frame = now;
      if (capture || !o.profile.empty())
        seconds = o.start + static_cast<double>(frames) / static_cast<double>(o.fps);
      else if ((SDL_GetWindowFlags(window) & SDL_WINDOW_INPUT_FOCUS) != 0)
        seconds += static_cast<double>(now - previous) / 1000000000.;
      previous = now;
      const float aspect = static_cast<float>(render_width) / static_cast<float>(render_height);
      const auto generation_start = SDL_GetTicksNS();
      auto frame = o.scenery < 0 ? human_resources::presentation::style_frame(static_cast<float>(seconds), aspect) : human_resources::presentation::generate_scenery({static_cast<human_resources::presentation::SceneryTheme>(o.scenery), o.seed, seconds * 2.4, aspect}).scene;
      const auto render_start = SDL_GetTicksNS();
      ok = renderer.render(frame, capture, !o.offscreen && capture == nullptr);
      if (ok && o.offscreen && !o.profile.empty())
        ok = renderer.wait_idle();
      if (!ok)
        break;
      if (!o.profile.empty() && frames >= 30) {
        if (generation_ms.empty())
          measured_start = generation_start;
        generation_ms.push_back(static_cast<double>(render_start - generation_start) / 1.e6);
        render_ms.push_back(static_cast<double>(SDL_GetTicksNS() - render_start) / 1.e6);
        const auto stats = renderer.stats();
        totals.world_draws += stats.world_draws;
        totals.shadow_draws += stats.shadow_draws;
        totals.visible_instances += stats.visible_instances;
        totals.culled_instances += stats.culled_instances;
        totals.triangles += stats.triangles;
      }
      ++frames;
      if (o.frames > 0 && frames >= o.frames)
        running = false;
      if (live && (o.profile.empty() || o.paced)) {
        const bool focused = o.paced || (SDL_GetWindowFlags(window) & SDL_WINDOW_INPUT_FOCUS) != 0;
        const auto interval = 1000000000ULL / (focused ? o.fps : 15U);
        next_frame += interval;
        const auto now_ticks = SDL_GetTicksNS();
        if (next_frame > now_ticks)
          SDL_DelayPrecise(next_frame - now_ticks);
        else
          next_frame = now_ticks;
      }
    }
    if (o.frames > 0 && frames != o.frames) {
      if (ok)
        SDL_SetError("Preview interrupted after %u of %u requested frames", frames, o.frames);
      ok = false;
    }
    if (ok && o.resize_test && resize_mask != 7) {
      ok = false;
      SDL_SetError("Resize smoke did not observe all viewport sizes");
    }
    SDL_Log("style-preview backend=%s frames=%u size=%ux%u fps=%u scripted_time=%.3f result=%s", renderer.backend(), frames, render_width, render_height, o.fps, static_cast<double>(seconds), ok ? "pass" : "fail");
    if (!ok)
      SDL_Log("Style preview failed: %s", SDL_GetError());
    if (!o.profile.empty() && !generation_ms.empty()) {
      const double elapsed = static_cast<double>(SDL_GetTicksNS() - measured_start) / 1.e6;
      const auto percentile = [](std::vector<double> samples, double quantile) {
        std::sort(samples.begin(), samples.end());
        return samples[static_cast<std::size_t>(quantile * static_cast<double>(samples.size() - 1))];
      };
      std::ofstream report(o.profile);
      const auto count = static_cast<double>(generation_ms.size());
      report << "{\"frames\":" << generation_ms.size() << ",\"width\":" << render_width << ",\"height\":" << render_height
             << ",\"average_frame_ms\":" << elapsed / static_cast<double>(generation_ms.size())
             << ",\"generation_p50_ms\":" << percentile(generation_ms, .5) << ",\"generation_p95_ms\":" << percentile(generation_ms, .95)
             << ",\"render_p50_ms\":" << percentile(render_ms, .5) << ",\"render_p95_ms\":" << percentile(render_ms, .95)
             << ",\"frame_interval_p50_ms\":" << percentile(interval_ms, .5) << ",\"frame_interval_p95_ms\":" << percentile(interval_ms, .95)
             << ",\"world_draws_mean\":" << totals.world_draws / count << ",\"shadow_draws_mean\":" << totals.shadow_draws / count
             << ",\"visible_instances_mean\":" << totals.visible_instances / count << ",\"culled_instances_mean\":" << totals.culled_instances / count
             << ",\"triangles_mean\":" << static_cast<double>(totals.triangles) / count << ",\"resize_mask\":" << resize_mask
             << ",\"result\":\"" << (ok ? "pass" : "fail") << "\"}\n";
      ok = ok && report.good();
    }
  }
  if (capture && !SDL_CloseIO(capture))
    ok = false;
  SDL_DestroyWindow(window);
  return ok ? 0 : 1;
}
} // namespace
int main(int argc, char** argv) {
  Options options;
  if (!parse(argc, argv, options)) {
    SDL_Log("Usage: --content path --capture raw.rgba --frames count --fps 30 --width 720 --height 1280 --time 0");
    return 1;
  }
  if (options.capture.empty() && options.scenery < 0)
    options.scenery = 0;
  if (!SDL_Init(SDL_INIT_VIDEO))
    return 1;
  if (options.content.empty()) {
    const auto base = std::filesystem::path(SDL_GetBasePath());
    for (const auto& candidate : {base / "style-preview/content", base / "../style-preview/content", std::filesystem::path("build/style-preview/content")}) {
      std::error_code error;
      if (std::filesystem::is_directory(candidate, error)) {
        options.content = candidate.lexically_normal().string();
        break;
      }
    }
    if (options.content.empty()) {
      SDL_Log("Scenery content is missing; run tools/cook_style_preview.py with the art Python, or pass --content path");
      SDL_Quit();
      return 1;
    }
  }
  const int result = run(options);
  SDL_Quit();
  return result;
}
