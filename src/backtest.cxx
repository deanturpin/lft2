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

struct trade_result {
  std::size_t entry_bar{};
  std::size_t exit_bar{};
  double entry_price{};
  double exit_price{};
  double gain_pct{};
  std::size_t duration_bars{};
};

struct best_entry_context {
  std::size_t bar_idx{};
  double gain_pct{};
  double entry_price{};
  std::string_view timestamp{};
  // Previous 5 bars before entry (to analyse what preceded the big move)
  std::array<double, 5> prev_closes{};
  std::array<double, 5> prev_volumes{};
};

struct backtest_result {
  std::string_view symbol;
  std::size_t bar_count{};
  std::size_t trade_count{};
  double best_gain_pct{};
  std::size_t best_gain_duration{};
  double worst_loss_pct{};
  std::size_t worst_loss_duration{};
  double avg_gain_pct{};
  double win_rate_pct{};
  double profit_factor{};
  double suggested_tp_pct{};
  double suggested_sl_pct{};
  double max_intraday_gain_pct{}; // Best possible gain from entry signal to EOD
  double max_intraday_loss_pct{}; // Worst drawdown from entry signal to EOD
  best_entry_context top_entry{};  // Context for best entry point
};

// Run backtest simulation using actual entry/exit logic
template <std::size_t N>
backtest_result run_backtest(std::string_view symbol,
                              std::span<const bar, N> bars) {
  auto result = backtest_result{.symbol = symbol, .bar_count = bars.size()};

  if (bars.empty())
    return result;

  auto trades = std::vector<trade_result>{};
  auto in_position = false;
  auto current_position = position{};
  auto entry_bar_idx = 0uz;

  // Track maximum possible gains/losses for parameter optimisation
  auto max_intraday_gains = std::vector<double>{};
  auto max_intraday_losses = std::vector<double>{};

  // Helper to detect gap between bars (overnight or market close)
  // Gaps indicate non-trading periods where we should close positions
  auto is_gap = [&bars](std::size_t prev_idx, std::size_t curr_idx) {
    if (prev_idx >= bars.size() || curr_idx >= bars.size())
      return false;

    // Check if dates differ (YYYY-MM-DD portion of timestamp)
    auto prev_date = bars[prev_idx].timestamp.substr(0, 10);
    auto curr_date = bars[curr_idx].timestamp.substr(0, 10);
    return prev_date != curr_date;
  };

  // Analyse maximum possible intraday movements from every bar
  auto best_entry_idx = 0uz;
  auto best_entry_gain = 0.0;

  for (auto i = 0uz; i < bars.size(); ++i) {
    if (!is_valid(bars[i]))
      continue;

    // For each bar, calculate max gain/loss until end of trading day
    auto entry_price = bars[i].open;
    auto max_gain = 0.0;
    auto max_loss = 0.0;

    for (auto j = i; j < bars.size(); ++j) {
      if (!is_valid(bars[j]))
        continue;
      if (j > i && is_gap(j - 1, j))
        break; // Stop at end of trading day

      auto gain = (bars[j].high - entry_price) / entry_price * 100.0;
      auto loss = (bars[j].low - entry_price) / entry_price * 100.0;
      max_gain = std::max(max_gain, gain);
      max_loss = std::min(max_loss, loss);
    }

    max_intraday_gains.push_back(max_gain);
    max_intraday_losses.push_back(max_loss);

    // Track best entry point
    if (max_gain > best_entry_gain) {
      best_entry_gain = max_gain;
      best_entry_idx = i;
    }
  }

  // Capture context around best entry point
  if (best_entry_idx >= 5 && best_entry_idx < bars.size()) {
    result.top_entry.bar_idx = best_entry_idx;
    result.top_entry.gain_pct = best_entry_gain;
    result.top_entry.entry_price = bars[best_entry_idx].open;
    result.top_entry.timestamp = bars[best_entry_idx].timestamp;

    // Get previous 5 bars
    for (auto k = 0uz; k < 5; ++k) {
      auto idx = best_entry_idx - 5 + k;
      result.top_entry.prev_closes[k] = bars[idx].close;
      result.top_entry.prev_volumes[k] = static_cast<double>(bars[idx].volume);
    }
  }

  // Run trading simulation for actual trade statistics
  for (auto i = 0uz; i < bars.size(); ++i) {
    if (!is_valid(bars[i]))
      continue;

    if (!in_position) {
      // Check for entry signal using last 21 bars (for SMA calculation)
      if (i >= 21) {
        auto history = std::span{bars.data(), i + 1}.last(21);
        if (is_entry(history)) {
          // Enter position at next bar's open
          if (i + 1 < bars.size() && is_valid(bars[i + 1])) {
            auto entry_price = bars[i + 1].open;
            auto levels = calculate_levels(entry_price, default_params);
            current_position = position{.entry_price = entry_price,
                                        .take_profit = levels.take_profit,
                                        .stop_loss = levels.stop_loss,
                                        .trailing_stop = levels.trailing_stop};
            in_position = true;
            entry_bar_idx = i + 1;
          }
        }
      }
    } else {
      // Check for overnight gap - close position at last bar before gap
      if (i > entry_bar_idx && is_gap(i - 1, i)) {
        auto exit_price = bars[i - 1].close;
        auto gain_pct = (exit_price - current_position.entry_price) /
                        current_position.entry_price * 100.0;
        auto duration = (i - 1) - entry_bar_idx;

        trades.push_back(trade_result{
            .entry_bar = entry_bar_idx,
            .exit_bar = i - 1,
            .entry_price = current_position.entry_price,
            .exit_price = exit_price,
            .gain_pct = gain_pct,
            .duration_bars = duration,
        });

        in_position = false;
        continue;
      }

      // Update trailing stop
      auto current_price = bars[i].close;
      if (current_price > current_position.entry_price) {
        auto new_trailing = current_price * (1.0 - default_params.trailing_stop_pct);
        if (new_trailing > current_position.trailing_stop)
          current_position.trailing_stop = new_trailing;
      }

      // Check for exit
      if (is_exit(current_position, bars[i])) {
        auto exit_price = bars[i].close;
        auto gain_pct = (exit_price - current_position.entry_price) /
                        current_position.entry_price * 100.0;
        auto duration = i - entry_bar_idx;

        trades.push_back(trade_result{
            .entry_bar = entry_bar_idx,
            .exit_bar = i,
            .entry_price = current_position.entry_price,
            .exit_price = exit_price,
            .gain_pct = gain_pct,
            .duration_bars = duration,
        });

        in_position = false;
      }
    }
  }

  if (trades.empty())
    return result;

  // Calculate statistics from completed trades
  result.trade_count = trades.size();

  // Find best and worst trades
  auto best_trade_it = std::ranges::max_element(
      trades, [](const auto &a, const auto &b) { return a.gain_pct < b.gain_pct; });
  auto worst_trade_it = std::ranges::min_element(
      trades, [](const auto &a, const auto &b) { return a.gain_pct < b.gain_pct; });

  result.best_gain_pct = best_trade_it->gain_pct;
  result.best_gain_duration = best_trade_it->duration_bars;
  result.worst_loss_pct = worst_trade_it->gain_pct;
  result.worst_loss_duration = worst_trade_it->duration_bars;

  // Average gain
  auto total_gain = 0.0;
  for (const auto &t : trades)
    total_gain += t.gain_pct;
  result.avg_gain_pct = total_gain / trades.size();

  // Win rate
  auto winning_trades =
      std::ranges::count_if(trades, [](const auto &t) { return t.gain_pct > 0.0; });
  result.win_rate_pct =
      static_cast<double>(winning_trades) / trades.size() * 100.0;

  // Profit factor
  auto total_wins = 0.0;
  auto total_losses = 0.0;
  for (const auto &t : trades) {
    if (t.gain_pct > 0.0)
      total_wins += t.gain_pct;
    else
      total_losses += std::abs(t.gain_pct);
  }
  result.profit_factor = total_losses > 0.0 ? total_wins / total_losses : 0.0;

  // Suggested parameters based on actual trades
  result.suggested_tp_pct = result.best_gain_pct * 0.5;
  result.suggested_sl_pct =
      std::min(std::abs(result.worst_loss_pct) * 0.5, result.best_gain_pct * 0.25);

  // Calculate maximum possible intraday movements from entry signals
  if (!max_intraday_gains.empty()) {
    result.max_intraday_gain_pct =
        *std::ranges::max_element(max_intraday_gains);
  }
  if (!max_intraday_losses.empty()) {
    result.max_intraday_loss_pct =
        *std::ranges::min_element(max_intraday_losses);
  }

  return result;
}

