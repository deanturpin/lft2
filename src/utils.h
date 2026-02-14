#pragma once

// Compile-time utility functions
// General helpers that don't fit in nstd (standard library replacements)

namespace utils {

// Constexpr near-equality test for floating point comparisons
// Useful for compile-time unit tests where exact equality is unreliable
// std::abs becomes constexpr in C++23, but we avoid it for C++20 compatibility
constexpr bool near(double a, double b, double epsilon = 0.01) {
  auto diff = a - b;
  return (diff < epsilon) && (diff > -epsilon);
}

// Unit tests
namespace {
// Test: exact equality
static_assert(near(4.0, 4.0));

// Test: within default epsilon (0.01)
static_assert(near(4.0, 4.005));

// Test: outside default epsilon
static_assert(!near(4.0, 4.02));

// Test: custom epsilon
static_assert(near(4.0, 4.05, 0.1));

// Test: negative values
static_assert(near(-4.0, -4.005));

// Test: zero
static_assert(near(0.0, 0.0));
} // namespace

} // namespace utils
