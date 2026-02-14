#include "entry.h"
#include "exit.h"
#include "params.h"
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
#include "prices/de.h"
#include "prices/dia.h"
#include "prices/ge.h"
#include "prices/googl.h"
#include "prices/gs.h"
#include "prices/hon.h"
#include "prices/ief.h"
#include "prices/iwm.h"
#include "prices/jnj.h"
#include "prices/jpm.h"
#include "prices/ko.h"
#include "prices/lly.h"
#include "prices/lmnd.h"
#include "prices/meta.h"
#include "prices/ms.h"
#include "prices/msft.h"
#include "prices/nvda.h"
#include "prices/nvo.h"
#include "prices/pep.h"
#include "prices/pfe.h"
#include "prices/pg.h"
#include "prices/qqq.h"
#include "prices/rsp.h"
#include "prices/sap.h"
#include "prices/slb.h"
#include "prices/spy.h"
#include "prices/tlt.h"
#include "prices/tsla.h"
#include "prices/tsm.h"
#include "prices/unh.h"
#include "prices/vnq.h"
#include "prices/wmt.h"
#include "prices/xlf.h"
#include "prices/xlk.h"
#include "prices/xom.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <print>
#include <span>
#include <vector>

struct backtest_result {
  std::string_view symbol;
  std::size_t bar_count{};
  double max_gain_pct{};
  double max_loss_pct{};
  double avg_range_pct{};
  double volatility{};
  double win_rate_pct{};
  double avg_win_pct{};
  double avg_loss_pct{};
  double profit_factor{};
  double suggested_tp_pct{};
  double suggested_sl_pct{};
};

// Calculate backtest statistics for a single asset
template <std::size_t N>
backtest_result run_backtest(std::string_view symbol,
                              std::span<const bar, N> bars) {
  auto result = backtest_result{.symbol = symbol, .bar_count = bars.size()};

  if (bars.empty())
    return result;

  // Calculate intraday gains and losses (excluding gaps)
  auto intraday_gains = std::vector<double>{};
  auto intraday_losses = std::vector<double>{};
  auto continuous_ranges = std::vector<double>{};
  auto continuous_closes = std::vector<double>{};

  for (auto i = 0uz; i < bars.size(); ++i) {
    if (!is_valid(bars[i]))
      continue;

    // Check for gap from previous bar
    auto is_continuous = true;
    if (i > 0 && is_valid(bars[i - 1])) {
      // Parse timestamps to check gap (simplified: assume 5-min bars)
      // For now, just assume all bars are continuous
      // TODO: Add proper timestamp gap detection
    }

    if (is_continuous) {
      auto entry = bars[i].open;
      auto high = bars[i].high;
      auto low = bars[i].low;
      auto close = bars[i].close;

      // Maximum gain if entered at open and hit high
      auto max_gain_bar =
          entry > 0.0 ? (high - entry) / entry * 100.0 : 0.0;
      // Maximum loss if entered at open and hit low
      auto max_loss_bar = entry > 0.0 ? (low - entry) / entry * 100.0 : 0.0;

      intraday_gains.push_back(max_gain_bar);
      intraday_losses.push_back(max_loss_bar);
      continuous_ranges.push_back(close > 0.0 ? (high - low) / close * 100.0
                                              : 0.0);
      continuous_closes.push_back(close);
    }
  }

  if (intraday_gains.empty())
    return result;

  // Best and worst single-bar outcomes
  result.max_gain_pct = *std::ranges::max_element(intraday_gains);
  result.max_loss_pct = *std::ranges::min_element(intraday_losses);

  // Average intrabar range
  auto range_sum = 0.0;
  for (auto r : continuous_ranges)
    range_sum += r;
  result.avg_range_pct = range_sum / continuous_ranges.size();

  // Volatility (standard deviation of returns)
  if (continuous_closes.size() > 1) {
    auto returns = std::vector<double>{};
    for (auto i = 0uz; i < continuous_closes.size() - 1; ++i) {
      auto ret = (continuous_closes[i + 1] - continuous_closes[i]) /
                 continuous_closes[i] * 100.0;
      returns.push_back(ret);
    }

    auto mean = 0.0;
    for (auto r : returns)
      mean += r;
    mean /= returns.size();

    auto variance = 0.0;
    for (auto r : returns)
      variance += (r - mean) * (r - mean);
    variance /= returns.size();

    result.volatility = std::sqrt(variance);
  }

  // Win rate and expectancy metrics
  auto winning_bars = std::vector<double>{};
  auto losing_bars = std::vector<double>{};

  for (auto g : intraday_gains)
    if (g > 0.0)
      winning_bars.push_back(g);

  for (auto l : intraday_losses)
    if (l < 0.0)
      losing_bars.push_back(l);

  result.win_rate_pct = !intraday_gains.empty()
                            ? (static_cast<double>(winning_bars.size()) /
                               intraday_gains.size() * 100.0)
                            : 0.0;

  // Average win and loss
  if (!winning_bars.empty()) {
    auto sum = 0.0;
    for (auto w : winning_bars)
      sum += w;
    result.avg_win_pct = sum / winning_bars.size();
  }

  if (!losing_bars.empty()) {
    auto sum = 0.0;
    for (auto l : losing_bars)
      sum += l;
    result.avg_loss_pct = sum / losing_bars.size();
  }

  // Profit factor
  auto total_gains = 0.0;
  for (auto w : winning_bars)
    total_gains += w;

  auto total_losses = 0.0;
  for (auto l : losing_bars)
    total_losses += std::abs(l);

  result.profit_factor =
      total_losses > 0.0 ? total_gains / total_losses : 0.0;

  // Suggested trading parameters
  result.suggested_tp_pct = result.max_gain_pct * 0.5;
  result.suggested_sl_pct = std::min(std::abs(result.max_loss_pct) * 0.5,
                                     result.volatility * 2.0);

  return result;
}

