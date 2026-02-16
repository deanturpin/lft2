#pragma once
#include <string_view>

// NYSE market hours and trading rules
// All times in UTC (NYSE is ET: UTC-5 in winter, UTC-4 in summer)
namespace market {

// Market hours (regular session)
// NYSE: 9:30 AM - 4:00 PM ET
// Winter (EST): 14:30 - 21:00 UTC
// Summer (EDT): 13:30 - 20:00 UTC
// Using winter hours (most conservative)
constexpr auto MARKET_OPEN_HOUR = 14;    // 14:30 UTC = 9:30 AM ET
constexpr auto MARKET_OPEN_MINUTE = 30;
constexpr auto MARKET_CLOSE_HOUR = 21;   // 21:00 UTC = 4:00 PM ET
constexpr auto MARKET_CLOSE_MINUTE = 0;

// Risk management times
constexpr auto RISK_ON_DELAY_MINUTES = 60;     // Wait 1 hour after market open
constexpr auto RISK_OFF_ADVANCE_MINUTES = 30;  // Stop 30 minutes before close
constexpr auto LIQUIDATE_ADVANCE_MINUTES = 10; // Force liquidate 10 minutes before close

// Extract hour and minute from ISO 8601 timestamp
// Format: "2026-02-16T14:30:00Z"
constexpr int extract_hour(std::string_view timestamp) {
	if (timestamp.size() < 14)
		return -1;

	// Hour is at positions 11-12
	auto hour_str = timestamp.substr(11, 2);
	auto hour = 0;
	for (auto c : hour_str) {
		if (c < '0' || c > '9')
			return -1;
		hour = hour * 10 + (c - '0');
	}
	return hour;
}

constexpr int extract_minute(std::string_view timestamp) {
	if (timestamp.size() < 17)
		return -1;

	// Minute is at positions 14-15
	auto min_str = timestamp.substr(14, 2);
	auto minute = 0;
	for (auto c : min_str) {
		if (c < '0' || c > '9')
			return -1;
		minute = minute * 10 + (c - '0');
	}
	return minute;
}

// Convert time to minutes since midnight UTC for comparison
constexpr int to_minutes_utc(int hour, int minute) {
	return hour * 60 + minute;
}

// Check if timestamp is at or after market close (4:00 PM ET = 21:00 UTC)
constexpr bool market_close(std::string_view timestamp) {
	auto hour = extract_hour(timestamp);
	auto minute = extract_minute(timestamp);

	if (hour < 0 || minute < 0)
		return false;

	auto current = to_minutes_utc(hour, minute);
	auto close = to_minutes_utc(MARKET_CLOSE_HOUR, MARKET_CLOSE_MINUTE);

	return current >= close;
}

// Check if we should force liquidate (10 minutes before close)
constexpr bool liquidation_time(std::string_view timestamp) {
	auto hour = extract_hour(timestamp);
	auto minute = extract_minute(timestamp);

	if (hour < 0 || minute < 0)
		return false;

	auto current = to_minutes_utc(hour, minute);
	auto liquidate = to_minutes_utc(MARKET_CLOSE_HOUR, MARKET_CLOSE_MINUTE) - LIQUIDATE_ADVANCE_MINUTES;

	return current >= liquidate;
}

// Check if market is open (between 9:30 AM and 4:00 PM ET)
constexpr bool market_open(std::string_view timestamp) {
	auto hour = extract_hour(timestamp);
	auto minute = extract_minute(timestamp);

	if (hour < 0 || minute < 0)
		return false;

	auto current = to_minutes_utc(hour, minute);
	auto open = to_minutes_utc(MARKET_OPEN_HOUR, MARKET_OPEN_MINUTE);
	auto close = to_minutes_utc(MARKET_CLOSE_HOUR, MARKET_CLOSE_MINUTE);

	return current >= open && current < close;
}

// Check if we should take new positions (risk-on period)
// Only between 1 hour after open and 30 minutes before close
constexpr bool risk_on(std::string_view timestamp) {
	auto hour = extract_hour(timestamp);
	auto minute = extract_minute(timestamp);

	if (hour < 0 || minute < 0)
		return false;

	auto current = to_minutes_utc(hour, minute);
	auto risk_on_start = to_minutes_utc(MARKET_OPEN_HOUR, MARKET_OPEN_MINUTE) + RISK_ON_DELAY_MINUTES;
	auto risk_off_start = to_minutes_utc(MARKET_CLOSE_HOUR, MARKET_CLOSE_MINUTE) - RISK_OFF_ADVANCE_MINUTES;

	return current >= risk_on_start && current < risk_off_start;
}

// Check if we're in risk-off period (last 30 minutes of trading day)
// During this time: liquidate positions, block new entries
constexpr bool risk_off(std::string_view timestamp) {
	if (!market_open(timestamp))
		return false;  // Market closed, not risk-off period

	return !risk_on(timestamp);  // Market open but outside safe window
}

// Unit tests
namespace {
	// Test: market open time
	static_assert(market_open("2026-02-16T14:30:00Z"), "Market should be open at 14:30 UTC");
	static_assert(market_open("2026-02-16T15:00:00Z"), "Market should be open at 15:00 UTC");
	static_assert(market_open("2026-02-16T20:59:00Z"), "Market should be open at 20:59 UTC");

