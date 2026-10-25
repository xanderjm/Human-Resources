#include <human_resources/accessibility/session.h>

#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace human_resources::accessibility {
namespace {

constexpr std::array<char, 16> hex_digits{
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

std::string hex(const std::string_view value) {
  std::string output;
  output.reserve(value.size() * 2U);
  for (const char character : value) {
    const auto byte = static_cast<unsigned char>(character);
    output.push_back(hex_digits[byte >> 4U]);
    output.push_back(hex_digits[byte & 0x0fU]);
  }
  return output;
}

void append_float(std::string& output, const float value) {
  std::array<char, 32> buffer{};
  const auto encoded = std::to_chars(
      buffer.data(), buffer.data() + buffer.size(), value, std::chars_format::fixed, 2);
  if (encoded.ec == std::errc{})
    output.append(buffer.data(), encoded.ptr);
}

} // namespace

std::string serialize(const std::span<const human_resources::ui::AccessibilityNode> nodes) {
  std::string output;
  output.reserve(nodes.size() * 96U);
  for (const human_resources::ui::AccessibilityNode& node : nodes) {
    if (!output.empty())
      output.push_back('\n');
    output.append(std::to_string(node.id));
    output.push_back('\t');
    output.append(std::to_string(static_cast<std::uint8_t>(node.role)));
    output.push_back('\t');
    output.push_back(node.focusable ? '1' : '0');
    output.push_back('\t');
    output.push_back(node.disabled ? '1' : '0');
    for (const float value :
        {node.frame.x, node.frame.y, node.frame.width, node.frame.height}) {
      output.push_back('\t');
      append_float(output, value);
    }
    output.push_back('\t');
    output.append(hex(node.label));
    output.push_back('\t');
    output.append(hex(node.value));
    output.push_back('\t');
    output.append(hex(node.stable_id));
    output.push_back('\t');
    output.push_back(node.checked ? '1' : '0');
    output.push_back('\t');
    output.push_back(node.selected ? '1' : '0');
  }
  return output;
}

} // namespace human_resources::accessibility
