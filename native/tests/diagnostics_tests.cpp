#include "test_support.h"

#include <human_resources/diagnostics/diagnostics.h>

#include <array>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace {

using test_support::require;
using namespace human_resources::diagnostics;

class CaptureSink final : public Sink {
public:
  void write(const Record& record) noexcept override {
    records.push_back(record);
  }
  std::vector<Record> records;
};

void structured_records_are_copied_and_redacted() {
  Logger logger;
  CaptureSink sink;
  require(logger.add_sink(sink), "capture sink is admitted");
  require(logger.add_sink(sink), "adding the same sink is idempotent");
  const std::array fields{
      Field::unsigned_value("tick", 42U),
      Field::text_value("account", "secret-account", Privacy::sensitive),
      Field::boolean_value("replayed", true),
  };
  require(logger.try_record({.severity = Severity::warning,
              .subsystem = Subsystem::authority,
              .code = 7001U,
              .message = "intent rejected",
              .session_id = 9U,
              .has_simulation_tick = true,
              .simulation_tick = 42U,
              .fields = fields}),
      "structured event is accepted without calling a sink");
  require(sink.records.empty(), "producer never invokes sink synchronously");
  const auto report = logger.drain();
  require(report.delivered == 1U && report.dropped == 0U, "drain reports exact counts");
  require(sink.records.size() == 1U, "drain delivers one record");
  const Record& record = sink.records.front();
  require(record.code == 7001U && record.session_id == 9U && record.simulation_tick == 42U,
      "correlation fields survive storage");
  require(std::string(record.fields[0].value.data()) == "42", "integer field is formatted");
  require(record.fields[0].kind == FieldKind::unsigned_integer, "typed field metadata survives bounded storage");
  require(std::string(record.fields[1].value.data()) == "<redacted>", "sensitive field never reaches a sink");
  require(std::string(record.fields[2].value.data()) == "true", "boolean field is formatted");
}

void severity_filter_and_bounded_queue_fail_safely() {
  Logger logger;
  logger.set_minimum_severity(Severity::warning);
  require(!logger.try_record({.severity = Severity::info, .message = "filtered"}), "filtered event is not queued");
  require(logger.total_dropped() == 0U, "severity filtering is not counted as loss");
  logger.set_minimum_severity(Severity::trace);
  for (std::size_t index = 0; index < Logger::queue_capacity; ++index) {
    require(logger.try_record(
                {.severity = Severity::debug, .code = static_cast<std::uint32_t>(index), .message = "queued"}),
        "bounded queue accepts capacity records");
  }
  for (std::size_t index = 0; index < 10U; ++index) {
    require(!logger.try_record({.severity = Severity::debug, .message = "overflow"}),
        "full queue drops instead of blocking");
  }
  const auto report = logger.drain();
  require(report.delivered == Logger::queue_capacity, "drain preserves every admitted event");
  require(report.dropped == 10U && logger.total_dropped() == 10U, "drop accounting is explicit and cumulative");

  std::array<Record, 4> history{};
  require(logger.copy_history(history) == history.size(), "bounded breadcrumb tail is readable");
  require(std::string(history.back().message.data()) == "overflow",
      "history retains recent context even when delivery queue is full");
}

void truncation_and_sink_capacity_are_explicit() {
  Logger logger;
  std::array<CaptureSink, Logger::sink_capacity + 1U> sinks;
  for (std::size_t index = 0; index < Logger::sink_capacity; ++index) {
    require(logger.add_sink(sinks[index]), "sink slots admit configured capacity");
  }
  require(!logger.add_sink(sinks.back()), "sink overflow fails closed");
  const std::string long_message(max_message_bytes + 40U, 'x');
  require(logger.try_record({.severity = Severity::error, .message = long_message}),
      "long message is admitted with bounded copy");
  (void)logger.drain();
  require(sinks[0].records.front().truncated, "bounded truncation is visible to sinks");
  logger.remove_sink(sinks[0]);
  require(logger.add_sink(sinks.back()), "removed sink releases its bounded slot");
}

} // namespace

int main() {
  structured_records_are_copied_and_redacted();
  severity_filter_and_bounded_queue_fail_safely();
  truncation_and_sink_capacity_are_explicit();
  std::cout << "diagnostics contracts passed\n";
  return 0;
}