	// Test: market closed
	static_assert(!market_open("2026-02-16T14:29:00Z"), "Market should be closed at 14:29 UTC");
	static_assert(!market_open("2026-02-16T21:00:00Z"), "Market should be closed at 21:00 UTC");
	static_assert(!market_open("2026-02-16T21:01:00Z"), "Market should be closed at 21:01 UTC");

	// Test: market close detection
	static_assert(market_close("2026-02-16T21:00:00Z"), "Should detect close at 21:00 UTC");
	static_assert(market_close("2026-02-16T21:30:00Z"), "Should detect close after 21:00 UTC");
	static_assert(!market_close("2026-02-16T20:59:00Z"), "Should not detect close before 21:00 UTC");

	// Test: liquidation time (10 minutes before close = 20:50 UTC)
	static_assert(liquidation_time("2026-02-16T20:50:00Z"), "Should liquidate at 20:50 UTC");
	static_assert(liquidation_time("2026-02-16T20:55:00Z"), "Should liquidate at 20:55 UTC");
	static_assert(!liquidation_time("2026-02-16T20:49:00Z"), "Should not liquidate before 20:50 UTC");

	// Test: risk-on period (1 hour after open to 30 min before close)
	// Risk on: 15:30 UTC (1 hour after 14:30) to 20:30 UTC (30 min before 21:00)
	static_assert(!risk_on("2026-02-16T14:30:00Z"), "Risk off at market open");
	static_assert(!risk_on("2026-02-16T15:29:00Z"), "Risk off before 1 hour");
	static_assert(risk_on("2026-02-16T15:30:00Z"), "Risk on at 15:30 UTC");
	static_assert(risk_on("2026-02-16T18:00:00Z"), "Risk on during mid-day");
	static_assert(risk_on("2026-02-16T20:29:00Z"), "Risk on before cutoff");
	static_assert(!risk_on("2026-02-16T20:30:00Z"), "Risk off at 30 min before close");
	static_assert(!risk_on("2026-02-16T20:45:00Z"), "Risk off near close");

	// Test: risk-off period (market open but outside safe window)
	static_assert(risk_off("2026-02-16T14:30:00Z"), "Risk off: first hour after open");
	static_assert(risk_off("2026-02-16T15:29:00Z"), "Risk off: before risk-on starts");
	static_assert(!risk_off("2026-02-16T15:30:00Z"), "Not risk off during safe window");
	static_assert(!risk_off("2026-02-16T18:00:00Z"), "Not risk off during mid-day");
	static_assert(risk_off("2026-02-16T20:30:00Z"), "Risk off: last 30 minutes");
	static_assert(risk_off("2026-02-16T20:50:00Z"), "Risk off: near close");
	static_assert(!risk_off("2026-02-16T21:00:00Z"), "Not risk off: market closed");
	static_assert(!risk_off("2026-02-16T13:00:00Z"), "Not risk off: market closed");
}

} // namespace market
