#include "bar.h"
#include "exit.h"
#include "fix.h"
#include "json.h"
#include "market.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <print>
#include <string>
#include <vector>

// Position from account module
struct Position {
	std::string symbol;
	double qty;
	double avg_entry_price;
	std::string side;
};

// Parse positions.json from account module
std::vector<Position> load_positions() {
	auto ifs = std::ifstream{"docs/positions.json"};
	if (!ifs)
		return {};

	auto content = std::string{};
	auto line = std::string{};
	while (std::getline(ifs, line))
		content += line;

	auto positions = std::vector<Position>{};

	// Parse JSON array manually
	auto start = content.find('[');
	if (start == std::string::npos)
		return {};

	auto pos = start + 1;
	while (pos < content.size()) {
		auto obj_start = content.find('{', pos);
		if (obj_start == std::string::npos)
			break;

		auto obj_end = content.find('}', obj_start);
		if (obj_end == std::string::npos)
			break;

		auto obj = content.substr(obj_start, obj_end - obj_start + 1);

		// Extract fields
		Position p;

		// symbol
		auto symbol_key = obj.find("\"symbol\"");
		if (symbol_key != std::string::npos) {
			auto colon = obj.find(':', symbol_key);
			auto quote1 = obj.find('"', colon);
			auto quote2 = obj.find('"', quote1 + 1);
			p.symbol = obj.substr(quote1 + 1, quote2 - quote1 - 1);
		}

		// qty
		auto qty_key = obj.find("\"qty\"");
		if (qty_key != std::string::npos) {
			auto colon = obj.find(':', qty_key);
			auto comma = obj.find(',', colon);
			auto num_str = obj.substr(colon + 1, comma - colon - 1);
			p.qty = std::stod(num_str);
		}

		// avg_entry_price
		auto price_key = obj.find("\"avg_entry_price\"");
		if (price_key != std::string::npos) {
			auto colon = obj.find(':', price_key);
			auto comma = obj.find(',', colon);
			if (comma == std::string::npos)
				comma = obj.find('}', colon);
			auto num_str = obj.substr(colon + 1, comma - colon - 1);
			p.avg_entry_price = std::stod(num_str);
		}

		// side
		auto side_key = obj.find("\"side\"");
		if (side_key != std::string::npos) {
			auto colon = obj.find(':', side_key);
			auto quote1 = obj.find('"', colon);
			auto quote2 = obj.find('"', quote1 + 1);
			p.side = obj.substr(quote1 + 1, quote2 - quote1 - 1);
		}

		positions.push_back(p);
		pos = obj_end + 1;
	}

	return positions;
}

int main() {
	std::println("Low Frequency Trader v2 - Exit Module\n");

	// Load open positions
	auto positions = load_positions();

	if (positions.empty()) {
		std::println("No open positions to check");
		return 0;
	}

	std::println("Checking {} position(s) for exit signals...", positions.size());

	// Check if we need to liquidate everything (using latest bar timestamp)
	// We'll check per-position using the bar timestamp

	// Collect sell orders
	auto sell_orders = std::vector<std::string>{};
	auto seq_num = 1;

	for (const auto& pos : positions) {
		std::println("\n📊 Checking {} ({} shares @ ${:.2f})",
			pos.symbol, pos.qty, pos.avg_entry_price);

		// Load latest bars for this symbol
		auto bars = load_bars(pos.symbol);

		if (bars.empty()) {
			std::println("   ⚠️  No bar data available, skipping");
			continue;
		}

		auto latest_price = bars.back().close;
		auto profit_pct = ((latest_price - pos.avg_entry_price) / pos.avg_entry_price) * 100.0;

		std::println("   Current price: ${:.2f} ({:+.2f}%)", latest_price, profit_pct);

		// Check exit conditions
		auto should_exit = false;
		auto exit_reason = std::string{};

		// Force exit during risk-off period (last 30 min of trading day)
		if (market::risk_off(bars.back().timestamp)) {
			should_exit = true;
			exit_reason = "risk_off_liquidation";
			std::println("⚠️  Risk-off period - liquidating at {}", std::string{bars.back().timestamp});
		}
		// Check normal exit conditions using our exit logic
		else {
			// Create position with take profit (+10%), stop loss (-5%), trailing stop
			auto mock_position = position{
				.entry_price = pos.avg_entry_price,
				.take_profit = pos.avg_entry_price * 1.10,
				.stop_loss = pos.avg_entry_price * 0.95,
				.trailing_stop = pos.avg_entry_price * 0.95  // Start at stop loss level
			};

			// Check exit strategy
			if (is_exit(mock_position, bars.back())) {
				should_exit = true;

				// Determine which condition triggered
				if (profit_pct >= 10.0)
					exit_reason = "take_profit";
				else if (profit_pct <= -5.0)
					exit_reason = "stop_loss";
				else
					exit_reason = "trailing_stop";
			}
		}

		if (should_exit) {
			std::println("   ✅ Exit signal: {}", exit_reason);

			// Generate FIX sell order
			auto order_id = std::format("EXIT_{}_{}_{}", pos.symbol, seq_num,
				std::chrono::system_clock::now().time_since_epoch().count());

			auto msg = fix::message{fix::NEW_ORDER_SINGLE}
				.add(fix::CL_ORD_ID, order_id)
				.add(fix::HANDL_INST, "1")
				.add(fix::SYMBOL, pos.symbol)
				.add(fix::SIDE, fix::SIDE_SELL)
				.add(fix::ORDER_QTY, static_cast<int>(pos.qty))
				.add(fix::ORD_TYPE, fix::ORD_TYPE_MARKET)
				.add(fix::TIME_IN_FORCE, fix::TIME_IN_FORCE_DAY)
				.add(fix::TEXT, exit_reason);

			sell_orders.push_back(msg.build(seq_num));
			seq_num++;
		} else {
			std::println("   ⏭️  No exit signal - holding position");
		}
	}

	// Write sell.fix file
	if (!sell_orders.empty()) {
		auto ofs = std::ofstream{"docs/sell.fix"};
		for (const auto& order : sell_orders)
			ofs << order;

		std::println("\n✓ Generated {} sell order(s) in docs/sell.fix", sell_orders.size());
	} else {
		std::println("\n✓ No exit signals - all positions held");

		// Create empty file to indicate module ran
		auto ofs = std::ofstream{"docs/sell.fix"};
	}

	return 0;
}
