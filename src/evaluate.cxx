// Market evaluator
// Platform-agnostic strategy evaluation that reads market data and generates signals
// Run this after fetch to analyse market snapshots and identify entry/exit opportunities

#include "bar.h"
#include "entry.h"
#include "exit.h"
#include "json.h"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <print>
#include <string>
#include <vector>

struct Candidate {
	std::string symbol;
	std::string strategy;
};

struct Signal {
	std::string symbol;
	std::string strategy;
	std::string action;  // "entry" or "exit"
	double price;
	std::string timestamp;
};

std::vector<Candidate> load_strategies() {
	auto strategies_path = std::filesystem::path{"docs/strategies.json"};
	auto ifs = std::ifstream{strategies_path};

	if (!ifs) {
		std::println("Error: strategies.json not found");
		return {};
	}

	auto json_str = std::string{std::istreambuf_iterator<char>(ifs), {}};
	auto candidates = std::vector<Candidate>{};

	// Parse recommendations array
	auto rec_pos = json_str.find("\"recommendations\"");
	if (rec_pos == std::string::npos)
		return {};

	auto array_start = json_str.find('[', rec_pos);
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

		// Extract symbol
		auto sym_key = obj.find("\"symbol\"");
		if (sym_key != std::string::npos) {
			auto colon = obj.find(':', sym_key);
			auto quote1 = obj.find('"', colon);
			auto quote2 = obj.find('"', quote1 + 1);
			auto symbol = obj.substr(quote1 + 1, quote2 - quote1 - 1);

			// Extract strategy
			auto strat_key = obj.find("\"strategy\"");
			if (strat_key != std::string::npos) {
				auto colon2 = obj.find(':', strat_key);
				auto quote3 = obj.find('"', colon2);
				auto quote4 = obj.find('"', quote3 + 1);
				auto strategy = obj.substr(quote3 + 1, quote4 - quote3 - 1);

				candidates.push_back(Candidate{std::string{symbol}, std::string{strategy}});
			}
		}

		pos = obj_end + 1;
	}

	return candidates;
}

std::vector<bar> fetch_latest_bars(std::string_view symbol, int count) {
	// Load from docs/bars/{symbol}.json
	auto bars_path = std::filesystem::path{"docs/bars"} / std::format("{}.json", symbol);
	auto ifs = std::ifstream{bars_path};

	if (!ifs) {
		std::println("Warning: No bar data for {}", symbol);
		return {};
	}

	auto json_str = std::string{std::istreambuf_iterator<char>(ifs), {}};
	auto bars = std::vector<bar>{};

	// Find bars array
	auto pos = json_str.find("\"bars\"");
	if (pos == std::string::npos)
		return {};

	auto array_start = json_str.find('[', pos);
	if (array_start == std::string::npos)
		return {};

	pos = array_start + 1;
	while (true) {
		auto obj_start = json_str.find('{', pos);
		if (obj_start == std::string::npos)
			break;

		auto obj_end = json_str.find('}', obj_start);
		if (obj_end == std::string::npos)
			break;

		auto obj = json_str.substr(obj_start, obj_end - obj_start);

		auto extract_double = [&obj](std::string_view key) {
			auto key_pos = obj.find(key);
			if (key_pos == std::string::npos)
				return 0.0;
			auto colon = obj.find(':', key_pos);
			auto comma = obj.find(',', colon);
			auto value_str = obj.substr(colon + 1, comma - colon - 1);
			return std::stod(std::string{value_str});
		};

		auto extract_int = [&obj](std::string_view key) {
			auto key_pos = obj.find(key);
			if (key_pos == std::string::npos)
				return 0u;
			auto colon = obj.find(':', key_pos);
			auto comma = obj.find(',', colon);
			auto value_str = obj.substr(colon + 1, comma - colon - 1);
			return static_cast<std::uint32_t>(std::stoul(std::string{value_str}));
		};

		auto extract_string = [&obj](std::string_view key) {
			auto key_pos = obj.find(key);
			if (key_pos == std::string::npos)
				return std::string{};
			auto colon = obj.find(':', key_pos);
			auto quote1 = obj.find('"', colon);
			auto quote2 = obj.find('"', quote1 + 1);
			return obj.substr(quote1 + 1, quote2 - quote1 - 1);
		};

		auto b = bar{};
		b.close = extract_double("\"c\"");
		b.high = extract_double("\"h\"");
		b.low = extract_double("\"l\"");
		b.open = extract_double("\"o\"");
		b.vwap = extract_double("\"vw\"");
		b.volume = extract_int("\"v\"");
		b.num_trades = extract_int("\"n\"");

		auto ts = extract_string("\"t\"");
		static std::vector<std::string> timestamp_storage;
		timestamp_storage.push_back(ts);
		b.timestamp = timestamp_storage.back();

		if (is_valid(b))
			bars.push_back(b);

		pos = obj_end + 1;
	}

	// Return last N bars
	if (bars.size() > static_cast<size_t>(count))
		return std::vector<bar>{bars.end() - count, bars.end()};

	return bars;
}

