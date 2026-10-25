#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <span>
#include <string_view>

namespace human_resources::diagnostics {

enum class Severity : std::uint8_t {
  trace,
  debug,
  info,
  warning,
  error,
  critical,
};

enum class Subsystem : std::uint8_t {
  application,
  authority,
  protocol,
  replay,
  ui,
  renderer,
  audio,
  platform,
  asset,
  network,
};

enum class Privacy : std::uint8_t {
  public_data,
  sensitive,
};

enum class FieldKind : std::uint8_t {
  text,
  signed_integer,
  unsigned_integer,
  decimal,
  boolean,
};

struct Field final {
  std::string_view key;
  FieldKind kind{FieldKind::text};
  Privacy privacy{Privacy::public_data};
  std::string_view text;
  std::int64_t signed_integer{};
  std::uint64_t unsigned_integer{};
  double decimal{};
  bool boolean{};

  [[nodiscard]] static Field text_value(std::string_view key, std::string_view value,
      Privacy privacy = Privacy::public_data) noexcept;
  [[nodiscard]] static Field signed_value(std::string_view key, std::int64_t value,
      Privacy privacy = Privacy::public_data) noexcept;
  [[nodiscard]] static Field unsigned_value(std::string_view key, std::uint64_t value,
      Privacy privacy = Privacy::public_data) noexcept;
  [[nodiscard]] static Field decimal_value(std::string_view key, double value,
      Privacy privacy = Privacy::public_data) noexcept;
  [[nodiscard]] static Field boolean_value(std::string_view key, bool value,
      Privacy privacy = Privacy::public_data) noexcept;
};

struct Event final {
  Severity severity{Severity::info};
  Subsystem subsystem{Subsystem::application};
  std::uint32_t code{};
  std::string_view message;
  std::uint64_t session_id{};
  std::uint64_t monotonic_ns{};
  bool has_simulation_tick{};
  std::uint64_t simulation_tick{};
  std::span<const Field> fields;
};

inline constexpr std::size_t max_message_bytes = 192;
inline constexpr std::size_t max_field_count = 6;
inline constexpr std::size_t max_field_key_bytes = 24;
inline constexpr std::size_t max_field_value_bytes = 64;

struct StoredField final {
  std::array<char, max_field_key_bytes> key{};
  std::array<char, max_field_value_bytes> value{};
  FieldKind kind{FieldKind::text};
  Privacy privacy{Privacy::public_data};
};

struct Record final {
  Severity severity{Severity::info};
  Subsystem subsystem{Subsystem::application};
  std::uint32_t code{};
  std::array<char, max_message_bytes> message{};
  std::uint64_t session_id{};
  std::uint64_t monotonic_ns{};
  bool has_simulation_tick{};
  std::uint64_t simulation_tick{};
  std::array<StoredField, max_field_count> fields{};
  std::uint8_t field_count{};
  bool truncated{};
};

class Sink {
public:
  virtual ~Sink() = default;
  virtual void write(const Record& record) noexcept = 0;
};

struct DrainReport final {
  std::size_t delivered{};
  std::uint64_t dropped{};
};

class Logger final {
public:
  static constexpr std::size_t queue_capacity = 256;
  static constexpr std::size_t history_capacity = 256;
  static constexpr std::size_t sink_capacity = 4;

  Logger() = default;
  Logger(const Logger&) = delete;
  Logger& operator=(const Logger&) = delete;

  void set_minimum_severity(Severity severity) noexcept;
  [[nodiscard]] Severity minimum_severity() const noexcept;

  // Sink registration and removal are owner-thread operations. Every sink must
  // outlive the logger and any drain in progress.
  [[nodiscard]] bool add_sink(Sink& sink) noexcept;
  void remove_sink(Sink& sink) noexcept;

  // This is the only producer-side operation. It never allocates, never waits
  // for a lock, and never calls a sink. Contention or a full queue increments
  // the dropped counter instead of stalling a render, audio, or worker thread.
  [[nodiscard]] bool try_record(const Event& event) noexcept;

  // Drain from a non-critical owner thread. Sinks are called after the queue
  // lock is released, so a slow platform/file sink cannot block producers.
  [[nodiscard]] DrainReport drain() noexcept;

  [[nodiscard]] std::size_t copy_history(std::span<Record> output) const noexcept;
  [[nodiscard]] std::uint64_t total_dropped() const noexcept;

private:
  [[nodiscard]] static Record store(const Event& event) noexcept;

  mutable std::mutex mutex_;
  std::array<Record, queue_capacity> pending_{};
  std::size_t pending_head_{};
  std::size_t pending_count_{};
  std::array<Record, history_capacity> history_{};
  std::size_t history_head_{};
  std::size_t history_count_{};
  std::array<Sink*, sink_capacity> sinks_{};
  std::size_t sink_count_{};
  std::atomic<Severity> minimum_{Severity::info};
  std::atomic<std::uint64_t> dropped_{0};
  std::atomic<std::uint64_t> total_dropped_{0};
};

[[nodiscard]] std::string_view severity_name(Severity severity) noexcept;
[[nodiscard]] std::string_view subsystem_name(Subsystem subsystem) noexcept;

} // namespace human_resources::diagnostics
