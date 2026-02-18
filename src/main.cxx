#include "json.h"
#include "snapshots/aapl.h"
#include "snapshots/amzn.h"
#include "snapshots/asml.h"
#include "snapshots/baba.h"
#include "snapshots/bac.h"
#include "snapshots/cat.h"
#include "snapshots/cop.h"
#include "snapshots/cost.h"
#include "snapshots/crml.h"
#include "snapshots/cvx.h"
#include <print>

// Evaluate trades and open positions
// void evaluate(std::string_view symbol, std::array<bar, 1000> bars) {}

int main() {
  using namespace std::string_view_literals;
  std::println("Low Frequency Trader v2\n");

  // Test all 10 stocks
  auto stocks = std::array<std::pair<std::string_view, std::string_view>, 10>{
      {{"AAPL"sv, aapl_json},
       {"AMZN"sv, amzn_json},
       {"ASML"sv, asml_json},
       {"BABA"sv, baba_json},
       {"BAC"sv, bac_json},
       {"CAT"sv, cat_json},
       {"COP"sv, cop_json},
       {"COST"sv, cost_json},
       {"CRML"sv, crml_json},
       {"CVX"sv, cvx_json}}};

  for (const auto &[symbol, json] : stocks) {
    auto bars = parse_bars<1000>(json);

    std::println("{}\t{}", symbol, bars.front().open);

    // auto first = bars.front();
    // auto last = bars.back();

    // std::println("{}: {} bars | First: {} ${:.2f} | Last: {} ${:.2f}",
    // symbol,
    //              bars.size(), first.timestamp, first.close, last.timestamp,
    //              last.close);
  }

  return 0;
}
