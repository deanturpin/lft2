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
constexpr auto RISK_ON_DELAY = 15min; // Skip volatile opening period
constexpr auto RISK_OFF_START = 75min; // Start liquidation before close (3:45 PM ET)
                                        // With 15-min data delay, gives ~60 min real time
                                        // to liquidate before 4:00 PM close

// Returns the UTC offset for America/New_York based on month and day.
// EDT (UTC-4) applies mid-March to early November; EST (UTC-5) otherwise.
// DST starts second Sunday of March (Mar 8-14), ends first Sunday of November (Nov 1-7).
constexpr minutes utc_offset(unsigned mo, unsigned day) {
  constexpr auto EST = minutes{-5h}; // UTC-5
  constexpr auto EDT = minutes{-4h}; // UTC-4

  // March: DST starts second Sunday (day 8-14), so assume EDT from day 9 onward
  if (mo == 3) return day >= 9 ? EDT : EST;

  // November: DST ends first Sunday (day 1-7), so assume EST from day 2 onward
  if (mo == 11) return day >= 2 ? EST : EDT;

  // April-October: EDT
  if (mo >= 4 && mo <= 10) return EDT;

  // December-February: EST
  return EST;
}
static_assert(utc_offset(1, 15) == minutes{-5h});  // January - EST
static_assert(utc_offset(3, 8) == minutes{-5h});   // March 8 - EST (DST starts)
static_assert(utc_offset(3, 9) == minutes{-4h});   // March 9 - EDT (after transition)
static_assert(utc_offset(3, 15) == minutes{-4h});  // March 15 - EDT
static_assert(utc_offset(4, 15) == minutes{-4h});  // April - EDT
static_assert(utc_offset(7, 15) == minutes{-4h});  // July - EDT
static_assert(utc_offset(10, 15) == minutes{-4h}); // October - EDT
static_assert(utc_offset(11, 1) == minutes{-4h});  // November 1 - EDT (DST ends)
static_assert(utc_offset(11, 2) == minutes{-5h});  // November 2 - EST (after transition)
static_assert(utc_offset(12, 15) == minutes{-5h}); // December - EST

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
  auto day = parse2(ts.substr(8, 2));
  auto h = parse2(ts.substr(11, 2));
  auto m = parse2(ts.substr(14, 2));

  if (mo < 0 || day < 0 || h < 0 || m < 0)
    return -1min;

  auto utc_min = minutes{h * 60 + m};
  auto offset = utc_offset(static_cast<unsigned>(mo), static_cast<unsigned>(day));

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

// True during the risk-off period: opening volatility window and pre-close
// liquidation window.
//
// Intended usage:
//   if (!market_open(ts)) return;    // market closed - nothing to do
//   if (risk_off(ts))     liquidate; // unsafe window - close all positions
//   else                  check_exits_and_entries;
constexpr bool risk_off(std::string_view timestamp) {
  if (!market_open(timestamp))
    return false;

  auto t = ny_minutes(timestamp);
  auto risk_start = SESSION_OPEN_ET + RISK_ON_DELAY;
  auto risk_end = SESSION_CLOSE_ET - RISK_OFF_START;

  return t < risk_start || t >= risk_end;
}
// Unsafe: 09:30–09:44 ET and 14:45–15:59 ET; safe: 09:45–14:44 ET
static_assert(risk_off("2026-02-16T14:30:00Z")); // 09:30 ET - first 15 min
static_assert(
    risk_off("2026-02-16T14:44:00Z")); // 09:44 ET - one minute before safe
static_assert(
    !risk_off("2026-02-16T14:45:00Z")); // 09:45 ET - safe window starts
static_assert(!risk_off("2026-02-16T18:00:00Z")); // 13:00 ET - mid-day
static_assert(
    !risk_off("2026-02-16T19:44:00Z")); // 14:44 ET - one minute before cutoff
static_assert(risk_off("2026-02-16T19:45:00Z"));  // 14:45 ET - last 75 minutes
static_assert(risk_off("2026-02-16T20:15:00Z"));  // 15:15 ET - still risk-off
static_assert(!risk_off("2026-02-16T21:00:00Z")); // 16:00 ET - market closed
static_assert(!risk_off("2026-02-16T13:00:00Z")); // 08:00 ET - pre-market

} // namespace market
