#pragma once
#include <human_resources/progression/progression.h>
namespace human_resources::progression::detail {
inline constexpr std::uint64_t maximum_value = 9000000000000000ULL;
inline constexpr std::size_t maximum_payload = 64 * 1024;
[[nodiscard]] bool valid_receipt(const RunReceipt& receipt) noexcept;
[[nodiscard]] bool valid_account(const Account& account) noexcept;
[[nodiscard]] bool follows(const RunReceipt& newer, const RunReceipt& older) noexcept;
[[nodiscard]] Account fresh_account();
[[nodiscard]] std::uint32_t unlocked(const Counters& counters) noexcept;
[[nodiscard]] std::uint64_t counter_value(const Counters& counters, std::string_view counter) noexcept;
[[nodiscard]] Account settled(Account account, const RunReceipt& receipt, bool recovered);
[[nodiscard]] Account settled_recorded(Account account, const RunReceipt& receipt,
    std::uint64_t recorded_credits, bool recovered);
enum class AccountDecodeIssue : std::uint8_t { none, malformed, unsupported };
[[nodiscard]] AccountDecodeIssue decode_versioned_account(std::span<const std::byte> payload,
    std::uint32_t envelope_schema, Account& output) noexcept;
[[nodiscard]] bool valid_continuation(const Account& account);
}
