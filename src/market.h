#pragma once
#include <chrono>
#include <string_view>

// NYSE market hours and trading rules.
//
// Bar timestamps from Alpaca are always UTC (ISO 8601, "Z" suffix).
//
// NYSE regular session: 09:30–16:00 America/New_York.
//
// DST handling: std::chrono::locate_zone() is not constexpr, so we apply the
// offset by month: EDT (UTC-4) for April–October, EST (UTC-5) otherwise.
// DST transitions fall on Sundays in March and November — non-trading days —
// so month granularity is exact for every NYSE trading day.
namespace market {

namespace {

using namespace std::chrono_literals;
using std::chrono::minutes;

// NYSE session boundaries in local ET time (DST-agnostic)
constexpr auto SESSION_OPEN_ET = 9h + 30min; // 09:30 ET
constexpr auto SESSION_CLOSE_ET = 16h;       // 16:00 ET

// Risk window offsets
constexpr auto RISK_ON_DELAY = 60min;  // Skip the volatile first hour
constexpr auto RISK_OFF_START = 30min; // Stop 30 minutes before close

// Returns the UTC offset for America/New_York based on month alone.
// EDT (UTC-4) applies April–October; EST (UTC-5) otherwise.
constexpr minutes utc_offset(unsigned mo) {
  constexpr auto EST = minutes{-5h}; // UTC-5
  constexpr auto EDT = minutes{-4h}; // UTC-4
  return (mo >= 4 && mo <= 10) ? EDT : EST;
}
static_assert(utc_offset(1) == minutes{-5h}); // January   - EST
static_assert(
    utc_offset(3) ==
    minutes{-5h}); // March     - EST (transition month, non-trading Sunday)
static_assert(utc_offset(4) == minutes{-4h});  // April     - EDT
static_assert(utc_offset(7) == minutes{-4h});  // July      - EDT
static_assert(utc_offset(10) == minutes{-4h}); // October   - EDT
static_assert(
    utc_offset(11) ==
    minutes{-5h}); // November  - EST (transition month, non-trading Sunday)
static_assert(utc_offset(12) == minutes{-5h}); // December  - EST

// Parse two ASCII digits from a string_view, returns -1 on invalid input.
constexpr int parse2(std::string_view s) {
  if (s[0] < '0' || s[0] > '9' || s[1] < '0' || s[1] > '9')
    return -1;
  return (s[0] - '0') * 10 + (s[1] - '0');
}
static_assert(parse2("14") == 14);
static_assert(parse2("09") == 9);
static_assert(parse2("00") == 0);
static_assert(parse2("59") == 59);
static_assert(parse2("X9") == -1); // non-digit

// Parse a UTC ISO 8601 timestamp ("2026-02-16T14:30:00Z") and return
// minutes since midnight in America/New_York, accounting for DST.
// Returns -1min on parse failure.
constexpr minutes ny_minutes(std::string_view ts) {
  if (ts.size() < 19)
    return -1min;

  auto mo = parse2(ts.substr(5, 2));
  auto h = parse2(ts.substr(11, 2));
  auto m = parse2(ts.substr(14, 2));

  if (mo < 0 || h < 0 || m < 0)
    return -1min;

  auto utc_min = minutes{h * 60 + m};
  auto offset = utc_offset(static_cast<unsigned>(mo));

  // Apply offset, wrapping into [0, 24h)
  auto local_min = utc_min + offset;
  constexpr auto day_min = minutes{24 * 60};
  if (local_min < minutes{0})
    local_min += day_min;
  if (local_min >= day_min)
    local_min -= day_min;
  return local_min;
}
static_assert(ny_minutes("2026-02-16T14:30:00Z") ==
              9h + 30min); // 14:30 UTC = 09:30 EST
static_assert(ny_minutes("2026-07-01T13:30:00Z") ==
              9h + 30min); // 13:30 UTC = 09:30 EDT
static_assert(ny_minutes("2026-02-16T21:00:00Z") ==
              16h); // 21:00 UTC = 16:00 EST
static_assert(ny_minutes("2026-07-01T20:00:00Z") ==
              16h);                        // 20:00 UTC = 16:00 EDT
static_assert(ny_minutes("bad") == -1min); // too short → failure

} // anonymous namespace

// ============================================================
// Public API: two functions, one decision tree
// ============================================================

// True while the NYSE regular session is open (09:30–16:00 ET, DST-aware).
constexpr bool market_open(std::string_view timestamp) {
  auto t = ny_minutes(timestamp);
  return t >= SESSION_OPEN_ET && t < SESSION_CLOSE_ET;
}
// EST (winter, UTC-5): session is 14:30–21:00 UTC
static_assert(market_open("2026-02-16T14:30:00Z"));  // 09:30 ET - open
static_assert(market_open("2026-02-16T20:59:00Z"));  // 15:59 ET - open
static_assert(!market_open("2026-02-16T14:29:00Z")); // 09:29 ET - not yet open
static_assert(!market_open("2026-02-16T21:00:00Z")); // 16:00 ET - closed
static_assert(!market_open("2026-02-16T13:00:00Z")); // 08:00 ET - pre-market
// EDT (summer, UTC-4): session is 13:30–20:00 UTC
static_assert(market_open("2026-07-01T13:30:00Z"));  // 09:30 ET - open
static_assert(market_open("2026-07-01T19:59:00Z"));  // 15:59 ET - open
static_assert(!market_open("2026-07-01T13:29:00Z")); // 09:29 ET - not yet open
static_assert(!market_open("2026-07-01T20:00:00Z")); // 16:00 ET - closed

// True during the risk-off period: first hour after open and last 30 min.
//
// Intended usage:
//   if (!market_open(ts)) return;    // market closed - nothing to do
//   if (risk_off(ts))     liquidate; // unsafe window - close all positions
//   else                  check_exits_and_entries;
constexpr bool risk_off(std::string_view timestamp) {
  if (!market_open(timestamp))
    return false;

  auto t = ny_minutes(timestamp);
  auto risk_start = SESSION_OPEN_ET + RISK_ON_DELAY; // 10:30 ET
  auto risk_end = SESSION_CLOSE_ET - RISK_OFF_START; // 15:30 ET

  return t < risk_start || t >= risk_end;
}
// Unsafe: 09:30–10:29 ET and 15:30–15:59 ET; safe: 10:30–15:29 ET
static_assert(risk_off("2026-02-16T14:30:00Z")); // 09:30 ET - first hour
static_assert(
    risk_off("2026-02-16T15:29:00Z")); // 10:29 ET - one minute before safe
static_assert(
    !risk_off("2026-02-16T15:30:00Z")); // 10:30 ET - safe window starts
static_assert(!risk_off("2026-02-16T18:00:00Z")); // 13:00 ET - mid-day
static_assert(
    !risk_off("2026-02-16T20:29:00Z")); // 15:29 ET - one minute before cutoff
static_assert(risk_off("2026-02-16T20:30:00Z"));  // 15:30 ET - last 30 minutes
static_assert(!risk_off("2026-02-16T21:00:00Z")); // 16:00 ET - market closed
static_assert(!risk_off("2026-02-16T13:00:00Z")); // 08:00 ET - pre-market

} // namespace market
