#include "bar.h"
#include "entry.h"
#include "fix.h"
#include "json.h"
#include "market.h"
#include <chrono>
#include <format>
#include <fstream>
#include <iostream>
#include <print>
#include <string>
#include <thread>
#include <vector>

// Candidate from strategies.json
struct Candidate {
	std::string symbol;
	std::string strategy;
};

// Account info
struct AccountInfo {
	double cash;
	double portfolio_value;
};

// Load recommended candidates from strategies.json
std::vector<Candidate> load_candidates() {
	auto ifs = std::ifstream{"docs/strategies.json"};
	if (!ifs)
		return {};

	auto content = std::string{};
	auto line = std::string{};
	while (std::getline(ifs, line))
		content += line;

	auto candidates = std::vector<Candidate>{};

	// Find recommendations array
	auto rec_start = content.find("\"recommendations\"");
	if (rec_start == std::string::npos)
		return {};

	auto array_start = content.find('[', rec_start);
	if (array_start == std::string::npos)
		return {};

	auto pos = array_start + 1;
	while (pos < content.size()) {
		auto obj_start = content.find('{', pos);
		if (obj_start == std::string::npos)
			break;

		auto obj_end = content.find('}', obj_start);
		if (obj_end == std::string::npos)
			break;

		auto obj = content.substr(obj_start, obj_end - obj_start + 1);

		Candidate c;

		// symbol
		auto symbol_key = obj.find("\"symbol\"");
		if (symbol_key != std::string::npos) {
			auto colon = obj.find(':', symbol_key);
			auto quote1 = obj.find('"', colon);
			auto quote2 = obj.find('"', quote1 + 1);
			c.symbol = obj.substr(quote1 + 1, quote2 - quote1 - 1);
		}

		// recommended_strategy
		auto strat_key = obj.find("\"recommended_strategy\"");
		if (strat_key != std::string::npos) {
			auto colon = obj.find(':', strat_key);
			auto quote1 = obj.find('"', colon);
			auto quote2 = obj.find('"', quote1 + 1);
			c.strategy = obj.substr(quote1 + 1, quote2 - quote1 - 1);
		}

		if (!c.symbol.empty() && !c.strategy.empty())
			candidates.push_back(c);

		pos = obj_end + 1;
	}

	return candidates;
}

// Load account balance
AccountInfo load_account_info() {
	auto ifs = std::ifstream{"docs/account.json"};
	if (!ifs)
		return {0.0, 0.0};

	auto content = std::string{};
	auto line = std::string{};
	while (std::getline(ifs, line))
		content += line;

	AccountInfo info;

	// cash
	auto cash_key = content.find("\"cash\"");
	if (cash_key != std::string::npos) {
		auto colon = content.find(':', cash_key);
		auto comma = content.find(',', colon);
		auto num_str = content.substr(colon + 1, comma - colon - 1);
		info.cash = std::stod(num_str);
	}

	// portfolio_value
	auto pv_key = content.find("\"portfolio_value\"");
	if (pv_key != std::string::npos) {
		auto colon = content.find(':', pv_key);
		auto comma = content.find(',', colon);
		if (comma == std::string::npos)
			comma = content.find('}', colon);
		auto num_str = content.substr(colon + 1, comma - colon - 1);
		info.portfolio_value = std::stod(num_str);
	}

	return info;
}

// Load existing positions to avoid duplicates
std::vector<std::string> load_existing_symbols() {
	auto ifs = std::ifstream{"docs/positions.json"};
	if (!ifs)
		return {};

	auto content = std::string{};
	auto line = std::string{};
	while (std::getline(ifs, line))
		content += line;

	auto symbols = std::vector<std::string>{};

	auto pos = 0uz;
	while (pos < content.size()) {
		auto symbol_key = content.find("\"symbol\"", pos);
		if (symbol_key == std::string::npos)
			break;

		auto colon = content.find(':', symbol_key);
		auto quote1 = content.find('"', colon);
		auto quote2 = content.find('"', quote1 + 1);
		symbols.push_back(content.substr(quote1 + 1, quote2 - quote1 - 1));

		pos = quote2 + 1;
	}

	return symbols;
}

