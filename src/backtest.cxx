// Backtest module - tests strategies against historical bar data
// Uses the same constexpr entry/exit code as live trading

#include "bar.h"
#include "entry.h"
#include "exit.h"
#include "json.h"
#include <algorithm>
#include <cmath>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <print>
#include <sstream>
#include <string>
#include <vector>

struct StrategyResult {
	std::string symbol;
	std::string strategy_name;
	double win_rate = 0.0;
	double avg_profit = 0.0;
	int trade_count = 0;
	double total_return = 0.0;
	int min_duration_bars = 0;
	int max_duration_bars = 0;
	std::string first_timestamp;
	std::string last_timestamp;
};

struct Trade {
	double entry_price;
	double exit_price;
	double profit_pct;
	bool win;
	int duration_bars;
};

// Simple runtime JSON bar parser (not constexpr like json.h)
std::vector<bar> load_bars(const std::filesystem::path& file_path) {
	auto ifs = std::ifstream{file_path};
	if (!ifs)
		return {};

	auto json_str = std::string{std::istreambuf_iterator<char>(ifs), {}};
	auto bars = std::vector<bar>{};

	// Find the "bars" array
	auto pos = json_str.find("\"bars\"");
	if (pos == std::string::npos)
		return {};

	auto array_start = json_str.find('[', pos);
	if (array_start == std::string::npos)
		return {};

	// Parse each bar object
	pos = array_start + 1;
	while (pos < json_str.size()) {
		// Find next object start
		auto obj_start = json_str.find('{', pos);
		if (obj_start == std::string::npos)
			break;

		auto obj_end = json_str.find('}', obj_start);
		if (obj_end == std::string::npos)
			break;

		// Extract bar fields
		auto b = bar{};
		auto obj = json_str.substr(obj_start, obj_end - obj_start + 1);

		// Simple field extraction (assumes specific order isn't required)
		auto extract_double = [&obj](std::string_view key) -> double {
			auto key_pos = obj.find(std::string{"\""}  + std::string{key} + "\"");
			if (key_pos == std::string::npos)
				return 0.0;
			auto colon = obj.find(':', key_pos);
			auto comma = obj.find_first_of(",}", colon);
			return std::stod(obj.substr(colon + 1, comma - colon - 1));
		};

		auto extract_int = [&obj](std::string_view key) -> std::uint32_t {
			auto key_pos = obj.find(std::string{"\""}  + std::string{key} + "\"");
			if (key_pos == std::string::npos)
				return 0;
			auto colon = obj.find(':', key_pos);
			auto comma = obj.find_first_of(",}", colon);
			return static_cast<std::uint32_t>(std::stoul(obj.substr(colon + 1, comma - colon - 1)));
		};

		auto extract_string = [&obj](std::string_view key) -> std::string {
			auto key_pos = obj.find(std::string{"\""}  + std::string{key} + "\"");
			if (key_pos == std::string::npos)
				return "";
			auto colon = obj.find(':', key_pos);
			auto quote1 = obj.find('"', colon);
			auto quote2 = obj.find('"', quote1 + 1);
			return obj.substr(quote1 + 1, quote2 - quote1 - 1);
		};

		b.close = extract_double("c");
		b.high = extract_double("h");
		b.low = extract_double("l");
		b.open = extract_double("o");
		b.vwap = extract_double("vw");
		b.volume = extract_int("v");
		b.num_trades = extract_int("n");

		auto ts = extract_string("t");
		// Store timestamp in a static buffer (simple approach for backtest)
		static std::vector<std::string> timestamp_storage;
		timestamp_storage.push_back(ts);
		b.timestamp = timestamp_storage.back();

		if (is_valid(b))
			bars.push_back(b);

		pos = obj_end + 1;
	}

	return bars;
}

// Check if current bar is at or near market close (4:00 PM ET = 20:00 UTC)
// Market close bars have timestamps like "2026-02-11T20:00:00Z" or later
bool is_market_close(std::string_view timestamp) {
	if (timestamp.size() < 14)
		return false;

	// Extract hour from "YYYY-MM-DDTHH:MM:SSZ" format (positions 11-12)
	auto hour_str = timestamp.substr(11, 2);
	auto hour = std::stoi(std::string{hour_str});

	// Market close is 20:00 UTC (4:00 PM ET) or later
	return hour >= 20;
}

