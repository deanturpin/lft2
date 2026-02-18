#pragma once
#include "json.h"
#include "nstd.h"
#include <span>
#include <string_view>

// Volume surge with price dip strategy
// Detects capitulation patterns: high volume selling followed by reversal
// Based on backtest analysis showing volume surges during weakness precede
// gains
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

// volume_surge_dip: insufficient history returns false
static_assert([] {
  auto bars = std::array<bar, 10>{};
  for (auto &b : bars)
    b = bar{.close = 97.0,
            .high = 100.0,
            .low = 97.0,
            .open = 99.0,
            .vwap = 98.0,
            .volume = 3000,
            .num_trades = 150,
            .timestamp = "2026-01-01T10:00:00Z"};
  return !volume_surge_dip(bars);
}());

// volume_surge_dip: high volume but price UP does not trigger
static_assert([] {
  auto bars = std::array<bar, 25>{};
  for (auto i = 0uz; i < 24uz; ++i)
    bars[i] = bar{.close = 100.0,
                  .high = 101.0,
                  .low = 99.0,
                  .open = 100.0,
                  .vwap = 100.0,
                  .volume = 1000,
                  .num_trades = 50,
                  .timestamp = "2026-01-01T10:00:00Z"};
  // 3x volume but price closes UP — should NOT trigger
  bars[24] = bar{.close = 102.0,
                 .high = 103.0,
                 .low = 99.0,
                 .open = 99.0,
                 .vwap = 101.0,
                 .volume = 3000,
                 .num_trades = 150,
                 .timestamp = "2026-01-01T11:00:00Z"};
  return !volume_surge_dip(bars);
}());

// volume_surge_dip: 3x volume with >1% price drop triggers
static_assert([] {
  auto bars = std::array<bar, 25>{};
  for (auto i = 0uz; i < 24uz; ++i)
    bars[i] = bar{.close = 100.0,
                  .high = 101.0,
                  .low = 99.0,
                  .open = 100.0,
                  .vwap = 100.0,
                  .volume = 1000,
                  .num_trades = 50,
                  .timestamp = "2026-01-01T10:00:00Z"};
  bars[24] = bar{.close = 97.0,
                 .high = 100.0,
                 .low = 97.0,
                 .open = 99.0,
                 .vwap = 98.0,
                 .volume = 3000,
                 .num_trades = 150,
                 .timestamp = "2026-01-01T11:00:00Z"};
  return volume_surge_dip(bars);
}());

// volume_surge_dip: price drops but volume is normal does not trigger
static_assert([] {
  auto bars = std::array<bar, 25>{};
  for (auto i = 0uz; i < 24uz; ++i)
    bars[i] = bar{.close = 100.0,
                  .high = 101.0,
                  .low = 99.0,
                  .open = 100.0,
                  .vwap = 100.0,
                  .volume = 1000,
                  .num_trades = 50,
                  .timestamp = "2026-01-01T10:00:00Z"};
  // Normal volume (1.5x) with big price drop — below 2x threshold
  bars[24] = bar{.close = 97.0,
                 .high = 100.0,
                 .low = 97.0,
                 .open = 99.0,
                 .vwap = 98.0,
                 .volume = 1500,
                 .num_trades = 75,
                 .timestamp = "2026-01-01T11:00:00Z"};
  return !volume_surge_dip(bars);
}());

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
  auto std_dev = nstd::sqrt(variance / lookback);

  // Avoid division by zero for flat prices
  if (std_dev < 0.0001)
    return false;

  auto current_price = history.back().close;
  auto deviation = (current_price - ma) / std_dev;

  // Buy when price is >2 standard deviations below mean (oversold)
  return deviation < -2.0;
}

// mean_reversion: insufficient history returns false
static_assert([] {
  auto bars = std::array<bar, 10>{};
  for (auto &b : bars)
    b = bar{.close = 94.0,
            .high = 95.0,
            .low = 94.0,
            .open = 95.0,
            .vwap = 94.5,
            .volume = 1000,
            .num_trades = 50,
            .timestamp = "2026-01-01T10:00:00Z"};
  return !mean_reversion(bars);
}());

// mean_reversion: flat prices (zero std dev) returns false
static_assert([] {
  auto bars = std::array<bar, 20>{};
  for (auto &b : bars)
    b = bar{.close = 100.0,
            .high = 101.0,
            .low = 99.0,
            .open = 100.0,
            .vwap = 100.0,
            .volume = 1000,
            .num_trades = 50,
            .timestamp = "2026-01-01T10:00:00Z"};
  return !mean_reversion(bars);
}());

