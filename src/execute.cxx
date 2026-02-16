// Trade executor
// Alpaca-specific order execution that reads signals and places trades
// Run this after evaluate to execute the generated trading signals

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <print>
#include <string>
#include <string_view>
#include <vector>

struct Signal {
	std::string symbol;
	std::string strategy;
	std::string action;
	double price;
	std::string timestamp;
};

struct AccountInfo {
	double buying_power;
	double cash;
	double portfolio_value;
};

std::vector<Signal> load_signals() {
	auto signals_path = std::filesystem::path{"docs/signals.json"};
	auto ifs = std::ifstream{signals_path};

	if (!ifs) {
		std::println("No signals.json found");
		return {};
	}

	auto json_str = std::string{std::istreambuf_iterator<char>(ifs), {}};
	auto signals = std::vector<Signal>{};

	// Parse signals array
	auto sig_pos = json_str.find("\"signals\"");
	if (sig_pos == std::string::npos)
		return {};

	auto array_start = json_str.find('[', sig_pos);
	if (array_start == std::string::npos)
		return {};

	auto pos = array_start + 1;
	while (true) {
		auto obj_start = json_str.find('{', pos);
		if (obj_start == std::string::npos)
			break;

		auto obj_end = json_str.find('}', obj_start);
		if (obj_end == std::string::npos)
			break;

		auto obj = json_str.substr(obj_start, obj_end - obj_start);

		// Extract fields
		auto extract_string = [&obj](std::string_view key) {
			auto key_pos = obj.find(key);
			if (key_pos == std::string::npos)
				return std::string{};
			auto colon = obj.find(':', key_pos);
			auto quote1 = obj.find('"', colon);
			auto quote2 = obj.find('"', quote1 + 1);
			return obj.substr(quote1 + 1, quote2 - quote1 - 1);
		};

		auto extract_double = [&obj](std::string_view key) {
			auto key_pos = obj.find(key);
			if (key_pos == std::string::npos)
				return 0.0;
			auto colon = obj.find(':', key_pos);
			auto comma = obj.find(',', colon);
			if (comma == std::string::npos)
				comma = obj.find('\n', colon);
			auto value_str = obj.substr(colon + 1, comma - colon - 1);
			return std::stod(std::string{value_str});
		};

		signals.push_back(Signal{
			.symbol = extract_string("\"symbol\""),
			.strategy = extract_string("\"strategy\""),
			.action = extract_string("\"action\""),
			.price = extract_double("\"price\""),
			.timestamp = extract_string("\"timestamp\"")
		});

		pos = obj_end + 1;
	}

	return signals;
}

AccountInfo fetch_account_info([[maybe_unused]] std::string_view api_key,
                                 [[maybe_unused]] std::string_view api_secret) {
	// In a real implementation, this would call Alpaca API
	// For now, return placeholder values
	// Example: curl -X GET "https://paper-api.alpaca.markets/v2/account"
	//          with headers APCA-API-KEY-ID and APCA-API-SECRET-KEY

	std::println("Fetching account information from Alpaca...");
	std::println("(Using paper trading account)");

	return AccountInfo{
		.buying_power = 100000.0,
		.cash = 100000.0,
		.portfolio_value = 100000.0
	};
}

int main() {
	using namespace std::string_view_literals;

	std::println("Low Frequency Trader v2 - Trade Executor\n");

	// Load credentials
	auto api_key = std::getenv("ALPACA_API_KEY");
	auto api_secret = std::getenv("ALPACA_API_SECRET");

	if (!api_key || !api_secret) {
		std::println("Error: ALPACA_API_KEY and ALPACA_API_SECRET must be set");
		std::println("Create a .env file with your credentials and source it");
		return 1;
	}

	std::println("Credentials loaded successfully");
	std::println("API Key: {}***", std::string_view{api_key}.substr(0, 8));

	// Fetch account balance
	auto account = fetch_account_info(api_key, api_secret);
	std::println("\nAccount Balance:");
	std::println("  Cash:            ${:.2f}", account.cash);
	std::println("  Buying Power:    ${:.2f}", account.buying_power);
	std::println("  Portfolio Value: ${:.2f}", account.portfolio_value);

	// Calculate position size (2% of portfolio value)
	auto position_size = account.portfolio_value * 0.02;
	std::println("\nPosition Size (2% of portfolio): ${:.2f}", position_size);

	// Load signals
	auto signals = load_signals();
	if (signals.empty()) {
		std::println("\nNo signals to execute");
		return 0;
	}

	std::println("\nFound {} signal(s) to execute:", signals.size());

	// Execute each signal
	for (const auto& signal : signals) {
		std::println("\n📋 Signal: {} {} @ ${:.2f}",
			signal.symbol, signal.action, signal.price);

		if (signal.action != "entry") {
			std::println("   ⏭️  Skipping non-entry signal");
			continue;
		}

		// Calculate shares to buy
		auto shares = static_cast<int>(position_size / signal.price);

		if (shares < 1) {
			std::println("   ❌ Position size too small (${:.2f} / ${:.2f} = {} shares)",
				position_size, signal.price, shares);
			continue;
		}

		auto order_value = shares * signal.price;

		std::println("   Strategy: {}", signal.strategy);
		std::println("   Shares:   {} (${:.2f} total)", shares, order_value);

		if (order_value > account.cash) {
			std::println("   ❌ Insufficient cash (need ${:.2f}, have ${:.2f})",
				order_value, account.cash);
			continue;
		}

		// Set position parameters (matching backtest logic)
		auto take_profit = signal.price * 1.10;  // +10%
		auto stop_loss = signal.price * 0.95;    // -5%

		std::println("   Take Profit: ${:.2f} (+10%)", take_profit);
		std::println("   Stop Loss:   ${:.2f} (-5%)", stop_loss);

		// In production, this would place the order via Alpaca API:
		// POST /v2/orders
		// {
		//   "symbol": signal.symbol,
		//   "qty": shares,
		//   "side": "buy",
		//   "type": "limit",
		//   "time_in_force": "day",
		//   "limit_price": signal.price,
		//   "order_class": "bracket",
		//   "take_profit": { "limit_price": take_profit },
		//   "stop_loss": { "stop_price": stop_loss }
		// }

		std::println("   ✅ Order ready (not executed - dry run mode)");

		// Deduct from available cash for next signal
		account.cash -= order_value;
	}

	std::println("\n✓ Execution complete (dry run - no actual orders placed)");
	std::println("Remaining cash: ${:.2f}", account.cash);

	return 0;
}
