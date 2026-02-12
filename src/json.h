#pragma once
#include "bar.h"
#include <array>
#include <string_view>

// Minimal constexpr JSON parser for Alpaca bar data
// Parses:
// {"bars":[{"c":val,"h":val,"l":val,"o":val,"t":"str","v":val,"vw":val,"n":val},...]}
// Designed to work with compile-time embedded JSON data via C++26 #embed

// Skip whitespace
constexpr void skip_ws(std::string_view &s) {
  while (!s.empty() &&
         (s[0] == ' ' || s[0] == '\n' || s[0] == '\r' || s[0] == '\t'))
    s.remove_prefix(1);
}

namespace {
constexpr bool test_skip_ws() {
  auto s = std::string_view{"  \n\t  hello"};
  skip_ws(s);
  return s == "hello";
}
static_assert(test_skip_ws());
} // namespace

// Expect and consume a specific character
constexpr bool expect(std::string_view &s, char c) {
  skip_ws(s);
  if (s.empty() || s[0] != c)
    return false;
  s.remove_prefix(1);
  return true;
}

namespace {
constexpr bool test_expect() {
  auto s1 = std::string_view{"  {"};
  if (!expect(s1, '{'))
    return false;

  auto s2 = std::string_view{"}"};
  if (expect(s2, '{'))
    return false;

  return true;
}
static_assert(test_expect());
} // namespace

// Parse a string value between quotes
constexpr std::string_view parse_string(std::string_view &s) {
  skip_ws(s);
  if (s.empty() || s[0] != '"')
    return {};

  s.remove_prefix(1);
  auto start = s.data();
  auto len = 0uz;

  while (!s.empty() && s[0] != '"') {
    s.remove_prefix(1);
    ++len;
  }

  if (!s.empty())
    s.remove_prefix(1);

  return {start, len};
}

namespace {
constexpr bool test_parse_string() {
  auto s1 = std::string_view{R"("hello")"};
  auto result1 = parse_string(s1);
  if (result1 != "hello")
    return false;

  auto s2 = std::string_view{R"(  "world"  )"};
  auto result2 = parse_string(s2);
  if (result2 != "world")
    return false;

  return true;
}
static_assert(test_parse_string());
} // namespace

// Parse a numeric value (double or unsigned long)
template <typename T> constexpr T parse_number(std::string_view &s) {
  skip_ws(s);

  auto start = s.data();
  auto len = 0uz;

  // Find end of number
  while (!s.empty() &&
         ((s[0] >= '0' && s[0] <= '9') || s[0] == '.' || s[0] == '-' ||
          s[0] == 'e' || s[0] == 'E' || s[0] == '+')) {
    s.remove_prefix(1);
    ++len;
  }

  // Simple constexpr parser for integers and floats
  auto result = T{};
  auto sv = std::string_view{start, len};
  auto negative = false;

  if (!sv.empty() && sv[0] == '-') {
    negative = true;
    sv.remove_prefix(1);
  }

  // Parse integer part
  auto int_part = 0.0;
  while (!sv.empty() && sv[0] >= '0' && sv[0] <= '9') {
    int_part = int_part * 10 + (sv[0] - '0');
    sv.remove_prefix(1);
  }

  // Parse fractional part
  auto frac_part = 0.0;
  if (!sv.empty() && sv[0] == '.') {
    sv.remove_prefix(1);
    auto divisor = 10.0;
    while (!sv.empty() && sv[0] >= '0' && sv[0] <= '9') {
      frac_part += (sv[0] - '0') / divisor;
      divisor *= 10;
      sv.remove_prefix(1);
    }
  }

  result = static_cast<T>((int_part + frac_part) * (negative ? -1 : 1));
  return result;
}

namespace {
constexpr bool test_parse_number() {
  auto s1 = std::string_view{"42"};
  if (parse_number<int>(s1) != 42)
    return false;

  auto s2 = std::string_view{"3.14159"};
  auto result2 = parse_number<double>(s2);
  if (result2 < 3.14 || result2 > 3.15)
    return false;

  auto s3 = std::string_view{"-123"};
  if (parse_number<int>(s3) != -123)
    return false;

  auto s4 = std::string_view{"255.75"};
  auto result4 = parse_number<double>(s4);
  if (result4 < 255.74 || result4 > 255.76)
    return false;

  return true;
}
static_assert(test_parse_number());
} // namespace

