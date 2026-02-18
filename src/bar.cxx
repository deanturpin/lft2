#include "bar.h"
#include "json.h"
#include "paths.h"
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

// Load bars from docs/bars/{symbol}.json produced by the fetch module.
// Uses the json.h parser — same logic as the constexpr path.
std::vector<bar> load_bars(std::string_view symbol) {
  auto ifs = std::ifstream{std::filesystem::path{paths::bars(symbol)}};
  if (!ifs)
    return {};

  auto content = std::string{std::istreambuf_iterator<char>(ifs), {}};
  auto s = std::string_view{content};

  // Scan the top-level object for the "bars" key.
  // The fetch module writes {"symbol":..., "bars":[...],...} so we cannot
  // assume "bars" is the first key — skip any preceding keys.
  if (!expect(s, '{'))
    return {};

  bool found = false;
  while (!s.empty() && !s.starts_with('}')) {
    skip_ws(s);
    auto key = parse_string(s);
    if (!expect(s, ':'))
      return {};
    if (key == "bars") {
      if (!expect(s, '['))
        return {};
      found = true;
      break;
    }
    // Skip non-array scalar value (string or number) and continue
    skip_ws(s);
    if (s.starts_with('"'))
      parse_string(s);
    else
      parse_number<double>(s);
    skip_comma(s);
  }

  if (!found)
    return {};

  // Persistent storage for timestamp string_views held by each bar.
  // parse_bar() sets b.timestamp as a string_view into the original JSON,
  // but content is local — so we must copy each timestamp into stable storage.
  static std::vector<std::string> timestamp_storage;

  auto bars = std::vector<bar>{};
  while (true) {
    skip_ws(s);
    if (s.empty() || s[0] == ']')
      break;

    auto b = parse_bar(s);
    if (is_valid(b)) {
      timestamp_storage.emplace_back(b.timestamp);
      b.timestamp = timestamp_storage.back();
      bars.push_back(b);
    }

    skip_ws(s);
    if (!s.empty() && s[0] == ',')
      s.remove_prefix(1);
  }

  return bars;
}
