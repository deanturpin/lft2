#pragma once
#include "json.h"

// Open long position being tracked
struct position {
  double entry_price;
  double take_profit;   // Absolute price level
  double stop_loss;     // Absolute price level
  double trailing_stop; // Absolute price level (updated each bar)
};

// Exit evaluation: called every 5 minutes with current bar and position
// Returns true if position should be closed
constexpr bool is_exit(const position &pos, const bar &current) {
  if (!is_valid(current))
    return false;

  const auto price = current.close;
  return price >= pos.take_profit || price <= pos.stop_loss ||
         price <= pos.trailing_stop;
}

// Unit tests
namespace {
// Test: take profit hit
static_assert([] {
  auto pos = position{100.0, 110.0, 90.0, 85.0};
  auto b = bar{.close = 110.0,
               .high = 110.5,
               .low = 109.0,
               .open = 109.5,
               .vwap = 109.8,
               .volume = 1000,
               .num_trades = 50,
               .timestamp = "2025-01-01T10:00:00Z"};
  return is_exit(pos, b);
}());

// Test: stop loss hit
static_assert([] {
  auto pos = position{100.0, 110.0, 90.0, 85.0};
  auto b = bar{.close = 89.0,
               .high = 90.5,
               .low = 88.5,
               .open = 90.0,
               .vwap = 89.5,
               .volume = 1500,
               .num_trades = 75,
               .timestamp = "2025-01-01T10:00:00Z"};
  return is_exit(pos, b);
}());

// Test: trailing stop hit
static_assert([] {
  auto pos = position{100.0, 110.0, 90.0, 95.0};
  auto b = bar{.close = 94.0,
               .high = 95.5,
               .low = 93.5,
               .open = 95.0,
               .vwap = 94.5,
               .volume = 1200,
               .num_trades = 60,
               .timestamp = "2025-01-01T10:00:00Z"};
  return is_exit(pos, b);
}());

// Test: no exit conditions met
static_assert([] {
  auto pos = position{100.0, 110.0, 90.0, 95.0};
  auto b = bar{.close = 105.0,
               .high = 106.0,
               .low = 103.5,
               .open = 104.0,
               .vwap = 104.8,
               .volume = 800,
               .num_trades = 40,
               .timestamp = "2025-01-01T10:00:00Z"};
  return !is_exit(pos, b);
}());
} // namespace
