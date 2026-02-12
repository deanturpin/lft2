#include "prices/aapl.h"
#include "prices/amzn.h"
#include "prices/asml.h"
#include "prices/baba.h"
#include "prices/bac.h"
#include "prices/cat.h"
#include "prices/cop.h"
#include "prices/cost.h"
#include "prices/crml.h"
#include "prices/cvx.h"
#include "json.h"
#include <print>

int main() {
  using namespace std::string_view_literals;
  std::println("Low Frequency Trader v2\n");

  // Test all 10 stocks
  auto stocks = std::array<std::pair<std::string_view, std::string_view>, 10>{{
      {"AAPL"sv, aapl_json}, {"AMZN"sv, amzn_json},
      {"ASML"sv, asml_json}, {"BABA"sv, baba_json},
      {"BAC"sv, bac_json},   {"CAT"sv, cat_json},
      {"COP"sv, cop_json},   {"COST"sv, cost_json},
      {"CRML"sv, crml_json}, {"CVX"sv, cvx_json}}};

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
