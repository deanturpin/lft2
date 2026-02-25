#pragma once
#include "json.h"

// Open long position being tracked
struct position {
  double entry_price;
  double take_profit;   // Absolute price level
  double stop_loss;     // Absolute price level
  double trailing_stop; // Absolute price level (updated each bar)
};

// Exit reason classification
enum class exit_reason {
  none,          // Still holding
  take_profit,   // Hit 2% gain
  stop_loss,     // Hit 2% loss
  trailing_stop, // Fell 1% below peak
  risk_off,      // Market closing (15:30 ET)
  end_of_data    // Backtest data ran out
};

// Returns specific exit reason if any condition met
constexpr exit_reason check_exit(const position &pos, const bar &current) {
  if (!is_valid(current))
    return exit_reason::none;

  const auto price = current.close;

  // Check in priority order: take profit first (most desirable)
  if (price >= pos.take_profit)
    return exit_reason::take_profit;
  if (price <= pos.stop_loss)
    return exit_reason::stop_loss;
  if (price <= pos.trailing_stop)
    return exit_reason::trailing_stop;

  return exit_reason::none;
}

// Exit evaluation: called every 5 minutes with current bar and position
// Returns true if position should be closed
constexpr bool is_exit(const position &pos, const bar &current) {
  return check_exit(pos, current) != exit_reason::none;
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

// Test: trailing stop ratchets up with price — caller updates trailing_stop
// each bar to track the peak. Here price rose to 108, trailing_stop raised to
// 107 (1% below peak), then drops to 106.5 → below trailing_stop → exit.
static_assert([] {
  // After price rose to 108, caller set trailing_stop = 108 * 0.99 = 106.92
  auto pos = position{100.0, 115.0, 90.0, 106.92};
  auto b = bar{.close = 106.5, // below ratcheted trailing stop
               .high = 107.0,
               .low = 106.0,
               .open = 107.0,
               .vwap = 106.7,
               .volume = 900,
               .num_trades = 45,
               .timestamp = "2025-01-01T14:00:00Z"};
  return is_exit(pos, b);
}());

// Test: trailing stop does not trigger while price stays above it
static_assert([] {
  auto pos = position{100.0, 115.0, 90.0, 106.92};
  auto b = bar{.close = 108.0, // above trailing stop — no exit
               .high = 108.5,
               .low = 107.5,
               .open = 107.8,
               .vwap = 108.0,
               .volume = 900,
               .num_trades = 45,
               .timestamp = "2025-01-01T14:05:00Z"};
  return !is_exit(pos, b);
}());
} // namespace
