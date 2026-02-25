#pragma once

// Trading parameters for position management
struct trading_params {
  double take_profit_pct; // Take profit as percentage above entry (e.g., 0.10 =
                          // 10%)
  double stop_loss_pct; // Stop loss as percentage below entry (e.g., 0.05 = 5%)
  double trailing_stop_pct; // Trailing stop distance as percentage (e.g., 0.03
                            // = 3%)
};

// Default trading parameters
constexpr auto default_params = trading_params{
    .take_profit_pct = 0.0125, .stop_loss_pct = 0.0125, .trailing_stop_pct = 0.01};

// Calculate absolute price levels from entry price and parameters
constexpr auto calculate_levels(double entry_price, trading_params params) {
  struct levels {
    double take_profit;
    double stop_loss;
    double trailing_stop;
  };

  return levels{.take_profit = entry_price * (1.0 + params.take_profit_pct),
                .stop_loss = entry_price * (1.0 - params.stop_loss_pct),
                .trailing_stop =
                    entry_price * (1.0 - params.trailing_stop_pct)};
}

// Unit tests
namespace {

// Test: default params are positive
static_assert(default_params.take_profit_pct > 0.0,
              "Take profit percentage must be positive");
static_assert(default_params.stop_loss_pct > 0.0,
              "Stop loss percentage must be positive");
static_assert(default_params.trailing_stop_pct > 0.0,
              "Trailing stop percentage must be positive");

// Test: risk/reward relationship makes sense
static_assert(
    default_params.take_profit_pct >= default_params.stop_loss_pct,
    "Take profit should be >= stop loss for positive expectancy");
static_assert(default_params.trailing_stop_pct < default_params.stop_loss_pct,
              "Trailing stop should be tighter than initial stop loss");

// Test: calculate levels with default params
static_assert(
    []() constexpr {
      auto entry = 100.0;
      auto levels = calculate_levels(entry, default_params);
      // Use entry * (1 + pct) for exact floating point match
      return levels.take_profit == entry * 1.0125 &&
             levels.stop_loss == entry * 0.9875 &&
             levels.trailing_stop == entry * 0.99;
    }(),
    "Default params should give TP=+1.25%, SL=-1.25%, trailing=-1% for entry=100");

// Test: calculate levels with custom params
static_assert(
    []() constexpr {
      auto entry = 200.0;
      auto params = trading_params{0.20, 0.10, 0.05};
      auto levels = calculate_levels(entry, params);
      return levels.take_profit == 240.0 && levels.stop_loss == 180.0 &&
             levels.trailing_stop == 190.0;
    }(),
    "Custom params (20%, 10%, 5%) should give TP=240, SL=180, trailing=190 for "
    "entry=200");
} // namespace
