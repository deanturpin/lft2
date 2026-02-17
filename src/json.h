#pragma once
#include "bar.h"
#include <array>
#include <string_view>

// Minimal constexpr JSON parser for Alpaca bar data
// Parses:
// {"bars":[{"c":val,"h":val,"l":val,"o":val,"t":"str","v":val,"vw":val,"n":val},...]}
// Designed to work with compile-time embedded JSON data via C++26 #embed
//
// Limitations (intentional — Alpaca data never requires these):
//   - No escape sequence support in strings (e.g. \", \\)
//   - No scientific notation in numbers (e.g. 1.23e-4)

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
  if (!s.starts_with(c))
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

// Skip optional comma (used after every value in arrays and objects)
constexpr void skip_comma(std::string_view &s) {
  skip_ws(s);
  if (s.starts_with(','))
    s.remove_prefix(1);
}

// Parse a string value between quotes.
// No escape sequence support — Alpaca strings contain none.
constexpr std::string_view parse_string(std::string_view &s) {
  skip_ws(s);
  if (!s.starts_with('"'))
    return {};

  s.remove_prefix(1);
  auto end = s.find('"');
  if (end == std::string_view::npos)
    return {};

  auto result = s.substr(0, end);
  s.remove_prefix(end + 1);
  return result;
}

namespace {
constexpr bool test_parse_string() {
  auto s1 = std::string_view{R"("hello")"};
  if (parse_string(s1) != "hello")
    return false;

  auto s2 = std::string_view{R"(  "world"  )"};
  if (parse_string(s2) != "world")
    return false;

  return true;
}
static_assert(test_parse_string());
} // namespace

