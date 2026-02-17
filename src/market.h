#pragma once
#include <string_view>

// NYSE market hours and trading rules
// All times in UTC (NYSE is ET: UTC-5 in winter, UTC-4 in summer)
namespace market {

namespace {

// Market hours (regular session)
// NYSE: 9:30 AM - 4:00 PM ET
// Winter (EST): 14:30 - 21:00 UTC
// Summer (EDT): 13:30 - 20:00 UTC
// Using winter hours (most conservative)
constexpr auto OPEN_HOUR    = 14;   // 14:30 UTC = 9:30 AM ET
constexpr auto OPEN_MINUTE  = 30;
constexpr auto CLOSE_HOUR   = 21;   // 21:00 UTC = 4:00 PM ET
constexpr auto CLOSE_MINUTE = 0;

// Risk window offsets
constexpr auto RISK_ON_DELAY_MINUTES  = 60;  // Skip first hour (volatile open)
constexpr auto RISK_OFF_ADVANCE_MINUTES = 30; // Stop 30 minutes before close

constexpr int to_minutes(int hour, int minute) {
	return hour * 60 + minute;
}

// Extract minutes-since-midnight from an ISO 8601 timestamp.
// Format: "2026-02-16T14:30:00Z"  (hour at [11..12], minute at [14..15])
constexpr int timestamp_minutes(std::string_view ts) {
	if (ts.size() < 17)
		return -1;

	auto parse2 = [](std::string_view s) {
		if (s[0] < '0' || s[0] > '9' || s[1] < '0' || s[1] > '9')
			return -1;
		return (s[0] - '0') * 10 + (s[1] - '0');
	};

	auto hour   = parse2(ts.substr(11, 2));
	auto minute = parse2(ts.substr(14, 2));
	return (hour < 0 || minute < 0) ? -1 : to_minutes(hour, minute);
}

} // anonymous namespace

// ============================================================
// Public API: two functions cover all trading decisions
// ============================================================

// True while the NYSE regular session is open (14:30–21:00 UTC, winter)
constexpr bool market_open(std::string_view timestamp) {
	auto current = timestamp_minutes(timestamp);
	return current >= 0
		&& current >= to_minutes(OPEN_HOUR, OPEN_MINUTE)
		&& current <  to_minutes(CLOSE_HOUR, CLOSE_MINUTE);
}

// True during the risk-off period: first hour after open and last 30 min.
// Usage in exit logic:
//   if (!market_open(ts)) return;   // market closed, nothing to do
//   if (risk_off(ts))    liquidate; // unsafe window, close all positions
//   else                 test_exits; // normal exit criteria
constexpr bool risk_off(std::string_view timestamp) {
	if (!market_open(timestamp))
		return false;

	auto current    = timestamp_minutes(timestamp);
	auto risk_start = to_minutes(OPEN_HOUR, OPEN_MINUTE)  + RISK_ON_DELAY_MINUTES;
	auto risk_end   = to_minutes(CLOSE_HOUR, CLOSE_MINUTE) - RISK_OFF_ADVANCE_MINUTES;

	// Risk-off: before safe window opens OR after safe window closes
	return current < risk_start || current >= risk_end;
}

// Unit tests
namespace {
	// market_open: standard session
	static_assert( market_open("2026-02-16T14:30:00Z"), "open at 14:30");
	static_assert( market_open("2026-02-16T15:00:00Z"), "open at 15:00");
	static_assert( market_open("2026-02-16T20:59:00Z"), "open at 20:59");
	static_assert(!market_open("2026-02-16T14:29:00Z"), "closed at 14:29");
	static_assert(!market_open("2026-02-16T21:00:00Z"), "closed at 21:00");
	static_assert(!market_open("2026-02-16T21:01:00Z"), "closed at 21:01");
	static_assert(!market_open("2026-02-16T13:00:00Z"), "closed pre-market");

	// risk_off: first hour (14:30–15:30) and last 30 min (20:30–21:00)
	// Safe (risk-on) window: 15:30–20:30 UTC
	static_assert( risk_off("2026-02-16T14:30:00Z"), "risk-off: market open, first hour");
	static_assert( risk_off("2026-02-16T15:29:00Z"), "risk-off: one minute before safe window");
	static_assert(!risk_off("2026-02-16T15:30:00Z"), "safe window starts at 15:30");
	static_assert(!risk_off("2026-02-16T18:00:00Z"), "safe: mid-day");
	static_assert(!risk_off("2026-02-16T20:29:00Z"), "safe: one minute before cutoff");
	static_assert( risk_off("2026-02-16T20:30:00Z"), "risk-off: last 30 minutes");
	static_assert( risk_off("2026-02-16T20:50:00Z"), "risk-off: near close");
	static_assert(!risk_off("2026-02-16T21:00:00Z"), "not risk-off: market closed");
	static_assert(!risk_off("2026-02-16T13:00:00Z"), "not risk-off: pre-market");
}

} // namespace market
