#include "resume_qa.h"
#include <cmath>
#include <iomanip>
#include <sstream>

namespace human_resources::client {
namespace {
std::string json_string(std::string_view value) {
  std::ostringstream out;
  out << '"';
  for (const char character : value) {
    const auto c = static_cast<unsigned char>(character);
    if (c == '"' || c == '\\')
      out << '\\' << c;
    else if (c < 32)
      out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<unsigned>(c) << std::dec;
    else
      out << c;
  }
  out << '"';
  return out.str();
}
} // namespace
bool ResumeQa::configure(std::string_view mode, std::string_view slot) {
  Mode candidate;
  if (mode == "prepare-choice")
    candidate = Mode::prepare_choice;
  else if (mode == "inspect")
    candidate = Mode::inspect;
  else if (mode == "resume")
    candidate = Mode::resume;
  else if (mode == "bank")
    candidate = Mode::bank;
  else
    return false;
  if (slot.empty() || slot.size() > 32)
    return false;
  for (const char c : slot)
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '_' || c == '-'))
      return false;
  mode_ = candidate;
  slot_ = slot;
  dispatched_ = held_ = false;
  return true;
}
product::Command ResumeQa::next_command(product::Screen screen, bool save_error, bool restored_run) {
  using product::Command;
  using product::Screen;
  if (!active() || save_error || dispatched_)
    return Command::none;
  Command command = Command::none;
  if (mode_ == Mode::prepare_choice && !restored_run && screen == Screen::title)
    command = Command::start;
  else if (restored_run && screen == Screen::pause) {
    if (mode_ == Mode::resume)
      command = Command::resume;
    else if (mode_ == Mode::bank)
      command = Command::finish;
  }
  if (command != Command::none)
    dispatched_ = true;
  return command;
}
float ResumeQa::movement_x(double seconds) const noexcept {
  return mode_ == Mode::prepare_choice ? .8F * static_cast<float>(std::sin(seconds * .7)) : 0.F;
}
bool ResumeQa::take_hold_marker(product::Screen screen) noexcept {
  using product::Screen;
  if (!active() || held_)
    return false;
  const bool hold = ((mode_ == Mode::prepare_choice || mode_ == Mode::resume) && screen == Screen::upgrade) ||
                    (mode_ == Mode::inspect && screen == Screen::pause) || (mode_ == Mode::bank && screen == Screen::results);
  if (hold)
    held_ = true;
  return hold;
}
std::vector<std::string> resume_log_lines(const gameplay::Snapshot& s, std::uint64_t run_id,
    std::uint8_t location, std::uint64_t stored_digest) {
  std::ostringstream out;
  out << std::setprecision(17) << "product-run-detail {\"run\":" << run_id << ",\"location\":" << +location
      << ",\"phase\":" << static_cast<unsigned>(s.phase) << ",\"active_seconds\":" << s.stats.active_seconds
      << ",\"level\":" << s.stats.level << ",\"score\":" << s.stats.score << ",\"captures\":" << s.stats.captures
      << ",\"xp\":" << s.xp << ",\"evolution_bits\":" << s.evolution_bits << ",\"state_digest\":\""
      << std::hex << std::setw(16) << std::setfill('0') << stored_digest << std::dec << "\",\"modules\":[";
  bool first = true;
  for (const auto rank : s.modules) {
    if (!first)
      out << ',';
    first = false;
    out << +rank;
  }
  out << "],\"research\":[";
  first = true;
  for (const auto rank : s.research) {
    if (!first)
      out << ',';
    first = false;
    out << rank;
  }
  out << "],\"choices\":[";
  if (s.phase == gameplay::Phase::choosing_upgrade) {
    first = true;
    for (const auto& choice : s.choices) {
      if (!first)
        out << ',';
      first = false;
      out << "{\"hardware\":" << (choice.hardware ? "true" : "false") << ",\"module\":" << static_cast<unsigned>(choice.module)
          << ",\"research\":" << static_cast<unsigned>(choice.research) << ",\"rarity\":" << static_cast<unsigned>(choice.rarity)
          << ",\"rank\":" << choice.next_rank << ",\"title\":" << json_string(choice.title)
          << ",\"description\":" << json_string(choice.description) << '}';
    }
  }
  out << "]}";
  return {out.str()};
}
} // namespace human_resources::client
