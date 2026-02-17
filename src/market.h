#pragma once
#include <chrono>
#include <string_view>

// NYSE market hours and trading rules.
//
// Bar timestamps from Alpaca are always UTC (ISO 8601, "Z" suffix), so we
// compare against UTC thresholds regardless of where this code runs.
// This makes the logic correct whether running locally, on CI, or on any
// cloud region.
//
// NYSE: 9:30 AM – 4:00 PM ET
// Winter (EST = UTC-5): 14:30 – 21:00 UTC  ← we use these (most conservative)
// Summer (EDT = UTC-4): 13:30 – 20:00 UTC
namespace market {

namespace {

using namespace std::chrono_literals;
using std::chrono::minutes;

// Session boundaries in UTC (winter / EST hours)
constexpr auto SESSION_OPEN  = 14h + 30min;
constexpr auto SESSION_CLOSE = 21h;

// Risk window offsets
constexpr auto RISK_ON_DELAY  = 60min;  // Skip the volatile first hour
constexpr auto RISK_OFF_START = 30min;  // Stop 30 minutes before close

// Parse minutes-since-midnight from a UTC ISO 8601 timestamp.
// Format: "2026-02-16T14:30:00Z"  (hour at [11..12], minute at [14..15])
// The "Z" suffix confirms UTC - no timezone conversion required.
constexpr minutes timestamp_minutes(std::string_view ts) {
	if (ts.size() < 17)
		return -1min;

	auto parse2 = [](std::string_view s) -> int {
		if (s[0] < '0' || s[0] > '9' || s[1] < '0' || s[1] > '9')
			return -1;
		return (s[0] - '0') * 10 + (s[1] - '0');
	};

	auto h = parse2(ts.substr(11, 2));
	auto m = parse2(ts.substr(14, 2));
	return (h < 0 || m < 0) ? -1min : minutes{h * 60 + m};
}

} // anonymous namespace

// ============================================================
// Public API: two functions, one decision tree
// ============================================================

// True while the NYSE regular session is open (14:30–21:00 UTC, winter).
constexpr bool market_open(std::string_view timestamp) {
	auto t = timestamp_minutes(timestamp);
	return t >= SESSION_OPEN && t < SESSION_CLOSE;
}

// True during the risk-off period: first hour after open and last 30 min.
//
// Intended usage:
//   if (!market_open(ts)) return;    // market closed - nothing to do
//   if (risk_off(ts))     liquidate; // unsafe window - close all positions
//   else                  check_exits_and_entries;
constexpr bool risk_off(std::string_view timestamp) {
	if (!market_open(timestamp))
		return false;

	auto t          = timestamp_minutes(timestamp);
	auto risk_start = SESSION_OPEN  + RISK_ON_DELAY;   // 15:30 UTC
	auto risk_end   = SESSION_CLOSE - RISK_OFF_START;  // 20:30 UTC

	return t < risk_start || t >= risk_end;
}

// Unit tests (compile-time, no runtime cost)
namespace {
	// market_open
	static_assert( market_open("2026-02-16T14:30:00Z"), "open at 14:30 UTC");
	static_assert( market_open("2026-02-16T15:00:00Z"), "open at 15:00 UTC");
	static_assert( market_open("2026-02-16T20:59:00Z"), "open at 20:59 UTC");
	static_assert(!market_open("2026-02-16T14:29:00Z"), "closed at 14:29 UTC");
	static_assert(!market_open("2026-02-16T21:00:00Z"), "closed at 21:00 UTC");
	static_assert(!market_open("2026-02-16T13:00:00Z"), "closed pre-market");

	// risk_off: unsafe windows are 14:30–15:29 and 20:30–20:59 UTC
	// Safe (risk-on) window: 15:30–20:29 UTC
	static_assert( risk_off("2026-02-16T14:30:00Z"), "risk-off: first hour");
	static_assert( risk_off("2026-02-16T15:29:00Z"), "risk-off: one minute before safe window");
	static_assert(!risk_off("2026-02-16T15:30:00Z"), "safe window starts at 15:30 UTC");
	static_assert(!risk_off("2026-02-16T18:00:00Z"), "safe: mid-day");
	static_assert(!risk_off("2026-02-16T20:29:00Z"), "safe: one minute before cutoff");
	static_assert( risk_off("2026-02-16T20:30:00Z"), "risk-off: last 30 minutes");
	static_assert( risk_off("2026-02-16T20:50:00Z"), "risk-off: near close");
	static_assert(!risk_off("2026-02-16T21:00:00Z"), "not risk-off: market closed");
	static_assert(!risk_off("2026-02-16T13:00:00Z"), "not risk-off: pre-market");
}

} // namespace market
