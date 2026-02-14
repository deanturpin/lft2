#pragma once
#include "utils.h"

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
// When upgrading to C++26: grep -r "nstd::sqrt" and replace with std::sqrt
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
static_assert(utils::near(sqrt(16.0), 4.0));

// Test: sqrt of non-perfect square
static_assert(utils::near(sqrt(2.0), 1.414, 0.001));

// Test: sqrt of zero
static_assert(sqrt(0.0) == 0.0);

// Test: sqrt of negative returns zero
static_assert(sqrt(-1.0) == 0.0);
} // namespace

} // namespace nstd