// Parse a numeric value (integer or decimal).
// Scientific notation is not supported — Alpaca never uses it.
template <typename T> constexpr T parse_number(std::string_view &s) {
  skip_ws(s);

  auto start = s.data();
  auto len = 0uz;

  // Scan digits, sign, and decimal point only — no e/E/+
  while (!s.empty() &&
         ((s[0] >= '0' && s[0] <= '9') || s[0] == '.' || s[0] == '-')) {
    s.remove_prefix(1);
    ++len;
  }

  auto sv = std::string_view{start, len};
  auto negative = sv.starts_with('-');
  if (negative)
    sv.remove_prefix(1);

  // Parse integer part
  auto int_part = 0.0;
  while (!sv.empty() && sv[0] >= '0' && sv[0] <= '9') {
    int_part = int_part * 10 + (sv[0] - '0');
    sv.remove_prefix(1);
  }

  // Parse fractional part
  auto frac_part = 0.0;
  if (sv.starts_with('.')) {
    sv.remove_prefix(1);
    auto divisor = 10.0;
    while (!sv.empty() && sv[0] >= '0' && sv[0] <= '9') {
      frac_part += (sv[0] - '0') / divisor;
      divisor *= 10;
      sv.remove_prefix(1);
    }
  }

  return static_cast<T>((int_part + frac_part) * (negative ? -1 : 1));
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

  while (!s.empty() && !s.starts_with('}')) {
    skip_ws(s);

    auto key = parse_string(s);
    if (!expect(s, ':'))
      return b;

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

    skip_comma(s);
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
constexpr std::array<bar, N> parse_bars(std::string_view json) {
  auto bars = std::array<bar, N>{};
  auto s = json;

  if (!expect(s, '{'))
    return bars;

  skip_ws(s);
  if (parse_string(s) != "bars")
    return bars;

  if (!expect(s, ':') || !expect(s, '['))
    return bars;

  for (auto i = 0uz; i < N; ++i) {
    skip_ws(s);
    if (s.empty() || s.starts_with(']'))
      break;

    bars[i] = parse_bar(s);
    skip_comma(s);
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

// ============================================================
// Key-based field extraction for generic JSON objects.
// These operate on a string_view of an already-isolated object
// (i.e. the content between { and }), scanning for a named key.
// ============================================================

// Extract a string value for a given key from a JSON object fragment.
// Returns empty string_view if the key is not found.
constexpr std::string_view json_string(std::string_view obj,
                                       std::string_view key) {
  auto s = obj;
  while (!s.empty()) {
    skip_ws(s);
    auto k = parse_string(s);
    if (!expect(s, ':'))
      return {};
    if (k == key)
      return parse_string(s);
    // Skip the value (string or number) and move to next key
    skip_ws(s);
    if (s.starts_with('"'))
      parse_string(s);
    else
      parse_number<double>(s);
    skip_comma(s);
  }
  return {};
}

// Extract a numeric value for a given key from a JSON object fragment.
// Handles both bare numbers and quoted numbers ("3.5" or 3.5) — Alpaca
// returns numeric fields like qty and avg_entry_price as quoted strings.
// Returns 0 if the key is not found.
template <typename T = double>
constexpr T json_number(std::string_view obj, std::string_view key) {
  auto s = obj;
  while (!s.empty()) {
    skip_ws(s);
    auto k = parse_string(s);
    if (!expect(s, ':'))
      return {};
    if (k == key) {
      skip_ws(s);
      // Accept both quoted ("3.5") and bare (3.5) numeric values
      if (s.starts_with('"')) {
        auto v = parse_string(s);
        return parse_number<T>(v);
      }
      return parse_number<T>(s);
    }
    // Skip the value and move to next key
    skip_ws(s);
    if (s.starts_with('"'))
      parse_string(s);
    else
      parse_number<double>(s);
    skip_comma(s);
  }
  return {};
}

namespace {
// Alpaca returns numeric fields as quoted strings in positions.json
constexpr auto test_obj = std::string_view{
    R"("symbol": "AAPL", "qty": "3", "avg_entry_price": "182.5", "side": "long")"};
static_assert(json_string(test_obj, "symbol") == "AAPL");
static_assert(json_string(test_obj, "side") == "long");
static_assert(json_number(test_obj, "qty") == 3.0);
static_assert(json_number(test_obj, "avg_entry_price") == 182.5);
static_assert(json_string(test_obj, "missing") == "");
// Bare numeric values also work
constexpr auto test_bare = std::string_view{R"("price": 99.5, "vol": 1000)"};
static_assert(json_number(test_bare, "price") == 99.5);
} // namespace

// Extract a string array for a given key from a top-level JSON object.
// Calls fn(std::string_view) once per element.
// E.g. json_string_array(json, "symbols", fn) on {"symbols":["AAPL","TSLA"]}
template <typename Fn>
constexpr void json_string_array(std::string_view json, std::string_view key,
                                 Fn fn) {
  auto s = json;
  if (!expect(s, '{'))
    return;
  while (!s.empty() && !s.starts_with('}')) {
    skip_ws(s);
    auto k = parse_string(s);
    if (!expect(s, ':'))
      return;
    if (k == key) {
      if (!expect(s, '['))
        return;
      while (!s.empty() && !s.starts_with(']')) {
        skip_ws(s);
        if (s.empty() || s.starts_with(']'))
          break;
        fn(parse_string(s));
        skip_comma(s);
      }
      return;
    }
    // Skip value (string, number, or nested array) and advance
    skip_ws(s);
    if (s.starts_with('[')) {
      // Skip nested array by tracking depth
      auto depth = 1;
      s.remove_prefix(1);
      while (!s.empty() && depth > 0) {
        if (s.starts_with('['))
          ++depth;
        else if (s.starts_with(']'))
          --depth;
        s.remove_prefix(1);
      }
    } else if (s.starts_with('"'))
      parse_string(s);
    else
      parse_number<double>(s);
    skip_comma(s);
  }
}

namespace {
constexpr bool test_json_string_array() {
  constexpr auto json =
      std::string_view{R"({"symbols":["AAPL","TSLA","NVDA"]})"};
  auto count = 0;
  auto first = std::string_view{};
  json_string_array(json, "symbols", [&](std::string_view v) {
    if (count == 0)
      first = v;
    ++count;
  });
  return count == 3 && first == "AAPL";
}
static_assert(test_json_string_array());
} // namespace
