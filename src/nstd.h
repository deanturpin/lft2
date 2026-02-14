#pragma once

// Non-standard library utilities (nstd)
// Constexpr implementations of standard library functions that aren't yet
// constexpr in C++20/23/26. These can be easily replaced when the standard
// evolves.
//
// Naming convention: nstd::function_name() mirrors std::function_name()
// When std becomes constexpr, find/replace nstd:: with std::

namespace nstd {

// Constexpr square root using Newton-Raphson method
// std::sqrt becomes constexpr in C++26
// TODO: Replace with std::sqrt when C++26 is available
constexpr double sqrt(double x) {
  if (x < 0.0)
    return 0.0;
  if (x == 0.0)
    return 0.0;

  auto guess = x;
  auto prev_guess = 0.0;
  constexpr auto epsilon = 0.00001;
  constexpr auto max_iterations = 100;

  for (auto i = 0; i < max_iterations; ++i) {
    prev_guess = guess;
    guess = (guess + x / guess) / 2.0;
    if (guess - prev_guess < epsilon && prev_guess - guess < epsilon)
      break;
  }

  return guess;
}

// Unit tests
namespace {
// Test: sqrt of perfect square
static_assert([] {
  auto result = sqrt(16.0);
  return result > 3.99 && result < 4.01; // Approximately 4.0
}());

// Test: sqrt of non-perfect square
static_assert([] {
  auto result = sqrt(2.0);
  return result > 1.41 && result < 1.42; // Approximately 1.414
}());

// Test: sqrt of zero
static_assert(sqrt(0.0) == 0.0);

// Test: sqrt of negative returns zero
static_assert(sqrt(-1.0) == 0.0);
} // namespace

} // namespace nstd
