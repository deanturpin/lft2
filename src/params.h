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
constexpr auto default_params = trading_params{.take_profit_pct = 0.02,
                                               .stop_loss_pct = 0.015,
                                               .trailing_stop_pct = 0.01};

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
static_assert(default_params.take_profit_pct >= default_params.stop_loss_pct,
              "Take profit should be >= stop loss for positive expectancy");
static_assert(default_params.trailing_stop_pct < default_params.stop_loss_pct,
              "Trailing stop should be tighter than initial stop loss");

} // namespace
