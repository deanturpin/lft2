// Backtest module - tests strategies against historical bar data
// Uses the same constexpr entry/exit code as live trading

#include "bar.h"
#include "entry.h"
#include "exit.h"
#include "json.h"
#include "market.h"
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

  // Capture data range
  if (!bars.empty()) {
    result.first_timestamp = std::string{bars.front().timestamp};
    result.last_timestamp = std::string{bars.back().timestamp};
  }

  auto trades = std::vector<Trade>{};
  auto position = std::optional<::position>{};
  auto entry_bar_index = 0uz;

  // Walk through bars simulating live trading
  for (auto i = 0uz; i < bars.size(); ++i) {
    auto history = std::span{bars.data(), i + 1};
    auto &ts = bars[i].timestamp;

    if (!market::market_open(ts))
      continue;

    // Risk-off: liquidate any open position, skip new entries
    if (position && market::risk_off(ts)) {
      auto exit_price = bars[i].close;
      auto profit_pct =
          (exit_price - position->entry_price) / position->entry_price;
      auto duration = static_cast<int>(i - entry_bar_index);

      trades.push_back(Trade{.entry_price = position->entry_price,
                             .exit_price = exit_price,
                             .profit_pct = profit_pct,
                             .win = profit_pct > 0.0,
                             .duration_bars = duration});

      position.reset();
      continue;
    }

    // Check for normal exit when in position
    if (position && is_exit(*position, bars[i])) {
      auto exit_price = bars[i].close;
      auto profit_pct =
          (exit_price - position->entry_price) / position->entry_price;
      auto duration = static_cast<int>(i - entry_bar_index);

      trades.push_back(Trade{.entry_price = position->entry_price,
                             .exit_price = exit_price,
                             .profit_pct = profit_pct,
                             .win = profit_pct > 0.0,
                             .duration_bars = duration});

      position.reset();
    }
    // Check for entry signal when not in position (only during safe window)
    else if (!position && i >= 20 && !market::risk_off(ts) &&
             entry_func(history)) {
      auto entry_price = bars[i].close;

      // Set position parameters (10% take profit, 5% stop loss)
      position = ::position{.entry_price = entry_price,
                            .take_profit = entry_price * 1.10,
                            .stop_loss = entry_price * 0.95,
                            .trailing_stop = entry_price * 0.95};
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
    if (bars.size() < 50) {
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
    for (const auto &r : results) {
      if (r.trade_count > 0) {
        std::println("    {} - {}: {} trades, {:.1f}% win, {:.2f}% avg profit",
                     symbol, r.strategy_name, r.trade_count, r.win_rate * 100.0,
                     r.avg_profit * 100.0);
      }
    }

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

  ofs << "{\n";
  ofs << std::format("  \"timestamp\": \"{}\",\n", get_iso_timestamp());
  ofs << "  \"recommendations\": [\n";

  for (auto i = 0uz; i < all_results.size(); ++i) {
    const auto &rec = all_results[i];
    ofs << "    {\n";
    ofs << std::format("      \"symbol\": \"{}\",\n", rec.symbol);
    ofs << std::format("      \"strategy\": \"{}\",\n", rec.strategy_name);
    ofs << std::format("      \"win_rate\": {:.3f},\n", rec.win_rate);
    ofs << std::format("      \"avg_profit\": {:.4f},\n", rec.avg_profit);
    ofs << std::format("      \"trade_count\": {},\n", rec.trade_count);
    ofs << std::format("      \"min_duration_bars\": {},\n",
                       rec.min_duration_bars);
    ofs << std::format("      \"max_duration_bars\": {},\n",
                       rec.max_duration_bars);
    ofs << std::format("      \"first_timestamp\": \"{}\",\n",
                       rec.first_timestamp);
    ofs << std::format("      \"last_timestamp\": \"{}\"\n",
                       rec.last_timestamp);
    ofs << "    }";
    if (i < all_results.size() - 1)
      ofs << ",";
    ofs << "\n";
  }

  ofs << "  ]\n";
  ofs << "}\n";

  std::println("Wrote {}", output_file.string());

  return 0;
}
