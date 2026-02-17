#include "bar.h"
#include "json.h"
#include <filesystem>
#include <format>
#include <fstream>
#include <string>
#include <vector>

// Load bars from docs/bars/{symbol}.json produced by the fetch module.
// Uses the json.h parser — same logic as the constexpr path.
std::vector<bar> load_bars(std::string_view symbol) {
  auto ifs = std::ifstream{
      std::filesystem::path{std::format("docs/bars/{}.json", symbol)}};
  if (!ifs)
    return {};

  auto content = std::string{std::istreambuf_iterator<char>(ifs), {}};
  auto s = std::string_view{content};

  // Advance past {"bars":[
  if (!expect(s, '{'))
    return {};
  skip_ws(s);
  if (parse_string(s) != "bars")
    return {};
  if (!expect(s, ':'))
    return {};
  if (!expect(s, '['))
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