// Parse a single bar object
constexpr bar parse_bar(std::string_view &s) {
  auto b = bar{};

  if (!expect(s, '{'))
    return b;

  while (!s.empty() && s[0] != '}') {
    skip_ws(s);

    // Parse key
    auto key = parse_string(s);
    if (!expect(s, ':'))
      return b;

    // Parse value based on key
    if (key == "c")
      b.close = parse_number<double>(s);
    else if (key == "h")
      b.high = parse_number<double>(s);
    else if (key == "l")
      b.low = parse_number<double>(s);
    else if (key == "o")
      b.open = parse_number<double>(s);
    else if (key == "t")
      b.timestamp = parse_string(s);
    else if (key == "v")
      b.volume = parse_number<unsigned long>(s);
    else if (key == "vw")
      b.vwap = parse_number<double>(s);
    else if (key == "n")
      b.num_trades = parse_number<unsigned long>(s);

    skip_ws(s);
    if (s[0] == ',')
      s.remove_prefix(1);
  }

  expect(s, '}');
  return b;
}

namespace {
constexpr bool test_parse_bar() {
  auto json = std::string_view{R"({
    "c": 255.75,
    "h": 255.855,
    "l": 255.47,
    "o": 255.63,
    "t": "2026-01-29T16:35:00Z",
    "v": 20688,
    "vw": 255.72,
    "n": 100
  })"};

  auto b = parse_bar(json);

  if (b.close < 255.74 || b.close > 255.76)
    return false;
  if (b.high < 255.85 || b.high > 255.86)
    return false;
  if (b.low < 255.46 || b.low > 255.48)
    return false;
  if (b.open < 255.62 || b.open > 255.64)
    return false;
  if (b.timestamp != "2026-01-29T16:35:00Z")
    return false;
  if (b.volume != 20688)
    return false;
  if (b.num_trades != 100)
    return false;

  return true;
}
static_assert(test_parse_bar());
} // namespace

// Parse full JSON response into array of bars
template <std::size_t N>
constexpr auto parse_bars(std::string_view json) -> std::array<bar, N> {
  auto bars = std::array<bar, N>{};
  auto s = json;

  // Expect {"bars":[
  if (!expect(s, '{'))
    return bars;

  skip_ws(s);
  auto key = parse_string(s);
  if (key != "bars")
    return bars;

  if (!expect(s, ':') || !expect(s, '['))
    return bars;

  // Parse array of bars
  for (auto i = 0uz; i < N; ++i) {
    skip_ws(s);
    if (s.empty() || s[0] == ']')
      break;

    bars[i] = parse_bar(s);

    skip_ws(s);
    if (s[0] == ',')
      s.remove_prefix(1);
  }

  return bars;
}

namespace {
constexpr bool test_parse_bars() {
  constexpr auto json = std::string_view{R"({
    "bars": [
      {
        "c": 255.75,
        "h": 255.855,
        "l": 255.47,
        "o": 255.63,
        "t": "2026-01-29T16:35:00Z",
        "v": 20688,
        "vw": 255.72,
        "n": 100
      },
      {
        "c": 255.69,
        "h": 256.325,
        "l": 255.66,
        "o": 256.28,
        "t": "2026-01-29T16:30:00Z",
        "v": 24829,
        "vw": 256.1,
        "n": 110
      }
    ]
  })"};

  constexpr auto bars = parse_bars<2>(json);

  if (bars[0].close < 255.74 || bars[0].close > 255.76)
    return false;
  if (bars[0].timestamp != "2026-01-29T16:35:00Z")
    return false;
  if (bars[0].volume != 20688)
    return false;

  if (bars[1].open < 256.27 || bars[1].open > 256.29)
    return false;
  if (bars[1].timestamp != "2026-01-29T16:30:00Z")
    return false;
  if (bars[1].volume != 24829)
    return false;

  return true;
}
static_assert(test_parse_bars());
} // namespace
