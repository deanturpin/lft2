#include "aapl_data.h"
#include "json.h"
#include <print>

int main() {
  std::println("Low Frequency Trader v2");

  auto bars = parse_bars<1000>(aapl_json);

  std::println("Parsed {} bars", bars.size());

  auto first = bars.front();
  auto last = bars.back();

  std::println("First bar: {} | O:{} H:{} L:{} C:{} V:{}", first.timestamp,
               first.open, first.high, first.low, first.close, first.volume);
  std::println("Last bar: {} | O:{} H:{} L:{} C:{} V:{}", last.timestamp,
               last.open, last.high, last.low, last.close, last.volume);

  return 0;
}
