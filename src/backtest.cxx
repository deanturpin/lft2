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

struct Trade {
  double entry_price;
  double exit_price;
  double profit_pct;
  bool win;
  int duration_bars;
  exit_reason reason;          // Why did it exit?
  std::string entry_timestamp; // When entered
  std::string exit_timestamp;  // When exited
};

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
  std::vector<Trade> trades; // Per-trade details for debug output
  bool viable = false;       // True if win_rate >= 0.50 && trade_count >= 5
};

// Convert exit_reason to string for output
constexpr std::string_view exit_reason_str(exit_reason r) {
  switch (r) {
  case exit_reason::take_profit:
    return "take_profit";
  case exit_reason::stop_loss:
    return "stop_loss";
  case exit_reason::trailing_stop:
    return "trailing_stop";
  case exit_reason::risk_off:
    return "risk_off";
  case exit_reason::end_of_data:
    return "end_of_data";
  default:
    return "none";
  }
}

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
                .duration_bars = static_cast<int>(i - entry_bar_index),
                .reason = exit_reason::risk_off,
                .entry_timestamp = std::string{bars[entry_bar_index].timestamp},
                .exit_timestamp = std::string{next.timestamp}});
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
    auto exit_check = position ? check_exit(*position, now) : exit_reason::none;
    if (exit_check != exit_reason::none) {
      auto profit_pct =
          (next.open - position->entry_price) / position->entry_price;
      trades.push_back(
          Trade{.entry_price = position->entry_price,
                .exit_price = next.open,
                .profit_pct = profit_pct,
                .win = profit_pct > 0.0,
                .duration_bars = static_cast<int>(i - entry_bar_index),
                .reason = exit_check,
                .entry_timestamp = std::string{bars[entry_bar_index].timestamp},
                .exit_timestamp = std::string{next.timestamp}});
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
  result.trades = trades; // Store for debug output

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

    // Test all strategies
    auto results = std::vector<StrategyResult>{};

    results.push_back(
        backtest_strategy(bars, volume_surge_dip, "volume_surge"));
    results.back().symbol = symbol;

    results.push_back(
        backtest_strategy(bars, mean_reversion, "mean_reversion"));
    results.back().symbol = symbol;

    results.push_back(
        backtest_strategy(bars, sma_crossover<10, 20>, "sma_crossover"));
    results.back().symbol = symbol;

    results.push_back(backtest_strategy(bars, price_dip, "price_dip"));
    results.back().symbol = symbol;

    results.push_back(
        backtest_strategy(bars, volatility_breakout, "volatility_breakout"));
    results.back().symbol = symbol;

    results.push_back(backtest_strategy(bars, rsi_oversold, "rsi_oversold"));
    results.back().symbol = symbol;

    results.push_back(
        backtest_strategy(bars, bollinger_breakout, "bollinger_breakout"));
    results.back().symbol = symbol;

    results.push_back(
        backtest_strategy(bars, macd_crossover, "macd_crossover"));
    results.back().symbol = symbol;

    results.push_back(backtest_strategy(bars, gap_fill, "gap_fill"));
    results.back().symbol = symbol;

    results.push_back(backtest_strategy(bars, momentum, "momentum"));
    results.back().symbol = symbol;

    results.push_back(
        backtest_strategy(bars, morning_breakout, "morning_breakout"));
    results.back().symbol = symbol;

    // Mark each strategy as viable and collect ALL results (not just best)
    auto viable_count = 0;
    for (auto &r : results) {
      // Viable = win_rate >= 50% AND minimum 5 trades for statistical validity
      r.viable = (r.win_rate >= 0.50 && r.trade_count >= 5);

      if (r.trade_count > 0) {
        auto viable_marker = r.viable ? "✓" : "✗";
        std::println("    {} {} - {}: {} trades, {:.1f}% win, {:.2f}% avg "
                     "profit",
                     viable_marker, symbol, r.strategy_name, r.trade_count,
                     r.win_rate * 100.0, r.avg_profit * 100.0);

        // Show per-trade breakdown
        for (const auto &t : r.trades) {
          std::println("      ${:.2f} → ${:.2f} ({:+.2f}%, {}, {} bars)",
                       t.entry_price, t.exit_price, t.profit_pct * 100.0,
                       exit_reason_str(t.reason), t.duration_bars);
        }

        if (r.viable)
          viable_count++;

        // Add ALL results with trades to output (entries module will filter
        // by viable flag)
        all_results.push_back(r);
      }
    }

    if (viable_count > 0) {
      std::println("✓ {} - {} viable strateg{}", symbol, viable_count,
                   viable_count == 1 ? "y" : "ies");
    } else {
      std::println("✗ {} - no viable strategies (none met ≥50% win rate with "
                   "≥5 trades)",
                   symbol);
    }
  }

  std::println("");

  // Count viable vs non-viable
  auto viable_count = std::ranges::count_if(
      all_results, [](const auto &r) { return r.viable; });
  std::println("Strategy/symbol combinations:");
  std::println("  Total tested: {}", all_results.size());
  std::println("  Viable (≥50% win, ≥5 trades): {}", viable_count);
  std::println("  Non-viable: {}", all_results.size() - viable_count);

  // Sort by: symbol (alphabetical) → viable (true first) → win_rate
  // (descending) This ensures entries module sees viable strategies first for
  // each symbol
  std::ranges::sort(all_results, [](const auto &a, const auto &b) {
    if (a.symbol != b.symbol)
      return a.symbol < b.symbol; // Alphabetical by symbol
    if (a.viable != b.viable)
      return a.viable > b.viable;   // Viable first
    return a.win_rate > b.win_rate; // Higher win rate first
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
      "viable": {},
      "min_duration_bars": {},
      "max_duration_bars": {},
      "first_timestamp": "{}",
      "last_timestamp": "{}",
      "trades": [
)",
        rec.symbol, rec.strategy_name, rec.win_rate, rec.avg_profit,
        rec.trade_count, rec.viable ? "true" : "false", rec.min_duration_bars,
        rec.max_duration_bars, rec.first_timestamp, rec.last_timestamp);

    // Export per-trade details
    for (auto j = 0uz; j < rec.trades.size(); ++j) {
      const auto &t = rec.trades[j];
      auto trade_sep = j + 1 < rec.trades.size() ? "," : "";
      ofs << std::format(
          R"(        {{"entry": {:.2f}, "exit": {:.2f}, "profit_pct": {:.4f}, "reason": "{}", "duration": {}}}{}
)",
          t.entry_price, t.exit_price, t.profit_pct, exit_reason_str(t.reason),
          t.duration_bars, trade_sep);
    }

    ofs << std::format("      ]\n    }}{}\n", sep);
  }

  ofs << "]}\n";

  std::println("Wrote {}", output_file.string());

  return 0;
}