int main(int argc, char *argv[]) {
  using namespace std::string_view_literals;

  // Check for --json flag
  auto json_output = false;
  if (argc > 1 && std::string_view{argv[1]} == "--json")
    json_output = true;

  if (!json_output)
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

  // Sort by max intraday gain descending (best possible movement)
  std::ranges::sort(results, [](const auto &a, const auto &b) {
    return a.max_intraday_gain_pct > b.max_intraday_gain_pct;
  });

  if (json_output) {
    // Output JSON for workflow
    std::println("{{");
    for (auto i = 0uz; i < results.size(); ++i) {
      const auto &r = results[i];
      std::println("  \"{}\": {{", r.symbol);
      std::println("    \"bar_count\": {},", r.bar_count);
      std::println("    \"trade_count\": {},", r.trade_count);
      std::println("    \"best_gain_pct\": {:.2f},", r.best_gain_pct);
      std::println("    \"best_gain_duration\": {},", r.best_gain_duration);
      std::println("    \"worst_loss_pct\": {:.2f},", r.worst_loss_pct);
      std::println("    \"worst_loss_duration\": {},", r.worst_loss_duration);
      std::println("    \"avg_gain_pct\": {:.2f},", r.avg_gain_pct);
      std::println("    \"win_rate_pct\": {:.2f},", r.win_rate_pct);
      std::println("    \"profit_factor\": {:.2f},", r.profit_factor);
      std::println("    \"suggested_tp_pct\": {:.2f},", r.suggested_tp_pct);
      std::println("    \"suggested_sl_pct\": {:.2f},", r.suggested_sl_pct);
      std::println("    \"max_intraday_gain_pct\": {:.2f},", r.max_intraday_gain_pct);
      std::println("    \"max_intraday_loss_pct\": {:.2f}", r.max_intraday_loss_pct);
      std::println("  }}{}", i < results.size() - 1 ? "," : "");
    }
    std::println("}}");
  } else {
    // Print table header
    std::println("{:<8} {:>6} {:>7} {:>10} {:>12} {:>10} {:>12} {:>10} {:>10} {:>12} "
                 "{:>10} {:>10} {:>12} {:>12}",
                 "Symbol", "Bars", "Trades", "BestGain%", "BestDur(5m)", "WorstLoss%",
                 "WorstDur(5m)", "AvgGain%", "WinRate%", "ProfitFact", "SugTP%", "SugSL%",
                 "MaxIntGain%", "MaxIntLoss%");

    // Print results
    for (const auto &r : results) {
      std::println("{:<8} {:>6} {:>7} {:>10.2f} {:>12} {:>10.2f} {:>12} {:>10.2f} "
                   "{:>10.1f} {:>12.2f} {:>10.2f} {:>10.2f} {:>12.2f} {:>12.2f}",
                   r.symbol, r.bar_count, r.trade_count, r.best_gain_pct,
                   r.best_gain_duration, r.worst_loss_pct, r.worst_loss_duration,
                   r.avg_gain_pct, r.win_rate_pct, r.profit_factor,
                   r.suggested_tp_pct, r.suggested_sl_pct, r.max_intraday_gain_pct,
                   r.max_intraday_loss_pct);
    }

    // Show top 5 best entry contexts
    std::println("\n=== Top 5 Best Entry Points - What Preceded the Big Moves? ===\n");
    for (auto i = 0uz; i < std::min(5uz, results.size()); ++i) {
      const auto &r = results[i];
      std::println("{} - {}% gain at bar {} ({})", r.symbol, r.top_entry.gain_pct,
                   r.top_entry.bar_idx, r.top_entry.timestamp);
      std::println("  Entry price: ${:.2f}", r.top_entry.entry_price);
      std::print("  Previous 5 closes: ");
      for (auto j = 0uz; j < 5; ++j)
        std::print("${:.2f} ", r.top_entry.prev_closes[j]);
      std::println("");
      std::print("  Previous 5 volumes: ");
      for (auto j = 0uz; j < 5; ++j)
        std::print("{:.0f} ", r.top_entry.prev_volumes[j]);
      std::println("\n");
    }
  }

  return 0;
}