// Backtest a specific strategy on bar data
template <typename EntryFunc>
StrategyResult backtest_strategy(std::span<const bar> bars, EntryFunc entry_func, std::string_view strategy_name) {
	auto result = StrategyResult{};
	result.strategy_name = std::string{strategy_name};

	// Capture data range
	if (!bars.empty()) {
		result.first_timestamp = std::string{bars.front().timestamp};
		result.last_timestamp = std::string{bars.back().timestamp};
	}

	auto trades = std::vector<Trade>{};
	auto position = std::optional<::position>{};
	auto entry_bar_index = 0uz;

	// Walk through bars simulating live trading
	for (auto i = 0uz; i < bars.size(); ++i) {
		auto history = std::span{bars.data(), i + 1};
		auto is_eod = is_market_close(bars[i].timestamp);

		// Check for exit when in position (including forced EOD close)
		if (position && (is_exit(*position, bars[i]) || is_eod)) {
			auto exit_price = bars[i].close;
			auto profit_pct = (exit_price - position->entry_price) / position->entry_price;
			auto duration = static_cast<int>(i - entry_bar_index);

			trades.push_back(Trade{
				.entry_price = position->entry_price,
				.exit_price = exit_price,
				.profit_pct = profit_pct,
				.win = profit_pct > 0.0,
				.duration_bars = duration
			});

			position.reset();
		}
		// Check for entry signal when not in position (but not at EOD)
		else if (!position && i >= 20 && !is_eod && entry_func(history)) {
			auto entry_price = bars[i].close;

			// Set position parameters (10% take profit, 5% stop loss)
			position = ::position{
				.entry_price = entry_price,
				.take_profit = entry_price * 1.10,
				.stop_loss = entry_price * 0.95,
				.trailing_stop = entry_price * 0.95
			};
			entry_bar_index = i;
		}
	}

	// Calculate metrics
	if (trades.empty()) {
		return result;
	}

	auto wins = 0uz;
	auto total_profit = 0.0;
	auto min_duration = std::numeric_limits<int>::max();
	auto max_duration = 0;

	for (const auto& trade : trades) {
		if (trade.win)
			wins++;
		total_profit += trade.profit_pct;
		min_duration = std::min(min_duration, trade.duration_bars);
		max_duration = std::max(max_duration, trade.duration_bars);
	}

	result.trade_count = static_cast<int>(trades.size());
	result.win_rate = static_cast<double>(wins) / trades.size();
	result.avg_profit = total_profit / trades.size();
	result.total_return = total_profit;
	result.min_duration_bars = min_duration;
	result.max_duration_bars = max_duration;

	return result;
}

std::string get_iso_timestamp() {
	auto now = std::chrono::system_clock::now();
	auto time = std::chrono::system_clock::to_time_t(now);
	auto ss = std::ostringstream{};
	ss << std::put_time(std::gmtime(&time), "%Y-%m-%dT%H:%M:%SZ");
	return ss.str();
}

