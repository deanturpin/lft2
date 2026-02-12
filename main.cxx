#include "aapl_data.h"
#include "json.h"
#include <print>

int main() {
  std::println("Low Frequency Trader v2");

  auto bars = parse_bars<1000>(aapl_json);

  std::println("Parsed {} bars", bars.size());
  std::println("First bar: {} | O:{} H:{} L:{} C:{} V:{}", bars[0].timestamp,
               bars[0].open, bars[0].high, bars[0].low, bars[0].close,
               bars[0].volume);
  std::println("Last bar: {} | O:{} H:{} L:{} C:{} V:{}", bars[999].timestamp,
               bars[999].open, bars[999].high, bars[999].low, bars[999].close,
               bars[999].volume);

  return 0;
}
