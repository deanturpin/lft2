#include "json.h"
#include <print>

int main() {
  std::println("Low Frequency Trader v2");

  // Test with small JSON sample
  constexpr std::string_view test_json = R"(
{
  "bars": [
    {
      "c": 255.75,
      "h": 255.855,
      "l": 255.47,
      "n": 100,
      "o": 255.63,
      "t": "2026-01-29T16:35:00Z",
      "v": 20688,
      "vw": 255.72
    },
    {
      "c": 255.69,
      "h": 256.325,
      "l": 255.66,
      "n": 110,
      "o": 256.28,
      "t": "2026-01-29T16:30:00Z",
      "v": 24829,
      "vw": 256.1
    }
  ]
}
)";

  constexpr auto bars = parse_bars<2>(test_json);

  std::println("Parsed {} bars:", bars.size());
  for (const auto &b : bars)
    std::println("  {} | O:{} H:{} L:{} C:{} V:{}", b.timestamp, b.open,
                 b.high, b.low, b.close, b.volume);

  return 0;
}
