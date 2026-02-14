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

// Validate OHLC relationships and other constraints
constexpr bool is_valid(bar b) {
  // OHLC relationships: high >= all prices, low <= all prices
  if (!(b.high >= b.close && b.high >= b.open && b.high >= b.low &&
        b.low <= b.close && b.low <= b.open))
    return false;

  // All prices must be positive
  if (b.close <= 0.0 || b.high <= 0.0 || b.low <= 0.0 || b.open <= 0.0)
    return false;

  // VWAP should be within high/low range if non-zero
  if (b.vwap > 0.0 && (b.vwap < b.low || b.vwap > b.high))
    return false;

  // Timestamp must not be empty and have reasonable length for ISO 8601
  // Expected format: "2025-01-01T10:00:00Z" (20 chars minimum)
  if (b.timestamp.empty() || b.timestamp.size() < 20)
    return false;

  // Basic ISO 8601 format check: YYYY-MM-DD
  if (b.timestamp.size() >= 10) {
    if (b.timestamp[4] != '-' || b.timestamp[7] != '-')
      return false;
  }

  return true;
}

// Unit tests
namespace {
// Test: valid bar
static_assert(
    [] {
      auto b = bar{.close = 100.0,
                   .high = 101.0,
                   .low = 99.0,
                   .open = 100.5,
                   .vwap = 100.2,
                   .timestamp = "2025-01-01T10:00:00Z"};
      return is_valid(b);
    }(),
    "Bar validation failed: close=100.0, high=101.0, low=99.0, open=100.5, "
    "vwap=100.2");

// Test: high below close (invalid)
static_assert(
    [] {
      auto b = bar{.close = 100.0,
                   .high = 99.0,
                   .low = 98.0,
                   .open = 99.5,
                   .timestamp = "2025-01-01T10:00:00Z"};
      return !is_valid(b);
    }(),
    "Bar incorrectly validated: high=99.0 must be >= close=100.0");

// Test: low above open (invalid)
static_assert(
    [] {
      auto b = bar{.close = 100.0,
                   .high = 101.0,
                   .low = 100.5,
                   .open = 100.0,
                   .timestamp = "2025-01-01T10:00:00Z"};
      return !is_valid(b);
    }(),
    "Bar incorrectly validated: low=100.5 must be <= open=100.0");

// Test: equal OHLC (valid)
static_assert(
    [] {
      auto b = bar{.close = 100.0,
                   .high = 100.0,
                   .low = 100.0,
                   .open = 100.0,
                   .timestamp = "2025-01-01T10:00:00Z"};
      return is_valid(b);
    }(),
    "Bar validation failed: equal OHLC=100.0 should be valid");

// Test: negative price (invalid)
static_assert(
    [] {
      auto b = bar{.close = -100.0,
                   .high = 101.0,
                   .low = 99.0,
                   .open = 100.0,
                   .timestamp = "2025-01-01T10:00:00Z"};
      return !is_valid(b);
    }(),
    "Bar incorrectly validated: close=-100.0 must be positive");

// Test: zero price (invalid)
static_assert(
    [] {
      auto b = bar{.close = 0.0,
                   .high = 101.0,
                   .low = 99.0,
                   .open = 100.0,
                   .timestamp = "2025-01-01T10:00:00Z"};
      return !is_valid(b);
    }(),
    "Bar incorrectly validated: close=0.0 must be positive");

// Test: VWAP above high (invalid)
static_assert(
    [] {
      auto b = bar{.close = 100.0,
                   .high = 101.0,
                   .low = 99.0,
                   .open = 100.0,
                   .vwap = 102.0,
                   .timestamp = "2025-01-01T10:00:00Z"};
      return !is_valid(b);
    }(),
    "Bar incorrectly validated: vwap=102.0 must be <= high=101.0");

// Test: VWAP below low (invalid)
static_assert(
    [] {
      auto b = bar{.close = 100.0,
                   .high = 101.0,
                   .low = 99.0,
                   .open = 100.0,
                   .vwap = 98.0,
                   .timestamp = "2025-01-01T10:00:00Z"};
      return !is_valid(b);
    }(),
    "Bar incorrectly validated: vwap=98.0 must be >= low=99.0");

// Test: VWAP within range (valid)
static_assert(
    [] {
      auto b = bar{.close = 100.0,
                   .high = 101.0,
                   .low = 99.0,
                   .open = 100.0,
                   .vwap = 100.0,
                   .timestamp = "2025-01-01T10:00:00Z"};
      return is_valid(b);
    }(),
    "Bar validation failed: vwap=100.0 within range [99.0, 101.0] should be "
    "valid");

// Test: empty timestamp (invalid)
static_assert(
    [] {
      auto b = bar{.close = 100.0,
                   .high = 101.0,
                   .low = 99.0,
                   .open = 100.0,
                   .timestamp = ""};
      return !is_valid(b);
    }(),
    "Bar incorrectly validated: timestamp is empty");

// Test: short timestamp (invalid)
static_assert(
    [] {
      auto b = bar{.close = 100.0,
                   .high = 101.0,
                   .low = 99.0,
                   .open = 100.0,
                   .timestamp = "2025-01-01"};
      return !is_valid(b);
    }(),
    "Bar incorrectly validated: timestamp='2025-01-01' too short (10 chars, "
    "need >=20)");

// Test: malformed timestamp (invalid)
static_assert(
    [] {
      auto b = bar{.close = 100.0,
                   .high = 101.0,
                   .low = 99.0,
                   .open = 100.0,
                   .timestamp = "20250101T10:00:00Z"};
      return !is_valid(b);
    }(),
    "Bar incorrectly validated: timestamp='20250101T10:00:00Z' missing dashes "
    "at positions 4 and 7");
} // namespace
