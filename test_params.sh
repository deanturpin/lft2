#!/bin/bash
# Test different parameter combinations and compare results

set -e

echo "Testing different TP/SL/TSL parameter combinations..."
echo ""

# Function to update params and run backtest
test_params() {
  local tp=$1
  local sl=$2
  local tsl=$3
  local name=$4

  echo "=========================================="
  echo "Testing: $name"
  echo "TP: ${tp}% | SL: ${sl}% | TSL: ${tsl}%"
  echo "=========================================="

  # Update params.h
  cat > src/params.h <<EOF
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
constexpr auto default_params = trading_params{.take_profit_pct = ${tp},
                                               .stop_loss_pct = ${sl},
                                               .trailing_stop_pct = ${tsl}};

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
EOF

  # Rebuild
  make build > /dev/null 2>&1

  # Run backtest on a few key symbols
  echo "Running backtest on USO, MSTR, XLE..."
  for symbol in USO MSTR XLE COIN HON; do
    if [ -f "docs/bars/${symbol}.json" ]; then
      ./backtest "${symbol}" 2>/dev/null | grep -A1 "viable" | head -5
    fi
  done

  echo ""
}

# Test 1: Current settings (baseline)
test_params 0.0125 0.0125 0.01 "BASELINE (1.25/1.25/1.0)"

# Test 2: Tighter symmetric
test_params 0.0075 0.0075 0.005 "TIGHT (0.75/0.75/0.5)"

# Test 3: Asymmetric favoring reward
test_params 0.01 0.0075 0.005 "REWARD (1.0/0.75/0.5)"

# Test 4: Very tight scalping
test_params 0.005 0.005 0.003 "SCALP (0.5/0.5/0.3)"

# Test 5: Wider for volatile stocks
test_params 0.02 0.015 0.01 "VOLATILE (2.0/1.5/1.0)"

echo "=========================================="
echo "Testing complete!"
echo "=========================================="