// mean_reversion: price >2 std devs below MA triggers
static_assert([] {
  auto bars = std::array<bar, 25>{};
  for (auto i = 0uz; i < 24uz; ++i)
    bars[i] = bar{.close = 100.0,
                  .high = 101.0,
                  .low = 99.0,
                  .open = 100.0,
                  .vwap = 100.0,
                  .volume = 1000,
                  .num_trades = 50,
                  .timestamp = "2026-01-01T10:00:00Z"};
  // Sharp drop well below the mean — triggers mean reversion
  bars[24] = bar{.close = 94.0,
                 .high = 95.0,
                 .low = 94.0,
                 .open = 95.0,
                 .vwap = 94.5,
                 .volume = 1000,
                 .num_trades = 50,
                 .timestamp = "2026-01-01T11:00:00Z"};
  return mean_reversion(bars);
}());

// mean_reversion: price varying ±2 around mean — well within 2 std devs
// With 10 bars alternating 98/102, std dev ≈ 2; a bar at 97 is only ~1.5σ below
static_assert([] {
  auto bars = std::array<bar, 20>{};
  for (auto i = 0uz; i < 19uz; ++i) {
    auto c = (i % 2 == 0) ? 98.0 : 102.0;
    bars[i] = bar{.close = c,
                  .high = c + 1.0,
                  .low = c - 1.0,
                  .open = c,
                  .vwap = c,
                  .volume = 1000,
                  .num_trades = 50,
                  .timestamp = "2026-01-01T10:00:00Z"};
  }
  // Last bar at 97 — mean≈100, std dev≈2, deviation≈-1.5σ, should NOT trigger
  bars[19] = bar{.close = 97.0,
                 .high = 98.0,
                 .low = 96.0,
                 .open = 97.5,
                 .vwap = 97.0,
                 .volume = 1000,
                 .num_trades = 50,
                 .timestamp = "2026-01-01T11:00:00Z"};
  return !mean_reversion(bars);
}());

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

// sma_crossover: insufficient history returns false
static_assert([] {
  auto bars = std::array<bar, 15>{};
  for (auto &b : bars)
    b = bar{.close = 100.0,
            .high = 101.0,
            .low = 99.0,
            .open = 100.0,
            .vwap = 100.0,
            .volume = 1000,
            .num_trades = 50,
            .timestamp = "2026-01-01T10:00:00Z"};
  return !sma_crossover(bars);
}());

// sma_crossover: short MA already above long MA (no crossover) does not trigger
static_assert([] {
  // 22 bars: first 11 at 95 (depresses long MA), last 11 rising to 105
  // Short MA > Long MA for both current and previous period — no crossover
  auto bars = std::array<bar, 22>{};
  for (auto i = 0uz; i < 11uz; ++i)
    bars[i] = bar{.close = 95.0,
                  .high = 96.0,
                  .low = 94.0,
                  .open = 95.0,
                  .vwap = 95.0,
                  .volume = 1000,
                  .num_trades = 50,
                  .timestamp = "2026-01-01T10:00:00Z"};
  for (auto i = 11uz; i < 22uz; ++i)
    bars[i] = bar{.close = 105.0,
                  .high = 106.0,
                  .low = 104.0,
                  .open = 105.0,
                  .vwap = 105.0,
                  .volume = 1000,
                  .num_trades = 50,
                  .timestamp = "2026-01-01T11:00:00Z"};
  return !sma_crossover(bars);
}());

// sma_crossover: bullish crossover triggers when short MA just crosses above
// long Setup: 21 bars at 90, then 1 bar at 200. Current:
// short(10)=10×90+1×200/10... no — indices from end:
//   short_sma (i=0..9, last 10): bars[21]=200, bars[12..20]=90 →
//   (200+9×90)/10=101 long_sma  (i=0..19, last 20): bars[21]=200,
//   bars[2..20]=90  → (200+19×90)/20=95.5 short > long ✓
// Previous: short(i=1..10): all bars[11..20]=90 → 90
//   long(i=1..20): bars[1..20]=90 → 90
//   prev_short(90) <= prev_long(90) ✓ — crossover!
static_assert([] {
  auto bars = std::array<bar, 22>{};
  for (auto i = 0uz; i < 21uz; ++i)
    bars[i] = bar{.close = 90.0,
                  .high = 91.0,
                  .low = 89.0,
                  .open = 90.0,
                  .vwap = 90.0,
                  .volume = 1000,
                  .num_trades = 50,
                  .timestamp = "2026-01-01T10:00:00Z"};
  bars[21] = bar{.close = 200.0,
                 .high = 201.0,
                 .low = 199.0,
                 .open = 200.0,
                 .vwap = 200.0,
                 .volume = 1000,
                 .num_trades = 50,
                 .timestamp = "2026-01-01T11:00:00Z"};
  return sma_crossover(bars);
}());

