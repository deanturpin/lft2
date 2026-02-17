#include "bar.h"
#include <filesystem>
#include <format>
#include <fstream>
#include <string>
#include <vector>

// Load bars from docs/bars/{symbol}.json produced by the fetch module.
// Parses the "bars" array and returns only valid bars.
std::vector<bar> load_bars(std::string_view symbol) {
	auto ifs = std::ifstream{std::filesystem::path{std::format("docs/bars/{}.json", symbol)}};
	if (!ifs)
		return {};

	auto json_str = std::string{std::istreambuf_iterator<char>(ifs), {}};

	auto pos = json_str.find("\"bars\"");
	if (pos == std::string::npos)
		return {};

	auto array_start = json_str.find('[', pos);
	auto array_end   = json_str.find(']', array_start);
	if (array_start == std::string::npos || array_end == std::string::npos)
		return {};

	// Persistent storage for timestamp string_views held by each bar
	static std::vector<std::string> timestamp_storage;

	auto extract_double = [](std::string_view obj, std::string_view key) {
		auto key_pos = obj.find(std::format("\"{}\"", key));
		if (key_pos == std::string_view::npos) return 0.0;
		auto colon = obj.find(':', key_pos);
		auto comma = obj.find_first_of(",}", colon);
		return std::stod(std::string{obj.substr(colon + 1, comma - colon - 1)});
	};

	auto extract_uint = [](std::string_view obj, std::string_view key) {
		auto key_pos = obj.find(std::format("\"{}\"", key));
		if (key_pos == std::string_view::npos) return 0u;
		auto colon = obj.find(':', key_pos);
		auto comma = obj.find_first_of(",}", colon);
		return static_cast<unsigned>(std::stoul(std::string{obj.substr(colon + 1, comma - colon - 1)}));
	};

	auto extract_string = [](std::string_view obj, std::string_view key) {
		auto key_pos = obj.find(std::format("\"{}\"", key));
		if (key_pos == std::string_view::npos) return std::string{};
		auto colon  = obj.find(':', key_pos);
		auto quote1 = obj.find('"', colon);
		auto quote2 = obj.find('"', quote1 + 1);
		return std::string{obj.substr(quote1 + 1, quote2 - quote1 - 1)};
	};

	auto bars    = std::vector<bar>{};
	auto obj_pos = array_start + 1;

	while (obj_pos < array_end) {
		auto obj_start = json_str.find('{', obj_pos);
		if (obj_start >= array_end) break;
		auto obj_end = json_str.find('}', obj_start);
		auto obj     = std::string_view{json_str}.substr(obj_start, obj_end - obj_start + 1);

		auto b       = bar{};
		b.close      = extract_double(obj, "c");
		b.high       = extract_double(obj, "h");
		b.low        = extract_double(obj, "l");
		b.open       = extract_double(obj, "o");
		b.vwap       = extract_double(obj, "vw");
		b.volume     = extract_uint(obj, "v");
		b.num_trades = extract_uint(obj, "n");

		timestamp_storage.push_back(extract_string(obj, "t"));
		b.timestamp = timestamp_storage.back();

		if (is_valid(b))
			bars.push_back(b);

		obj_pos = obj_end + 1;
	}

	return bars;
}