int main() {
	std::println("Backtest Module - Testing strategies");
	std::println("");

	// Load candidates from filter output
	auto candidates_file = std::filesystem::path{"docs/candidates.json"};
	if (!std::filesystem::exists(candidates_file)) {
		std::println("Error: candidates.json not found");
		std::println("Run filter module first");
		return 1;
	}

	// Parse candidates.json to get symbol list
	auto ifs = std::ifstream{candidates_file};
	auto json_str = std::string{std::istreambuf_iterator<char>(ifs), {}};

	// Simple JSON parsing to extract symbols array
	auto candidates = std::vector<std::string>{};
	auto pos = json_str.find("\"symbols\"");
	if (pos != std::string::npos) {
		auto start = json_str.find('[', pos);
		auto end = json_str.find(']', start);
		auto symbols_str = json_str.substr(start + 1, end - start - 1);

		// Extract individual symbols
		auto current = 0uz;
		while (current < symbols_str.size()) {
			auto quote1 = symbols_str.find('"', current);
			if (quote1 == std::string::npos)
				break;
			auto quote2 = symbols_str.find('"', quote1 + 1);
			if (quote2 == std::string::npos)
				break;

			candidates.push_back(symbols_str.substr(quote1 + 1, quote2 - quote1 - 1));
			current = quote2 + 1;
		}
	}

	std::println("Testing {} candidates from filter", candidates.size());
	std::println("");

	auto all_results = std::vector<StrategyResult>{};

	// Test each candidate with all three strategies
	for (const auto& symbol : candidates) {
		auto bar_file = std::filesystem::path{"docs/bars"} / (symbol + ".json");

		if (!std::filesystem::exists(bar_file)) {
			std::println("✗ {} - bar data not found", symbol);
			continue;
		}

		auto bars = load_bars(bar_file);
		if (bars.size() < 100) {
			std::println("✗ {} - insufficient bars ({})", symbol, bars.size());
			continue;
		}

		// Test all three strategies
		auto results = std::vector<StrategyResult>{};

		results.push_back(backtest_strategy(bars, volume_surge_dip, "volume_surge"));
		results[0].symbol = symbol;

		results.push_back(backtest_strategy(bars, mean_reversion, "mean_reversion"));
		results[1].symbol = symbol;

		results.push_back(backtest_strategy(bars, sma_crossover<10, 20>, "sma_crossover"));
		results[2].symbol = symbol;

		// Debug output showing trade counts and win rates
		for (const auto& r : results) {
			if (r.trade_count > 0) {
				std::println("    {} - {}: {} trades, {:.1f}% win, {:.2f}% avg profit",
					symbol, r.strategy_name, r.trade_count,
					r.win_rate * 100.0, r.avg_profit * 100.0);
			}
		}

		// Find best strategy for this symbol
		auto best = std::max_element(results.begin(), results.end(),
			[](const auto& a, const auto& b) {
				// Prefer strategies with more trades and higher win rate
				if (a.trade_count == 0 && b.trade_count == 0)
					return false;
				if (a.trade_count == 0)
					return true;
				if (b.trade_count == 0)
					return false;

				// Score = win_rate * avg_profit * sqrt(trade_count)
				auto score_a = a.win_rate * a.avg_profit * std::sqrt(static_cast<double>(a.trade_count));
				auto score_b = b.win_rate * b.avg_profit * std::sqrt(static_cast<double>(b.trade_count));
				return score_a < score_b;
			});

		// Include if it meets minimum criteria
		// Accept: (1 trade with 100% win and >5% profit) OR (2+ trades with 40% win and >0.1% profit)
		bool passes = (best->trade_count >= 1 && best->win_rate == 1.0 && best->avg_profit >= 0.05) ||
		              (best->trade_count >= 2 && best->win_rate >= 0.40 && best->avg_profit >= 0.001);

		if (passes) {
			std::println("✓ {} - {} (win: {:.1f}%, profit: {:.2f}%, trades: {})",
				symbol, best->strategy_name,
				best->win_rate * 100.0,
				best->avg_profit * 100.0,
				best->trade_count);
			all_results.push_back(*best);
		} else {
			std::println("✗ {} - no profitable strategy found", symbol);
		}
	}

	std::println("");
	std::println("Profitable strategies: {}/{}", all_results.size(), candidates.size());

	// Sort by total return (best first)
	std::sort(all_results.begin(), all_results.end(),
		[](const auto& a, const auto& b) { return a.total_return > b.total_return; });

	// Write strategies.json
	auto output_file = std::filesystem::path{"docs/strategies.json"};
	auto ofs = std::ofstream{output_file};

	if (!ofs) {
		std::println("Error: Could not write {}", output_file.string());
		return 1;
	}

	ofs << "{\n";
	ofs << std::format("  \"timestamp\": \"{}\",\n", get_iso_timestamp());
	ofs << "  \"recommendations\": [\n";

	for (auto i = 0uz; i < all_results.size(); ++i) {
		const auto& rec = all_results[i];
		ofs << "    {\n";
		ofs << std::format("      \"symbol\": \"{}\",\n", rec.symbol);
		ofs << std::format("      \"strategy\": \"{}\",\n", rec.strategy_name);
		ofs << std::format("      \"win_rate\": {:.3f},\n", rec.win_rate);
		ofs << std::format("      \"avg_profit\": {:.4f},\n", rec.avg_profit);
		ofs << std::format("      \"trade_count\": {},\n", rec.trade_count);
		ofs << std::format("      \"min_duration_bars\": {},\n", rec.min_duration_bars);
		ofs << std::format("      \"max_duration_bars\": {},\n", rec.max_duration_bars);
		ofs << std::format("      \"first_timestamp\": \"{}\",\n", rec.first_timestamp);
		ofs << std::format("      \"last_timestamp\": \"{}\"\n", rec.last_timestamp);
		ofs << "    }";
		if (i < all_results.size() - 1)
			ofs << ",";
		ofs << "\n";
	}

	ofs << "  ]\n";
	ofs << "}\n";

	std::println("Wrote {}", output_file.string());

	return 0;
}