// sma_crossover: flat prices (short == long MA) does not trigger
static_assert([] {
  auto bars = std::array<bar, 22>{};
  for (auto &b : bars)
    b = bar{.close = 100.0,
            .high = 101.0,
            .low = 99.0,
            .open = 100.0,
            .vwap = 100.0,
            .volume = 1000,
            .num_trades = 50,
            .timestamp = "2026-01-01T10:00:00Z"};
  return !sma_crossover(bars);
}());

// Price dip strategy
// Buys when the bar closes >1% below its open — a single-bar momentum reversal
// signal. Simple but catches intraday capitulation moves.
constexpr bool price_dip(std::span<const bar> history) {
  if (history.size() < 2)
    return false;

  auto current = history.back();
  if (!is_valid(current))
    return false;

  auto price_change_pct = (current.close - current.open) / current.open * 100.0;
  return price_change_pct < -1.0;
}

// price_dip: exact threshold — 1% drop triggers, 0.99% does not
static_assert([] {
  auto bars = std::array<bar, 2>{};
  bars[0] = bar{.close = 100.0,
                .high = 101.0,
                .low = 99.0,
                .open = 100.0,
                .vwap = 100.0,
                .volume = 1000,
                .num_trades = 10,
                .timestamp = "2026-01-01T10:00:00Z"};
  // 0.99% drop — should NOT trigger
  bars[1] = bar{.close = 99.01,
                .high = 100.0,
                .low = 98.5,
                .open = 100.0,
                .vwap = 99.5,
                .volume = 1000,
                .num_trades = 10,
                .timestamp = "2026-01-01T10:05:00Z"};
  return !price_dip(bars);
}());

// price_dip: exactly 1.01% drop triggers
static_assert([] {
  auto bars = std::array<bar, 2>{};
  bars[0] = bar{.close = 100.0,
                .high = 101.0,
                .low = 99.0,
                .open = 100.0,
                .vwap = 100.0,
                .volume = 1000,
                .num_trades = 10,
                .timestamp = "2026-01-01T10:00:00Z"};
  bars[1] = bar{.close = 98.98,
                .high = 100.0,
                .low = 98.5,
                .open = 100.0,
                .vwap = 99.0,
                .volume = 1000,
                .num_trades = 10,
                .timestamp = "2026-01-01T10:05:00Z"};
  return price_dip(bars);
}());

// price_dip: up bar does not trigger
static_assert([] {
  auto bars = std::array<bar, 2>{};
  bars[0] = bar{.close = 100.0,
                .high = 101.0,
                .low = 99.0,
                .open = 100.0,
                .vwap = 100.0,
                .volume = 1000,
                .num_trades = 10,
                .timestamp = "2026-01-01T10:00:00Z"};
  bars[1] = bar{.close = 101.5,
                .high = 102.0,
                .low = 100.0,
                .open = 100.0,
                .vwap = 101.0,
                .volume = 1000,
                .num_trades = 10,
                .timestamp = "2026-01-01T10:05:00Z"};
  return !price_dip(bars);
}());

// Volatility breakout strategy
// Buys when recent volatility expands to >1.5x historical average and the bar
// closes up — signals a breakout from compression rather than a breakdown.
constexpr bool volatility_breakout(std::span<const bar> history) {
  constexpr auto lookback = 20uz;
  constexpr auto recent_window = 5uz;

  if (history.size() < lookback + recent_window)
    return false;

  // Validate recent bars
  for (auto i = 0uz; i < recent_window; ++i)
    if (!is_valid(history[history.size() - 1 - i]))
      return false;

  // Recent volatility: average bar range over last 5 bars
  auto recent_vol = 0.0;
  for (auto i = 0uz; i < recent_window; ++i) {
    const auto &b = history[history.size() - 1 - i];
    recent_vol += (b.high - b.low) / b.close;
  }
  recent_vol /= recent_window;

  // Historical volatility: average bar range over the preceding lookback bars
  auto hist_vol = 0.0;
  for (auto i = recent_window; i < recent_window + lookback; ++i) {
    const auto &b = history[history.size() - 1 - i];
    if (!is_valid(b))
      return false;
    hist_vol += (b.high - b.low) / b.close;
  }
  hist_vol /= lookback;

  if (hist_vol < 0.0001)
    return false;

  // Breakout: volatility expands AND current bar closes up
  auto current = history.back();
  auto price_change_pct = (current.close - current.open) / current.open * 100.0;
  return recent_vol > hist_vol * 1.5 && price_change_pct > 0.0;
}

