// Manual host probe; its CPU timings do not qualify GPU or mobile performance.
#include <human_resources/gameplay/gameplay.h>
#include <human_resources/gameplay/tuning.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <new>

namespace {
std::uint64_t allocations{};
}
// Product containers use ordinary alignment. Count these allocations without
// changing the production allocator or adding a profiling dependency.
void* operator new(std::size_t bytes) {
  if (void* memory = std::malloc(bytes == 0 ? 1 : bytes)) { ++allocations; return memory; }
  throw std::bad_alloc();
}
void* operator new[](std::size_t bytes) { return ::operator new(bytes); }
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

namespace {
namespace gp = human_resources::gameplay;
struct Sample final {
  double active{}, cpu_nanoseconds{}, longest_run{};
  std::uint64_t calls{}, deaths{}, picks{}, active_allocations{}, draft_allocations{}, start_allocations{};
  std::size_t entities{}, projectiles{}, beams{}, shots{};
};
void print(const char* profile, unsigned minutes, const Sample& s) {
  std::printf("{\"profile\":\"%s\",\"aggregate_active_minutes\":%u,\"cpu_ms\":%.3f,\"mean_advance_us\":%.4f,"
      "\"calls\":%llu,\"deaths\":%llu,\"picks\":%llu,\"longest_run_seconds\":%.3f,"
      "\"max_entities\":%zu,\"max_projectiles\":%zu,\"max_beams\":%zu,\"max_shots\":%zu,"
      "\"active_allocations\":%llu,\"draft_allocations\":%llu,\"start_allocations\":%llu}\n",
      profile, minutes, s.cpu_nanoseconds / 1000000, s.cpu_nanoseconds / static_cast<double>(s.calls) / 1000,
      static_cast<unsigned long long>(s.calls), static_cast<unsigned long long>(s.deaths),
      static_cast<unsigned long long>(s.picks), s.longest_run, s.entities, s.projectiles, s.beams, s.shots,
      static_cast<unsigned long long>(s.active_allocations), static_cast<unsigned long long>(s.draft_allocations),
      static_cast<unsigned long long>(s.start_allocations));
}
void profile(const char* name, const bool maxed) {
  gp::Game game;
  Sample result;
  std::uint64_t seed = 437;
  std::size_t next_mark{};
  constexpr std::array<unsigned, 3> marks{5, 10, 30};
  const auto start = [&] {
    gp::RunSetup setup{.seed = seed++};
    if (maxed) { setup.permanent_ranks = gp::tuning::permanent_maximum_ranks; setup.unlocked_blueprints = 65535; }
    const auto before = allocations;
    game.start(setup);
    result.start_allocations += allocations - before;
  };
  start();
  while (next_mark < marks.size()) {
    const auto& view = game.snapshot();
    if (view.phase == gp::Phase::dead) { ++result.deaths; start(); continue; }
    if (view.phase == gp::Phase::choosing_upgrade) {
      const auto before = allocations;
      if (!game.choose_upgrade(static_cast<std::size_t>(result.picks % 3))) std::abort();
      result.draft_allocations += allocations - before;
      ++result.picks;
      continue;
    }
    // Identical scripted movement and seed sequence across fresh and maxed.
    const float t = static_cast<float>(view.stats.active_seconds);
    const gp::Control input{{std::sin(t * 0.45F) * 0.7F, std::cos(t * 0.31F) * 0.3F}, false};
    const auto previous = view.stats.active_seconds;
    const auto before_allocations = allocations;
    const auto began = std::chrono::steady_clock::now();
    game.advance(1.F / 60, input);
    const auto ended = std::chrono::steady_clock::now();
    result.cpu_nanoseconds += static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(ended - began).count());
    const auto delta = allocations - before_allocations;
    if (game.snapshot().phase == gp::Phase::choosing_upgrade) result.draft_allocations += delta;
    else result.active_allocations += delta;
    ++result.calls;
    const auto& after = game.snapshot();
    result.active += after.stats.active_seconds - previous;
    result.longest_run = std::max(result.longest_run, after.stats.active_seconds);
    result.entities = std::max(result.entities, after.entities.size());
    result.projectiles = std::max(result.projectiles, after.projectiles.size());
    result.beams = std::max(result.beams, after.beams.size());
    result.shots = std::max(result.shots, after.shots.size());
    std::size_t entity_limit = gp::tuning::biological_active_cap;
    for (const auto cap : gp::tuning::enemy_active_cap) entity_limit += static_cast<std::size_t>(cap);
    if (after.entities.size() > entity_limit || after.projectiles.size() > gp::tuning::projectile_active_cap ||
        after.beams.size() > gp::tuning::evolution_beams || after.shots.size() > 64) std::abort();
    if (result.active >= static_cast<double>(marks[next_mark] * 60)) print(name, marks[next_mark++], result);
  }
}
}
int main() {
  profile("fresh", false);
  profile("maxed", true);
}
