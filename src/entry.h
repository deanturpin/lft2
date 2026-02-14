#pragma once
#include "json.h"
#include <cmath>
#include <span>

// Constexpr square root using Newton-Raphson method
// Needed because std::sqrt isn't constexpr until C++26
constexpr double constexpr_sqrt(double x) {
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

// Volume surge with price dip strategy
// Detects capitulation patterns: high volume selling followed by reversal
// Based on backtest analysis showing volume surges during weakness precede gains
constexpr bool volume_surge_dip(std::span<const bar> history) {
  constexpr auto lookback = 20uz;
  if (history.size() < lookback)
    return false;

  // Validate bars
  for (auto i = 0uz; i < lookback; ++i)
    if (!is_valid(history[history.size() - 1 - i]))
      return false;

  // Calculate average volume over lookback period
  auto avg_vol = 0.0;
  for (auto i = 0uz; i < lookback; ++i)
    avg_vol += history[history.size() - lookback + i].volume;
  avg_vol /= lookback;

  if (avg_vol == 0.0)
    return false;

  auto current = history.back();
  auto vol_ratio = current.volume / avg_vol;
  auto price_change_pct = (current.close - current.open) / current.open * 100.0;

  // Volume surge (>2x average) with price dropping (>1% down)
  return vol_ratio > 2.0 && price_change_pct < -1.0;
}

// Mean reversion strategy
// Buys when price is more than 2 standard deviations below moving average
// Classic statistical arbitrage approach for oversold conditions
constexpr bool mean_reversion(std::span<const bar> history) {
  constexpr auto lookback = 20uz;
  if (history.size() < lookback)
    return false;

  // Validate bars
  for (auto i = 0uz; i < lookback; ++i)
    if (!is_valid(history[history.size() - 1 - i]))
      return false;

  // Calculate 20-period simple moving average
  auto sum = 0.0;
  for (auto i = 0uz; i < lookback; ++i)
    sum += history[history.size() - lookback + i].close;
  auto ma = sum / lookback;

  // Calculate standard deviation of prices
  auto variance = 0.0;
  for (auto i = 0uz; i < lookback; ++i) {
    auto diff = history[history.size() - lookback + i].close - ma;
    variance += diff * diff;
  }
  auto std_dev = constexpr_sqrt(variance / lookback);

  // Avoid division by zero for flat prices
  if (std_dev < 0.0001)
    return false;

  auto current_price = history.back().close;
  auto deviation = (current_price - ma) / std_dev;

  // Buy when price is >2 standard deviations below mean (oversold)
  return deviation < -2.0;
}

// Simple moving average crossover entry strategy
// Bullish signal when short-term MA crosses above long-term MA
// Traditional momentum indicator
template <size_t short_period = 10, size_t long_period = 20>
constexpr bool sma_crossover(std::span<const bar> history) {
  constexpr auto min_bars = long_period + 1;

  if (history.size() < min_bars)
    return false;

  // Validate all bars in the required window
  for (auto i = 0uz; i < min_bars; ++i)
    if (!is_valid(history[history.size() - 1 - i]))
      return false;

  // Calculate current SMAs
  auto short_sma = 0.0;
  auto long_sma = 0.0;

  for (auto i = 0uz; i < short_period; ++i)
    short_sma += history[history.size() - 1 - i].close;
  short_sma /= short_period;

  for (auto i = 0uz; i < long_period; ++i)
    long_sma += history[history.size() - 1 - i].close;
  long_sma /= long_period;

  // Calculate previous SMAs for crossover detection
  auto prev_short_sma = 0.0;
  auto prev_long_sma = 0.0;

  for (auto i = 1uz; i <= short_period; ++i)
    prev_short_sma += history[history.size() - 1 - i].close;
  prev_short_sma /= short_period;

  for (auto i = 1uz; i <= long_period; ++i)
    prev_long_sma += history[history.size() - 1 - i].close;
  prev_long_sma /= long_period;

  // Detect bullish crossover (short crosses above long)
  return prev_short_sma <= prev_long_sma && short_sma > long_sma;
}

// Master entry function combining multiple strategies
// Returns true if any strategy signals an entry
// Strategies are evaluated in order of backtest effectiveness
constexpr bool is_entry(std::span<const bar> history) {
  // Priority 1: Volume surge with dip (capitulation pattern from backtest)
  if (volume_surge_dip(history))
    return true;

  // Priority 2: Mean reversion (statistical oversold)
  if (mean_reversion(history))
    return true;

  // Priority 3: SMA crossover (momentum confirmation)
  if (sma_crossover(history))
    return true;

  return false;
}

// Unit tests
namespace {
// Test: insufficient history returns false for all strategies
static_assert([] {
  auto bars = std::array<bar, 5>{};
  for (auto i = 0uz; i < bars.size(); ++i)
    bars[i] = bar{.close = 100.0,
                  .high = 101.0,
                  .low = 99.0,
                  .open = 100.0,
                  .vwap = 100.0,
                  .volume = 1000,
                  .num_trades = 50,
                  .timestamp = "2025-01-01T10:00:00Z"};
  return !is_entry(bars);
}());

// Test: flat prices with normal volume returns false
static_assert([] {
  auto bars = std::array<bar, 25>{};
  for (auto i = 0uz; i < bars.size(); ++i)
    bars[i] = bar{.close = 100.0,
                  .high = 101.0,
                  .low = 99.0,
                  .open = 100.0,
                  .vwap = 100.0,
                  .volume = 1000,
                  .num_trades = 50,
                  .timestamp = "2025-01-01T10:00:00Z"};
  return !is_entry(bars);
}());

// Test: volume surge with dip triggers entry
static_assert([] {
  auto bars = std::array<bar, 25>{};
  // First 24 bars with normal volume
  for (auto i = 0uz; i < 24uz; ++i)
    bars[i] = bar{.close = 100.0,
                  .high = 101.0,
                  .low = 99.0,
                  .open = 100.0,
                  .vwap = 100.0,
                  .volume = 1000,
                  .num_trades = 50,
                  .timestamp = "2025-01-01T10:00:00Z"};
  // Last bar: volume surge with price drop
  bars[24] = bar{.close = 97.0,
                 .high = 100.0,
                 .low = 97.0,
                 .open = 99.0,
                 .vwap = 98.0,
                 .volume = 3000, // 3x average volume
                 .num_trades = 150,
                 .timestamp = "2025-01-01T11:00:00Z"};
  return is_entry(bars);
}());

// Test: mean reversion triggers when >2 std devs below MA
static_assert([] {
  auto bars = std::array<bar, 25>{};
  // First 24 bars around price 100
  for (auto i = 0uz; i < 24uz; ++i)
    bars[i] = bar{.close = 100.0,
                  .high = 101.0,
                  .low = 99.0,
                  .open = 100.0,
                  .vwap = 100.0,
                  .volume = 1000,
                  .num_trades = 50,
                  .timestamp = "2025-01-01T10:00:00Z"};
  // Last bar: sharp drop (>2 std devs below mean of 100)
  bars[24] = bar{.close = 94.0,
                 .high = 95.0,
                 .low = 94.0,
                 .open = 95.0,
                 .vwap = 94.5,
                 .volume = 1000,
                 .num_trades = 50,
                 .timestamp = "2025-01-01T11:00:00Z"};
  return is_entry(bars);
}());

// Test: individual strategy functions work correctly
static_assert([] {
  auto bars = std::array<bar, 25>{};
  // Setup bars with volume surge and dip
  for (auto i = 0uz; i < 24uz; ++i)
    bars[i] = bar{.close = 100.0,
                  .high = 101.0,
                  .low = 99.0,
                  .open = 100.0,
                  .vwap = 100.0,
                  .volume = 1000,
                  .num_trades = 50,
                  .timestamp = "2025-01-01T10:00:00Z"};
  bars[24] = bar{.close = 97.0,
                 .high = 100.0,
                 .low = 97.0,
                 .open = 99.0,
                 .vwap = 98.0,
                 .volume = 3000,
                 .num_trades = 150,
                 .timestamp = "2025-01-01T11:00:00Z"};
  return volume_surge_dip(bars);
}());
} // namespace