// volatility_breakout: insufficient history returns false
static_assert([] {
  auto bars = std::array<bar, 10>{};
  for (auto &b : bars)
    b = bar{.close = 100.0,
            .high = 101.0,
            .low = 99.0,
            .open = 100.0,
            .vwap = 100.0,
            .volume = 1000,
            .num_trades = 10,
            .timestamp = "2026-01-01T10:00:00Z"};
  return !volatility_breakout(bars);
}());

// volatility_breakout: flat historical bars with no expansion does not trigger
static_assert([] {
  auto bars = std::array<bar, 30>{};
  for (auto &b : bars)
    b = bar{.close = 100.0,
            .high = 100.1,
            .low = 99.9,
            .open = 100.0,
            .vwap = 100.0,
            .volume = 1000,
            .num_trades = 10,
            .timestamp = "2026-01-01T10:00:00Z"};
  return !volatility_breakout(bars);
}());

// volatility_breakout: range expansion with up close triggers
static_assert([] {
  auto bars = std::array<bar, 30>{};
  for (auto i = 0uz; i < 25uz; ++i)
    bars[i] = bar{.close = 100.0,
                  .high = 100.2,
                  .low = 99.8,
                  .open = 100.0,
                  .vwap = 100.0,
                  .volume = 1000,
                  .num_trades = 50,
                  .timestamp = "2026-01-01T10:00:00Z"};
  for (auto i = 25uz; i < 30uz; ++i)
    bars[i] = bar{.close = 102.0,
                  .high = 104.0,
                  .low = 98.0,
                  .open = 100.0,
                  .vwap = 101.0,
                  .volume = 1000,
                  .num_trades = 50,
                  .timestamp = "2026-01-01T11:00:00Z"};
  return volatility_breakout(bars);
}());

// volatility_breakout: expansion but bar closes down does not trigger
static_assert([] {
  auto bars = std::array<bar, 30>{};
  // Quiet historical bars
  for (auto i = 0uz; i < 25uz; ++i)
    bars[i] = bar{.close = 100.0,
                  .high = 100.2,
                  .low = 99.8,
                  .open = 100.0,
                  .vwap = 100.0,
                  .volume = 1000,
                  .num_trades = 10,
                  .timestamp = "2026-01-01T10:00:00Z"};
  // Wide recent bars but closing DOWN — should not trigger
  for (auto i = 25uz; i < 30uz; ++i)
    bars[i] = bar{.close = 98.0,
                  .high = 104.0,
                  .low = 96.0,
                  .open = 100.0,
                  .vwap = 100.0,
                  .volume = 1000,
                  .num_trades = 10,
                  .timestamp = "2026-01-01T11:00:00Z"};
  return !volatility_breakout(bars);
}());

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

  // Priority 4: Simple price dip (intraday momentum reversal)
  if (price_dip(history))
    return true;

  // Priority 5: Volatility breakout (expansion from compression)
  if (volatility_breakout(history))
    return true;

  return false;
}

// Dispatch entry check by strategy name — single source of truth for the
// name→function mapping used by entries.cxx and evaluate.cxx.
// Defined in entry.cxx.
bool dispatch_entry(std::string_view, std::span<const bar>);

// is_entry: insufficient history returns false for all strategies
static_assert([] {
  auto bars = std::array<bar, 5>{};
  for (auto &b : bars)
    b = bar{.close = 100.0,
            .high = 101.0,
            .low = 99.0,
            .open = 100.0,
            .vwap = 100.0,
            .volume = 1000,
            .num_trades = 50,
            .timestamp = "2026-01-01T10:00:00Z"};
  return !is_entry(bars);
}());

// is_entry: flat prices with normal volume returns false for all strategies
static_assert([] {
  auto bars = std::array<bar, 25>{};
  for (auto &b : bars)
    b = bar{.close = 100.0,
            .high = 101.0,
            .low = 99.0,
            .open = 100.0,
            .vwap = 100.0,
            .volume = 1000,
            .num_trades = 50,
            .timestamp = "2026-01-01T10:00:00Z"};
  return !is_entry(bars);
}());
