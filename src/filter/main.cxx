// Filter module - identifies candidate stocks from bar data
// Reads JSON bar files and applies filtering criteria

#include <filesystem>
#include <fstream>
#include <print>
#include <ranges>
#include <string>
#include <vector>

// Simple JSON generation (avoiding dependencies)
namespace json {
	auto quote(std::string_view s) { return std::format("\"{}\"", s); }

	auto array(const auto& items) {
		auto quoted = items | std::views::transform([](auto& s) { return quote(s); });
		return std::format("[{}]", std::views::join_with(quoted, ", "sv));
	}
}

struct FilterCriteria {
	double min_avg_volume = 1'000'000.0;
	double min_price = 10.0;
	double max_price = 500.0;
	double min_volatility = 0.01; // 1% daily range minimum
};

struct BarStats {
	std::string symbol;
	double avg_volume = 0.0;
	double avg_price = 0.0;
	double avg_range = 0.0; // (high - low) / close
	size_t bar_count = 0;
};

// Parse simple JSON manually (avoiding nlohmann::json dependency)
auto extract_symbol(std::string_view json) -> std::string {
	auto pos = json.find("\"symbol\":");
	if (pos == std::string_view::npos)
		return {};

	auto start = json.find('"', pos + 9);
	auto end = json.find('"', start + 1);
	return std::string{json.substr(start + 1, end - start - 1)};
}

auto count_bars(std::string_view json) -> size_t {
	auto pos = json.find("\"bars\":");
	if (pos == std::string_view::npos)
		return 0uz;

	auto count = 0uz;
	auto search_pos = pos;
	while ((search_pos = json.find("{\"t\":", search_pos + 1)) != std::string_view::npos)
		++count;

	return count;
}

auto calculate_stats(const std::filesystem::path& bar_file) -> BarStats {
	auto ifs = std::ifstream{bar_file};
	if (!ifs)
		return {};

	auto content = std::string{std::istreambuf_iterator<char>{ifs}, {}};

	auto stats = BarStats{};
	stats.symbol = extract_symbol(content);
	stats.bar_count = count_bars(content);

	// For now, return basic stats
	// TODO: Parse full JSON and calculate volume/volatility
	// This is a simplified version for demonstration

	return stats;
}

auto passes_filter(const BarStats& stats, const FilterCriteria& criteria) -> bool {
	// Minimum bar count check (ensure sufficient data)
	if (stats.bar_count < 100uz)
		return false;

	// Additional criteria would go here once we parse full JSON
	// For now, just check we have enough bars

	return true;
}

auto main() -> int {
	std::println("Filter Module - Identifying candidate stocks");
	std::println("");

	auto bars_dir = std::filesystem::path{"docs/bars"};

	if (!std::filesystem::exists(bars_dir)) {
		std::println("Error: bars directory not found: {}", bars_dir.string());
		return 1;
	}

	auto criteria = FilterCriteria{};
	auto candidates = std::vector<std::string>{};

	// Process all JSON files
	for (const auto& entry : std::filesystem::directory_iterator{bars_dir}) {
		if (entry.path().extension() != ".json")
			continue;

		auto stats = calculate_stats(entry.path());

		if (stats.symbol.empty()) {
			std::println("Warning: Could not parse {}", entry.path().filename().string());
			continue;
		}

		if (passes_filter(stats, criteria)) {
			candidates.push_back(stats.symbol);
			std::println("✓ {} ({} bars)", stats.symbol, stats.bar_count);
		} else {
			std::println("✗ {} (insufficient data: {} bars)", stats.symbol, stats.bar_count);
		}
	}

	std::println("");
	std::println("Candidates: {}/{}", candidates.size(),
		std::ranges::distance(std::filesystem::directory_iterator{bars_dir}));

	// Write candidates.json
	auto output_file = std::filesystem::path{"docs/candidates.json"};
	auto ofs = std::ofstream{output_file};

	if (!ofs) {
		std::println("Error: Could not write {}", output_file.string());
		return 1;
	}

	ofs << std::format(R"({{
  "timestamp": "{}",
  "symbols": {},
  "criteria": {{
    "min_avg_volume": {},
    "min_price": {},
    "max_price": {},
    "min_volatility": {}
  }},
  "total_candidates": {}
}}
)",
		std::chrono::system_clock::now(),
		json::array(candidates),
		criteria.min_avg_volume,
		criteria.min_price,
		criteria.max_price,
		criteria.min_volatility,
		candidates.size()
	);

	std::println("Wrote {}", output_file.string());

	return 0;
}
