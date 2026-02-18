// Backtest module - tests strategies against historical bar data
// Uses the same constexpr entry/exit code as live trading

#include "bar.h"
#include "entry.h"
#include "exit.h"
#include "json.h"
#include "market.h"
#include "params.h"
#include "paths.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <print>
#include <ranges>
#include <sstream>
#include <string>
#include <vector>

struct StrategyResult {
  std::string symbol;
  std::string strategy_name;
  double win_rate = 0.0;
  double avg_profit = 0.0;
  int trade_count = 0;
  double total_return = 0.0;
  int min_duration_bars = 0;
  int max_duration_bars = 0;
  std::string first_timestamp;
  std::string last_timestamp;
};

struct Trade {
  double entry_price;
  double exit_price;
  double profit_pct;
  bool win;
  int duration_bars;
};

// Backtest a specific strategy on bar data
template <typename EntryFunc>
StrategyResult backtest_strategy(std::span<const bar> bars,
                                 EntryFunc entry_func,
                                 std::string_view strategy_name) {
  auto result = StrategyResult{};
  result.strategy_name = std::string{strategy_name};

  if (bars.empty()) {
    std::println("  ✗ {} - no bars", strategy_name);
    return result;
  }

  result.first_timestamp = std::string{bars.front().timestamp};
  result.last_timestamp = std::string{bars.back().timestamp};

  auto trades = std::vector<Trade>{};
  auto position = std::optional<::position>{};
  auto entry_bar_index = 0uz;

  // Walk through bars: now is the signal bar (close), next is the fill
  // bar (open). Stop one bar early so lookahead is always valid.
  for (auto i = 20uz; i + 1 < bars.size(); ++i) {
    const auto &now = bars[i];
    const auto &next = bars[i + 1];
    auto history = std::span{bars.data(), i + 1};

    if (!market::market_open(now.timestamp))
      continue;

    // Risk-off: liquidate any open position; fill at next bar's open like any
    // exit
    if (position && market::risk_off(now.timestamp)) {
      auto profit_pct =
          (next.open - position->entry_price) / position->entry_price;
      trades.push_back(
          Trade{.entry_price = position->entry_price,
                .exit_price = next.open,
                .profit_pct = profit_pct,
                .win = profit_pct > 0.0,
                .duration_bars = static_cast<int>(i - entry_bar_index)});
      position.reset();
      continue;
    }

    // Update trailing stop to track peak price while in position
    if (position) {
      auto peak =
          position->trailing_stop / (1.0 - default_params.trailing_stop_pct);
      if (now.close > peak)
        position->trailing_stop =
            now.close * (1.0 - default_params.trailing_stop_pct);
    }

    // Exit signal fires on now's close; fill at next bar's open
    if (position && is_exit(*position, now)) {
      auto profit_pct =
          (next.open - position->entry_price) / position->entry_price;
      trades.push_back(
          Trade{.entry_price = position->entry_price,
                .exit_price = next.open,
                .profit_pct = profit_pct,
                .win = profit_pct > 0.0,
                .duration_bars = static_cast<int>(i - entry_bar_index)});
      position.reset();
    }
    // Entry signal fires on now's close; fill at next bar's open
    else if (!position && !market::risk_off(now.timestamp) &&
             entry_func(history)) {
      auto levels = calculate_levels(next.open, default_params);
      position = ::position{.entry_price = next.open,
                            .take_profit = levels.take_profit,
                            .stop_loss = levels.stop_loss,
                            .trailing_stop = levels.trailing_stop};
      entry_bar_index = i;
    }
  }

  // Calculate metrics
  if (trades.empty()) {
    return result;
  }

  auto wins = 0uz;
  auto total_profit = 0.0;
  auto min_duration = std::numeric_limits<int>::max();
  auto max_duration = 0;

  for (const auto &trade : trades) {
    if (trade.win)
      wins++;
    total_profit += trade.profit_pct;
    min_duration = std::min(min_duration, trade.duration_bars);
    max_duration = std::max(max_duration, trade.duration_bars);
  }

  result.trade_count = static_cast<int>(trades.size());
  result.win_rate = static_cast<double>(wins) / trades.size();
  result.avg_profit = total_profit / trades.size();
  result.total_return = total_profit;
  result.min_duration_bars = min_duration;
  result.max_duration_bars = max_duration;

  return result;
}

std::string get_iso_timestamp() {
  auto now = std::chrono::system_clock::now();
  auto time = std::chrono::system_clock::to_time_t(now);
  auto ss = std::ostringstream{};
  ss << std::put_time(std::gmtime(&time), "%Y-%m-%dT%H:%M:%SZ");
  return ss.str();
}

