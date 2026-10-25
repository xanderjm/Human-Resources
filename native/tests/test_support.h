#pragma once

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string_view>

namespace test_support {

inline void require(const bool condition, const std::string_view message) {
  if (!condition) {
    std::cerr << "FAILED: " << message << '\n';
    std::exit(1);
  }
}

inline void require_near(const float actual, const float expected, const float tolerance,
    const std::string_view message) {
  if (std::fabs(actual - expected) > tolerance) {
    std::cerr << "FAILED: " << message << " (actual=" << actual << ", expected=" << expected << ")\n";
    std::exit(1);
  }
}

} // namespace test_support