int main() {
	std::println("Low Frequency Trader v2 - Market Evaluator\n");

	// Load strategies
	auto candidates = load_strategies();
	if (candidates.empty()) {
		std::println("Error: No candidates in strategies.json");
		return 1;
	}

	std::println("Loaded {} candidates from strategies.json", candidates.size());

	// Check each candidate for entry signals
	auto signals = std::vector<Signal>{};

	for (const auto& candidate : candidates) {
		// Fetch latest 25 bars (need 20+ for strategy history)
		auto bars = fetch_latest_bars(candidate.symbol, 25);

		if (bars.size() < 20) {
			std::println("Warning: Insufficient bars for {} (got {})", candidate.symbol, bars.size());
			continue;
		}

		// Check entry signal using the recommended strategy
		auto should_enter = false;

		if (candidate.strategy == "volume_surge_dip")
			should_enter = volume_surge_dip(bars);
		else if (candidate.strategy == "mean_reversion")
			should_enter = mean_reversion(bars);
		else if (candidate.strategy == "sma_crossover")
			should_enter = sma_crossover(bars);

		if (should_enter) {
			signals.push_back(Signal{
				.symbol = candidate.symbol,
				.strategy = candidate.strategy,
				.action = "entry",
				.price = bars.back().close,
				.timestamp = std::string{bars.back().timestamp}
			});

			std::println("📈 ENTRY SIGNAL: {} using {} @ ${:.2f}",
				candidate.symbol, candidate.strategy, bars.back().close);
		}
	}

	std::println("\nGenerated {} entry signals", signals.size());

	// Write signals.json
	auto output_path = std::filesystem::path{"docs/signals.json"};
	auto ofs = std::ofstream{output_path};

	if (!ofs) {
		std::println("Error: Could not write signals.json");
		return 1;
	}

	ofs << "{\n";
	ofs << "  \"signals\": [\n";

	for (auto i = 0uz; i < signals.size(); ++i) {
		const auto& sig = signals[i];
		ofs << "    {\n";
		ofs << std::format("      \"symbol\": \"{}\",\n", sig.symbol);
		ofs << std::format("      \"strategy\": \"{}\",\n", sig.strategy);
		ofs << std::format("      \"action\": \"{}\",\n", sig.action);
		ofs << std::format("      \"price\": {:.2f},\n", sig.price);
		ofs << std::format("      \"timestamp\": \"{}\"\n", sig.timestamp);
		ofs << "    }";
		if (i < signals.size() - 1)
			ofs << ",";
		ofs << "\n";
	}

	ofs << "  ]\n";
	ofs << "}\n";

	std::println("Wrote signals to {}", output_path.string());

	return 0;
}
