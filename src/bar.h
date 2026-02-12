#pragma once
#include <cstdint>
#include <string_view>

// Core price bar structure representing a single time period of market data.
// Contains OHLC prices, volume, timestamp, and derived metrics.
struct bar {
  double close{};
  double high{};
  double low{};
  double open{};
  double vwap{}; // Volume-weighted average price for this period

  std::uint32_t volume{};     // Number of shares traded
  std::uint32_t num_trades{}; // Count of individual trades

  std::string_view timestamp{};
};
