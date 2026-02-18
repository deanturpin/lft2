#include "json.h"
#include <print>

// Test parsing empty bars array
namespace {
constexpr bool test_empty_bars() {
  constexpr auto json = std::string_view{R"({"bars":[]})"};
  constexpr auto bars = parse_bars<5>(json);

  // All bars should be default-initialized
  return bars[0].close == 0.0 && bars[0].timestamp.empty();
}
static_assert(test_empty_bars());
} // namespace

// Test parsing single bar
namespace {
constexpr bool test_single_bar() {
  constexpr auto json = std::string_view{R"({
    "bars": [{
      "c": 150.25,
      "h": 151.00,
      "l": 149.50,
      "o": 150.00,
      "t": "2026-01-15T10:00:00Z",
      "v": 1000000,
      "vw": 150.30,
      "n": 500
    }]
  })"};

  constexpr auto bars = parse_bars<1>(json);
  return bars[0].close > 150.24 && bars[0].close < 150.26 &&
         bars[0].timestamp == "2026-01-15T10:00:00Z" &&
         bars[0].volume == 1000000;
}
static_assert(test_single_bar());
} // namespace

// Test parsing multiple bars
namespace {
constexpr bool test_multiple_bars() {
  constexpr auto json = std::string_view{R"({
    "bars": [
      {"c": 100.0, "h": 101.0, "l": 99.0, "o": 100.5, "t": "2026-01-01T10:00:00Z", "v": 1000, "vw": 100.2, "n": 10},
      {"c": 101.0, "h": 102.0, "l": 100.0, "o": 101.5, "t": "2026-01-01T11:00:00Z", "v": 2000, "vw": 101.3, "n": 20},
      {"c": 102.0, "h": 103.0, "l": 101.0, "o": 102.5, "t": "2026-01-01T12:00:00Z", "v": 3000, "vw": 102.4, "n": 30}
    ]
  })"};

  constexpr auto bars = parse_bars<3>(json);
  return bars[0].close == 100.0 && bars[1].close == 101.0 &&
         bars[2].close == 102.0 &&
         bars[0].timestamp == "2026-01-01T10:00:00Z" && bars[2].volume == 3000;
}
static_assert(test_multiple_bars());
} // namespace

// Test parsing with large values
namespace {
constexpr bool test_large_values() {
  constexpr auto json = std::string_view{R"({
    "bars": [{
      "c": 9999999.99,
      "h": 10000000.00,
      "l": 9999998.00,
      "o": 9999999.50,
      "t": "2026-01-01T10:00:00Z",
      "v": 999999999,
      "vw": 9999999.75,
      "n": 100000
    }]
  })"};

  constexpr auto bars = parse_bars<1>(json);
  return bars[0].close > 9999999.98 && bars[0].close < 10000000.00 &&
         bars[0].volume == 999999999;
}
static_assert(test_large_values());
} // namespace

// Test parsing with negative values (validates parser handles negatives,
// not realistic for stock prices)
namespace {
constexpr bool test_negative_values() {
  constexpr auto json = std::string_view{R"({
    "bars": [{
      "c": -50.25,
      "h": -49.00,
      "l": -51.50,
      "o": -50.00,
      "t": "2026-01-01T10:00:00Z",
      "v": 1000,
      "vw": -50.30,
      "n": 10
    }]
  })"};

  constexpr auto bars = parse_bars<1>(json);
  return bars[0].close < -50.24 && bars[0].close > -50.26 &&
         bars[0].high < -48.99 && bars[0].high > -49.01;
}
static_assert(test_negative_values());
} // namespace

// Test array size mismatch (requesting more bars than available)
namespace {
constexpr bool test_size_mismatch() {
  constexpr auto json = std::string_view{R"({
    "bars": [
      {"c": 100.0, "h": 101.0, "l": 99.0, "o": 100.5, "t": "2026-01-01T10:00:00Z", "v": 1000, "vw": 100.2, "n": 10}
    ]
  })"};

  // Request 5 bars but only 1 available
  constexpr auto bars = parse_bars<5>(json);

  // First bar should be valid, rest should be default-initialized
  return bars[0].close == 100.0 && bars[1].close == 0.0 &&
         bars[0].timestamp == "2026-01-01T10:00:00Z" &&
         bars[1].timestamp.empty();
}
static_assert(test_size_mismatch());
} // namespace

int main() { std::println("All compile-time tests passed."); }