int main() {
  using namespace std::string_view_literals;
  std::println("Low Frequency Trader v2 - Backtest\n");

  // All stocks
  auto stocks = std::array{
      std::pair{"AAPL"sv, parse_bars<1000>(aapl_json)},
      std::pair{"AMZN"sv, parse_bars<1000>(amzn_json)},
      std::pair{"ASML"sv, parse_bars<1000>(asml_json)},
      std::pair{"BABA"sv, parse_bars<1000>(baba_json)},
      std::pair{"BAC"sv, parse_bars<1000>(bac_json)},
      std::pair{"CAT"sv, parse_bars<1000>(cat_json)},
      std::pair{"COP"sv, parse_bars<1000>(cop_json)},
      std::pair{"COST"sv, parse_bars<1000>(cost_json)},
      std::pair{"CRML"sv, parse_bars<1000>(crml_json)},
      std::pair{"CVX"sv, parse_bars<1000>(cvx_json)},
      std::pair{"DE"sv, parse_bars<1000>(de_json)},
      std::pair{"DIA"sv, parse_bars<1000>(dia_json)},
      std::pair{"GE"sv, parse_bars<1000>(ge_json)},
      std::pair{"GOOGL"sv, parse_bars<1000>(googl_json)},
      std::pair{"GS"sv, parse_bars<1000>(gs_json)},
      std::pair{"HON"sv, parse_bars<1000>(hon_json)},
      std::pair{"IEF"sv, parse_bars<1000>(ief_json)},
      std::pair{"IWM"sv, parse_bars<1000>(iwm_json)},
      std::pair{"JNJ"sv, parse_bars<1000>(jnj_json)},
      std::pair{"JPM"sv, parse_bars<1000>(jpm_json)},
      std::pair{"KO"sv, parse_bars<1000>(ko_json)},
      std::pair{"LLY"sv, parse_bars<1000>(lly_json)},
      std::pair{"LMND"sv, parse_bars<1000>(lmnd_json)},
      std::pair{"META"sv, parse_bars<1000>(meta_json)},
      std::pair{"MS"sv, parse_bars<1000>(ms_json)},
      std::pair{"MSFT"sv, parse_bars<1000>(msft_json)},
      std::pair{"NVDA"sv, parse_bars<1000>(nvda_json)},
      std::pair{"NVO"sv, parse_bars<1000>(nvo_json)},
      std::pair{"PEP"sv, parse_bars<1000>(pep_json)},
      std::pair{"PFE"sv, parse_bars<1000>(pfe_json)},
      std::pair{"PG"sv, parse_bars<1000>(pg_json)},
      std::pair{"QQQ"sv, parse_bars<1000>(qqq_json)},
      std::pair{"RSP"sv, parse_bars<1000>(rsp_json)},
      std::pair{"SAP"sv, parse_bars<1000>(sap_json)},
      std::pair{"SLB"sv, parse_bars<1000>(slb_json)},
      std::pair{"SPY"sv, parse_bars<1000>(spy_json)},
      std::pair{"TLT"sv, parse_bars<1000>(tlt_json)},
      std::pair{"TSLA"sv, parse_bars<1000>(tsla_json)},
      std::pair{"TSM"sv, parse_bars<1000>(tsm_json)},
      std::pair{"UNH"sv, parse_bars<1000>(unh_json)},
      std::pair{"VNQ"sv, parse_bars<1000>(vnq_json)},
      std::pair{"WMT"sv, parse_bars<1000>(wmt_json)},
      std::pair{"XLF"sv, parse_bars<1000>(xlf_json)},
      std::pair{"XLK"sv, parse_bars<1000>(xlk_json)},
      std::pair{"XOM"sv, parse_bars<1000>(xom_json)},
  };

  // Run backtests
  auto results = std::vector<backtest_result>{};
  for (const auto &[symbol, bars] : stocks) {
    auto result = run_backtest(symbol, std::span{bars});
    results.push_back(result);
  }

  // Sort by max gain descending
  std::ranges::sort(results, [](const auto &a, const auto &b) {
    return a.max_gain_pct > b.max_gain_pct;
  });

  // Print table header
  std::println(
      "{:<8} {:>6} {:>10} {:>10} {:>10} {:>10} {:>10} {:>10} {:>10} {:>12} "
      "{:>10} {:>10}",
      "Symbol", "Bars", "MaxGain%", "MaxLoss%", "AvgRng%", "Vol%", "WinRate%",
      "AvgWin%", "AvgLoss%", "ProfitFact", "SugTP%", "SugSL%");

  // Print results
  for (const auto &r : results) {
    std::println("{:<8} {:>6} {:>10.2f} {:>10.2f} {:>10.2f} {:>10.2f} {:>10.1f} "
                 "{:>10.2f} {:>10.2f} {:>12.2f} {:>10.2f} {:>10.2f}",
                 r.symbol, r.bar_count, r.max_gain_pct, r.max_loss_pct,
                 r.avg_range_pct, r.volatility, r.win_rate_pct, r.avg_win_pct,
                 r.avg_loss_pct, r.profit_factor, r.suggested_tp_pct,
                 r.suggested_sl_pct);
  }

  return 0;
}