// Load latest bars for a symbol
std::vector<bar> load_bars(std::string_view symbol) {
	auto path = std::format("docs/bars/{}.json", symbol);
	auto ifs = std::ifstream{path};
	if (!ifs)
		return {};

	auto json_str = std::string{std::istreambuf_iterator<char>(ifs), {}};
	auto bars = std::vector<bar>{};

	// Find the "bars" array
	auto pos = json_str.find("\"bars\"");
	if (pos == std::string::npos)
		return {};

	// Extract bars array manually
	auto array_start = json_str.find('[', pos);
	auto array_end = json_str.find(']', array_start);
	if (array_start == std::string::npos || array_end == std::string::npos)
		return {};

	// Static storage for timestamps
	static std::vector<std::string> timestamp_storage;

	auto obj_pos = array_start + 1;
	while (obj_pos < array_end) {
		auto obj_start = json_str.find('{', obj_pos);
		if (obj_start >= array_end)
			break;

		auto obj_end = json_str.find('}', obj_start);
		auto obj = json_str.substr(obj_start, obj_end - obj_start + 1);

		// Parse bar fields
		auto b = bar{};

		auto extract_double = [&](std::string_view key) {
			auto key_pos = obj.find(std::format("\"{}\"", key));
			if (key_pos == std::string::npos)
				return 0.0;
			auto colon = obj.find(':', key_pos);
			auto comma = obj.find(',', colon);
			if (comma == std::string::npos)
				comma = obj.find('}', colon);
			auto num_str = obj.substr(colon + 1, comma - colon - 1);
			return std::stod(std::string{num_str});
		};

		auto extract_int = [&](std::string_view key) {
			auto key_pos = obj.find(std::format("\"{}\"", key));
			if (key_pos == std::string::npos)
				return 0;
			auto colon = obj.find(':', key_pos);
			auto comma = obj.find(',', colon);
			if (comma == std::string::npos)
				comma = obj.find('}', colon);
			auto num_str = obj.substr(colon + 1, comma - colon - 1);
			return std::stoi(std::string{num_str});
		};

		auto extract_string = [&](std::string_view key) {
			auto key_pos = obj.find(std::format("\"{}\"", key));
			if (key_pos == std::string::npos)
				return std::string{};
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
		timestamp_storage.push_back(ts);
		b.timestamp = timestamp_storage.back();

		if (is_valid(b))
			bars.push_back(b);

		obj_pos = obj_end + 1;
	}

	return bars;
}

// Sleep until next whole 5-minute mark
void sleep_until_next_interval() {
	auto now = std::chrono::system_clock::now();
	auto tt = std::chrono::system_clock::to_time_t(now);
	auto tm = *std::gmtime(&tt);

	// Calculate next 5-minute boundary
	auto current_min = tm.tm_min;
	auto current_sec = tm.tm_sec;
	auto next_interval = ((current_min / 5) + 1) * 5;
	auto wait_min = next_interval - current_min;
	auto wait_sec = 60 - current_sec;  // Wait until start of next minute

	if (wait_min > 0) {
		std::println("⏰ Waiting {}:{:02d} until next 5-minute interval...", wait_min - 1, wait_sec);
		auto wait_time = std::chrono::minutes(wait_min) - std::chrono::seconds(current_sec);
		std::this_thread::sleep_until(now + wait_time);
	}
}

// NOTE: Removed is_market_close - now using market::is_risk_on() from market.h

int main() {
	std::println("Low Frequency Trader v2 - Entry Module\n");

	// NOTE: Risk-on check moved to per-candidate evaluation using bar timestamps

	// Wait until next 5-minute interval for fresh data
	sleep_until_next_interval();

	// Load candidates
	auto candidates = load_candidates();
	if (candidates.empty()) {
		std::println("No candidates to evaluate");
		auto ofs = std::ofstream{"docs/buy.fix"};
		return 0;
	}

	std::println("Evaluating {} candidate(s)...", candidates.size());

	// Load account info
	auto account = load_account_info();
	std::println("\nAccount Balance:");
	std::println("  Cash: ${:.2f}", account.cash);
	std::println("  Portfolio Value: ${:.2f}", account.portfolio_value);

	// Calculate position size (2% of portfolio)
	auto position_size = account.portfolio_value * 0.02;
	std::println("  Position Size (2%): ${:.2f}", position_size);

	// Load existing positions to avoid duplicates
	auto existing_symbols = load_existing_symbols();
	std::println("\nCurrently holding {} position(s)", existing_symbols.size());

	// Collect buy orders
	auto buy_orders = std::vector<std::string>{};
	auto seq_num = 1;

	for (const auto& candidate : candidates) {
		std::println("\n📊 Evaluating {}", candidate.symbol);
		std::println("   Strategy: {}", candidate.strategy);

		// Skip if already holding
		if (std::ranges::find(existing_symbols, candidate.symbol) != existing_symbols.end()) {
			std::println("   ⏭️  Already holding position");
			continue;
		}

		// Load bars
		auto bars = load_bars(candidate.symbol);
		if (bars.size() < 25) {
			std::println("   ⚠️  Insufficient data ({} bars)", bars.size());
			continue;
		}

		auto latest_price = bars.back().close;
		std::println("   Current price: ${:.2f}", latest_price);

		// Check if we're in risk-on period (1 hour after open, 30 min before close)
		if (!market::is_risk_on(bars.back().timestamp)) {
			std::println("   ⏭️  Risk-off period (too close to open/close)");
			continue;
		}

		// Check entry signal using recommended strategy
		auto should_enter = false;

		if (candidate.strategy == "volume_surge_dip")
			should_enter = volume_surge_dip(bars);
		else if (candidate.strategy == "mean_reversion")
			should_enter = mean_reversion(bars);
		else if (candidate.strategy == "sma_crossover")
			should_enter = sma_crossover(bars);
		else {
			std::println("   ⚠️  Unknown strategy: {}", candidate.strategy);
			continue;
		}

		if (!should_enter) {
			std::println("   ⏭️  No entry signal");
			continue;
		}

		// Calculate shares
		auto shares = static_cast<int>(position_size / latest_price);
		if (shares < 1) {
			std::println("   ❌ Position too small (${:.2f} / ${:.2f} = {} shares)",
				position_size, latest_price, shares);
			continue;
		}

		auto order_value = shares * latest_price;
		if (order_value > account.cash) {
			std::println("   ❌ Insufficient cash (need ${:.2f}, have ${:.2f})",
				order_value, account.cash);
			continue;
		}

		std::println("   ✅ Entry signal! Buying {} shares (${:.2f})",
			shares, order_value);

		// Generate FIX buy order
		auto order_id = std::format("ENTRY_{}_{}_{}", candidate.symbol, seq_num,
			std::chrono::system_clock::now().time_since_epoch().count());

		auto msg = fix::message{fix::NEW_ORDER_SINGLE}
			.add(fix::CL_ORD_ID, order_id)
			.add(fix::HANDL_INST, "1")
			.add(fix::SYMBOL, candidate.symbol)
			.add(fix::SIDE, fix::SIDE_BUY)
			.add(fix::ORDER_QTY, shares)
			.add(fix::ORD_TYPE, fix::ORD_TYPE_MARKET)
			.add(fix::TIME_IN_FORCE, fix::TIME_IN_FORCE_DAY)
			.add(fix::TEXT, candidate.strategy);

		buy_orders.push_back(msg.build(seq_num));
		seq_num++;

		// Deduct from available cash
		account.cash -= order_value;
	}

	// Write buy.fix file
	if (!buy_orders.empty()) {
		auto ofs = std::ofstream{"docs/buy.fix"};
		for (const auto& order : buy_orders)
			ofs << order;

		std::println("\n✓ Generated {} buy order(s) in docs/buy.fix", buy_orders.size());
	} else {
		std::println("\n✓ No entry signals");

		// Create empty file to indicate module ran
		auto ofs = std::ofstream{"docs/buy.fix"};
	}

	std::println("Remaining cash: ${:.2f}", account.cash);
	return 0;
}
