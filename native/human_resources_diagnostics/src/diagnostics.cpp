#include <human_resources/diagnostics/diagnostics.h>

#include <algorithm>
#include <charconv>
#include <cstdio>
#include <system_error>

namespace human_resources::diagnostics {
namespace {

template <std::size_t Size>
bool copy_text(std::array<char, Size>& destination, const std::string_view source) noexcept {
  static_assert(Size > 0);
  const std::size_t copied = std::min(source.size(), Size - 1U);
  if (copied > 0U) {
    std::copy_n(source.begin(), copied, destination.begin());
  }
  destination[copied] = '\0';
  return copied != source.size();
}

template <typename Integer>
bool copy_integer(std::array<char, max_field_value_bytes>& destination, const Integer value) noexcept {
  const auto result = std::to_chars(destination.data(), destination.data() + destination.size() - 1U, value);
  if (result.ec != std::errc{}) {
    return copy_text(destination, "<format-error>");
  }
  *result.ptr = '\0';
  return false;
}

StoredField store_field(const Field& field, bool& truncated) noexcept {
  StoredField stored;
  stored.kind = field.kind;
  stored.privacy = field.privacy;
  truncated = copy_text(stored.key, field.key) || truncated;
  if (field.privacy == Privacy::sensitive) {
    truncated = copy_text(stored.value, "<redacted>") || truncated;
    return stored;
  }
  switch (field.kind) {
  case FieldKind::text:
    truncated = copy_text(stored.value, field.text) || truncated;
    break;
  case FieldKind::signed_integer:
    truncated = copy_integer(stored.value, field.signed_integer) || truncated;
    break;
  case FieldKind::unsigned_integer:
    truncated = copy_integer(stored.value, field.unsigned_integer) || truncated;
    break;
  case FieldKind::decimal: {
    const int written = std::snprintf(stored.value.data(), stored.value.size(), "%.6g", field.decimal);
    if (written < 0 || static_cast<std::size_t>(written) >= stored.value.size()) {
      truncated = true;
    }
    break;
  }
  case FieldKind::boolean:
    truncated = copy_text(stored.value, field.boolean ? "true" : "false") || truncated;
    break;
  }
  return stored;
}

} // namespace

Field Field::text_value(const std::string_view key, const std::string_view value, const Privacy privacy) noexcept {
  return {.key = key, .kind = FieldKind::text, .privacy = privacy, .text = value};
}

Field Field::signed_value(const std::string_view key, const std::int64_t value, const Privacy privacy) noexcept {
  Field field;
  field.key = key;
  field.kind = FieldKind::signed_integer;
  field.privacy = privacy;
  field.signed_integer = value;
  return field;
}

Field Field::unsigned_value(const std::string_view key, const std::uint64_t value, const Privacy privacy) noexcept {
  Field field;
  field.key = key;
  field.kind = FieldKind::unsigned_integer;
  field.privacy = privacy;
  field.unsigned_integer = value;
  return field;
}

Field Field::decimal_value(const std::string_view key, const double value, const Privacy privacy) noexcept {
  Field field;
  field.key = key;
  field.kind = FieldKind::decimal;
  field.privacy = privacy;
  field.decimal = value;
  return field;
}

Field Field::boolean_value(const std::string_view key, const bool value, const Privacy privacy) noexcept {
  Field field;
  field.key = key;
  field.kind = FieldKind::boolean;
  field.privacy = privacy;
  field.boolean = value;
  return field;
}

void Logger::set_minimum_severity(const Severity severity) noexcept {
  minimum_.store(severity, std::memory_order_relaxed);
}

Severity Logger::minimum_severity() const noexcept {
  return minimum_.load(std::memory_order_relaxed);
}

bool Logger::add_sink(Sink& sink) noexcept {
  std::lock_guard lock(mutex_);
  for (std::size_t index = 0; index < sink_count_; ++index) {
    if (sinks_[index] == &sink) {
      return true;
    }
  }
  if (sink_count_ == sink_capacity) {
    return false;
  }
  sinks_[sink_count_++] = &sink;
  return true;
}

void Logger::remove_sink(Sink& sink) noexcept {
  std::lock_guard lock(mutex_);
  for (std::size_t index = 0; index < sink_count_; ++index) {
    if (sinks_[index] != &sink) {
      continue;
    }
    for (std::size_t cursor = index + 1U; cursor < sink_count_; ++cursor) {
      sinks_[cursor - 1U] = sinks_[cursor];
    }
    sinks_[--sink_count_] = nullptr;
    return;
  }
}

Record Logger::store(const Event& event) noexcept {
  Record record;
  record.severity = event.severity;
  record.subsystem = event.subsystem;
  record.code = event.code;
  record.session_id = event.session_id;
  record.monotonic_ns = event.monotonic_ns;
  record.has_simulation_tick = event.has_simulation_tick;
  record.simulation_tick = event.simulation_tick;
  record.truncated = copy_text(record.message, event.message);
  const std::size_t count = std::min(event.fields.size(), max_field_count);
  record.field_count = static_cast<std::uint8_t>(count);
  record.truncated = record.truncated || event.fields.size() > max_field_count;
  for (std::size_t index = 0; index < count; ++index) {
    record.fields[index] = store_field(event.fields[index], record.truncated);
  }
  return record;
}

bool Logger::try_record(const Event& event) noexcept {
  if (event.severity < minimum_.load(std::memory_order_relaxed)) {
    return false;
  }
  if (!mutex_.try_lock()) {
    dropped_.fetch_add(1U, std::memory_order_relaxed);
    total_dropped_.fetch_add(1U, std::memory_order_relaxed);
    return false;
  }

  const Record record = store(event);
  const std::size_t history_index = (history_head_ + history_count_) % history_capacity;
  if (history_count_ < history_capacity) {
    history_[history_index] = record;
    ++history_count_;
  } else {
    history_[history_head_] = record;
    history_head_ = (history_head_ + 1U) % history_capacity;
  }

  if (pending_count_ == queue_capacity) {
    mutex_.unlock();
    dropped_.fetch_add(1U, std::memory_order_relaxed);
    total_dropped_.fetch_add(1U, std::memory_order_relaxed);
    return false;
  }
  pending_[(pending_head_ + pending_count_) % queue_capacity] = record;
  ++pending_count_;
  mutex_.unlock();
  return true;
}

DrainReport Logger::drain() noexcept {
  std::array<Record, queue_capacity> records;
  std::array<Sink*, sink_capacity> sinks;
  std::size_t record_count = 0;
  std::size_t sink_count = 0;
  {
    std::lock_guard lock(mutex_);
    record_count = pending_count_;
    for (std::size_t index = 0; index < record_count; ++index) {
      records[index] = pending_[(pending_head_ + index) % queue_capacity];
    }
    pending_head_ = 0;
    pending_count_ = 0;
    sink_count = sink_count_;
    std::copy_n(sinks_.begin(), sink_count, sinks.begin());
  }

  for (std::size_t record_index = 0; record_index < record_count; ++record_index) {
    for (std::size_t sink_index = 0; sink_index < sink_count; ++sink_index) {
      sinks[sink_index]->write(records[record_index]);
    }
  }
  return {.delivered = record_count, .dropped = dropped_.exchange(0U, std::memory_order_relaxed)};
}

std::size_t Logger::copy_history(const std::span<Record> output) const noexcept {
  std::lock_guard lock(mutex_);
  const std::size_t copied = std::min(output.size(), history_count_);
  const std::size_t skipped = history_count_ - copied;
  for (std::size_t index = 0; index < copied; ++index) {
    output[index] = history_[(history_head_ + skipped + index) % history_capacity];
  }
  return copied;
}

std::uint64_t Logger::total_dropped() const noexcept {
  return total_dropped_.load(std::memory_order_relaxed);
}

std::string_view severity_name(const Severity severity) noexcept {
  switch (severity) {
  case Severity::trace:
    return "trace";
  case Severity::debug:
    return "debug";
  case Severity::info:
    return "info";
  case Severity::warning:
    return "warning";
  case Severity::error:
    return "error";
  case Severity::critical:
    return "critical";
  }
  return "unknown";
}

std::string_view subsystem_name(const Subsystem subsystem) noexcept {
  switch (subsystem) {
  case Subsystem::application:
    return "application";
  case Subsystem::authority:
    return "authority";
  case Subsystem::protocol:
    return "protocol";
  case Subsystem::replay:
    return "replay";
  case Subsystem::ui:
    return "ui";
  case Subsystem::renderer:
    return "renderer";
  case Subsystem::audio:
    return "audio";
  case Subsystem::platform:
    return "platform";
  case Subsystem::asset:
    return "asset";
  case Subsystem::network:
    return "network";
  }
  return "unknown";
}

} // namespace human_resources::diagnostics
