// Market evaluator
// Platform-agnostic strategy evaluation that reads market data and generates signals
// Run this after fetch to analyse market snapshots and identify entry/exit opportunities

#include "bar.h"
#include "entry.h"
#include "exit.h"
#include "json.h"
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
	auto ifs = std::ifstream{"docs/strategies.json"};
	if (!ifs) {
		std::println("Error: strategies.json not found");
		return {};
	}

	auto json_str    = std::string{std::istreambuf_iterator<char>(ifs), {}};
	auto candidates  = std::vector<Candidate>{};

	auto rec_pos = json_str.find("\"recommendations\"");
	if (rec_pos == std::string::npos)
		return {};

	auto pos = json_str.find('[', rec_pos) + 1;
	while (true) {
		auto obj_start = json_str.find('{', pos);
		if (obj_start == std::string::npos) break;
		auto obj_end = json_str.find('}', obj_start);
		if (obj_end == std::string::npos) break;

		auto obj = std::string_view{json_str}.substr(obj_start + 1, obj_end - obj_start - 1);
		auto c   = Candidate{std::string{json_string(obj, "symbol")},
		                     std::string{json_string(obj, "strategy")}};

		if (!c.symbol.empty() && !c.strategy.empty())
			candidates.push_back(c);

		pos = obj_end + 1;
	}

	return candidates;
}

int main() {
	std::println("Low Frequency Trader v2 - Market Evaluator\n");

	auto candidates = load_strategies();
	if (candidates.empty()) {
		std::println("Error: No candidates in strategies.json");
		return 1;
	}

	std::println("Loaded {} candidates from strategies.json", candidates.size());

	auto signals = std::vector<Signal>{};

	for (const auto& candidate : candidates) {
		auto bars = load_bars(candidate.symbol);

		if (bars.size() < 20) {
			std::println("Warning: Insufficient bars for {} (got {})", candidate.symbol, bars.size());
			continue;
		}

		auto should_enter = false;
		if (candidate.strategy == "volume_surge_dip")
			should_enter = volume_surge_dip(bars);
		else if (candidate.strategy == "mean_reversion")
			should_enter = mean_reversion(bars);
		else if (candidate.strategy == "sma_crossover")
			should_enter = sma_crossover(bars);

		if (should_enter) {
			signals.push_back({
				.symbol    = candidate.symbol,
				.strategy  = candidate.strategy,
				.action    = "entry",
				.price     = bars.back().close,
				.timestamp = std::string{bars.back().timestamp},
			});
			std::println("ENTRY SIGNAL: {} using {} @ ${:.2f}",
				candidate.symbol, candidate.strategy, bars.back().close);
		}
	}

	std::println("\nGenerated {} entry signals", signals.size());

	auto ofs = std::ofstream{"docs/signals.json"};
	if (!ofs) {
		std::println("Error: Could not write signals.json");
		return 1;
	}

	ofs << "{\n  \"signals\": [\n";
	for (auto i = 0uz; i < signals.size(); ++i) {
		const auto& sig = signals[i];
		ofs << std::format("    {{\"symbol\":\"{}\",\"strategy\":\"{}\",\"action\":\"{}\",\"price\":{:.2f},\"timestamp\":\"{}\"}}",
			sig.symbol, sig.strategy, sig.action, sig.price, sig.timestamp);
		ofs << (i < signals.size() - 1 ? ",\n" : "\n");
	}
	ofs << "  ]\n}\n";

	std::println("Wrote signals to docs/signals.json");
	return 0;
}
