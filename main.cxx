#include "aapl_data.h"
#include "amzn_data.h"
#include "asml_data.h"
#include "baba_data.h"
#include "bac_data.h"
#include "cat_data.h"
#include "cop_data.h"
#include "cost_data.h"
#include "crml_data.h"
#include "cvx_data.h"
#include "json.h"
#include <print>

int main() {
  std::println("Low Frequency Trader v2\n");

  // Test all 10 stocks
  auto stocks = std::array{
      std::pair{"AAPL", aapl_json}, std::pair{"AMZN", amzn_json},
      std::pair{"ASML", asml_json}, std::pair{"BABA", baba_json},
      std::pair{"BAC", bac_json},   std::pair{"CAT", cat_json},
      std::pair{"COP", cop_json},   std::pair{"COST", cost_json},
      std::pair{"CRML", crml_json}, std::pair{"CVX", cvx_json}};

  for (const auto &[symbol, json] : stocks) {
    auto bars = parse_bars<1000>(json);
    auto first = bars.front();
    auto last = bars.back();

    std::println("{}: {} bars | First: {} ${:.2f} | Last: {} ${:.2f}", symbol,
                 bars.size(), first.timestamp, first.close, last.timestamp,
                 last.close);
  }

  return 0;
}