int main() {
  std::println("Backtest Module - Testing strategies");
  std::println("");

  // Load candidates from filter output
  auto candidates_file = std::filesystem::path{paths::candidates};
  if (!std::filesystem::exists(candidates_file)) {
    std::println("Error: candidates.json not found");
    std::println("Run filter module first");
    return 1;
  }

  // Parse candidates.json to get symbol list
  auto ifs = std::ifstream{candidates_file};
  auto json_str = std::string{std::istreambuf_iterator<char>(ifs), {}};

  // Extract symbols array using json.h helper
  auto candidates = std::vector<std::string>{};
  json_string_array(json_str, "symbols", [&](std::string_view sym) {
    candidates.emplace_back(sym);
  });

  std::println("Testing {} candidates from filter", candidates.size());
  std::println("");

  auto all_results = std::vector<StrategyResult>{};

  // Test each candidate with all three strategies
  for (const auto &symbol : candidates) {
    auto bars = load_bars(symbol);
    if (bars.empty()) {
      std::println("✗ {} - bar data not found", symbol);
      continue;
    }
    if (bars.size() < 100) {
      std::println("✗ {} - insufficient bars ({})", symbol, bars.size());
      continue;
    }

    // Test all five strategies
    auto results = std::vector<StrategyResult>{};

    results.push_back(
        backtest_strategy(bars, volume_surge_dip, "volume_surge"));
    results[0].symbol = symbol;

    results.push_back(
        backtest_strategy(bars, mean_reversion, "mean_reversion"));
    results[1].symbol = symbol;

    results.push_back(
        backtest_strategy(bars, sma_crossover<10, 20>, "sma_crossover"));
    results[2].symbol = symbol;

    results.push_back(backtest_strategy(bars, price_dip, "price_dip"));
    results[3].symbol = symbol;

    results.push_back(
        backtest_strategy(bars, volatility_breakout, "volatility_breakout"));
    results[4].symbol = symbol;

    // Debug output showing trade counts and win rates
    for (const auto &r : results)
      if (r.trade_count > 0)
        std::println("    {} - {}: {} trades, {:.1f}% win, {:.2f}% avg profit",
                     symbol, r.strategy_name, r.trade_count, r.win_rate * 100.0,
                     r.avg_profit * 100.0);

    // Find best strategy for this symbol
    auto best = std::max_element(
        results.begin(), results.end(), [](const auto &a, const auto &b) {
          // Prefer strategies with more trades and higher win rate
          if (a.trade_count == 0 && b.trade_count == 0)
            return false;
          if (a.trade_count == 0)
            return true;
          if (b.trade_count == 0)
            return false;

          // Score = win_rate * avg_profit * sqrt(trade_count)
          auto score_a = a.win_rate * a.avg_profit *
                         std::sqrt(static_cast<double>(a.trade_count));
          auto score_b = b.win_rate * b.avg_profit *
                         std::sqrt(static_cast<double>(b.trade_count));
          return score_a < score_b;
        });

    // Include if it meets minimum criteria
    // Accept: (1 trade with 100% win and >5% profit) OR (2+ trades with 40% win
    // and >0.1% profit)
    bool passes = (best->trade_count >= 1 && best->win_rate == 1.0 &&
                   best->avg_profit >= 0.05) ||
                  (best->trade_count >= 2 && best->win_rate >= 0.40 &&
                   best->avg_profit >= 0.001);

    if (passes) {
      std::println("✓ {} - {} (win: {:.1f}%, profit: {:.2f}%, trades: {})",
                   symbol, best->strategy_name, best->win_rate * 100.0,
                   best->avg_profit * 100.0, best->trade_count);
      all_results.push_back(*best);
    } else {
      std::println("✗ {} - no profitable strategy found", symbol);
    }
  }

  std::println("");
  std::println("Profitable strategies: {}/{}", all_results.size(),
               candidates.size());

  // Sort by total return (best first)
  std::sort(all_results.begin(), all_results.end(),
            [](const auto &a, const auto &b) {
              return a.total_return > b.total_return;
            });

  // Write strategies.json
  auto output_file = std::filesystem::path{paths::strategies};
  auto ofs = std::ofstream{output_file};

  if (!ofs) {
    std::println("Error: Could not write {}", output_file.string());
    return 1;
  }

  ofs << "{\"timestamp\": \"" << get_iso_timestamp()
      << "\", \"recommendations\": [\n";

  for (auto i = 0uz; i < all_results.size(); ++i) {
    const auto &rec = all_results[i];
    auto sep = i + 1 < all_results.size() ? "," : "";
    ofs << std::format(
        R"(    {{
      "symbol": "{}",
      "strategy": "{}",
      "win_rate": {:.3f},
      "avg_profit": {:.4f},
      "trade_count": {},
      "min_duration_bars": {},
      "max_duration_bars": {},
      "first_timestamp": "{}",
      "last_timestamp": "{}"
    }}{}
)",
        rec.symbol, rec.strategy_name, rec.win_rate, rec.avg_profit,
        rec.trade_count, rec.min_duration_bars, rec.max_duration_bars,
        rec.first_timestamp, rec.last_timestamp, sep);
  }

  ofs << "]}\n";

  std::println("Wrote {}", output_file.string());

  return 0;
}
