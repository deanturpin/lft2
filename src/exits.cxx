#include "bar.h"
#include "exit.h"
#include "fix.h"
#include "json.h"
#include "market.h"
#include "paths.h"
#include "params.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <print>
#include <string>
#include <vector>

// Position from account module
struct Position {
  std::string symbol;
  double qty;
  double avg_entry_price;
  std::string side;
};

// Parse positions.json from account module
std::vector<Position> load_positions() {
  auto ifs = std::ifstream{paths::positions};
  if (!ifs)
    return {};

  auto content = std::string{std::istreambuf_iterator<char>(ifs), {}};
  auto positions = std::vector<Position>{};

  auto pos = content.find('[');
  if (pos == std::string::npos)
    return {};

  while (pos < content.size()) {
    auto obj_start = content.find('{', pos);
    if (obj_start == std::string::npos)
      break;
    auto obj_end = content.find('}', obj_start);
    if (obj_end == std::string::npos)
      break;

    auto obj = std::string_view{content}.substr(obj_start + 1,
                                                obj_end - obj_start - 1);
    positions.push_back({
        .symbol = std::string{json_string(obj, "symbol")},
        .qty = json_number(obj, "qty"),
        .avg_entry_price = json_number(obj, "avg_entry_price"),
        .side = std::string{json_string(obj, "side")},
    });

    pos = obj_end + 1;
  }

  return positions;
}

int main() {
  std::println("Low Frequency Trader v2 - Exit Module\n");

  // Load open positions
  auto positions = load_positions();

  if (positions.empty()) {
    std::println("No open positions to check");
    return 0;
  }

  std::println("Checking {} position(s) for exit signals...", positions.size());

  // Check if we need to liquidate everything (using latest bar timestamp)
  // We'll check per-position using the bar timestamp

  // Collect sell orders
  auto sell_orders = std::vector<std::string>{};
  auto seq_num = 1;

  for (const auto &pos : positions) {
    std::println("\n📊 Checking {} ({} shares @ ${:.2f})", pos.symbol, pos.qty,
                 pos.avg_entry_price);

    // Load latest bars for this symbol
    auto bars = load_bars(pos.symbol);

    if (bars.empty()) {
      std::println("   ⚠️  No bar data available, skipping");
      continue;
    }

    auto latest_price = bars.back().close;
    auto profit_pct =
        ((latest_price - pos.avg_entry_price) / pos.avg_entry_price) * 100.0;

    std::println("   Current price: ${:.2f} ({:+.2f}%)", latest_price,
                 profit_pct);

    // Check exit conditions
    auto should_exit = false;
    auto exit_reason = std::string{};

    // Force exit during risk-off period (last 30 min of trading day)
    if (market::risk_off(bars.back().timestamp)) {
      should_exit = true;
      exit_reason = "risk_off_liquidation";
      std::println("⚠️  Risk-off period - liquidating at {}",
                   std::string{bars.back().timestamp});
    }
    // Check normal exit conditions using our exit logic
    else {
      // Create position using shared params from params.h
      auto levels = calculate_levels(pos.avg_entry_price, default_params);
      auto mock_position = position{
          .entry_price = pos.avg_entry_price,
          .take_profit = levels.take_profit,
          .stop_loss = levels.stop_loss,
          .trailing_stop = levels.trailing_stop,
      };

      // Check exit strategy
      if (is_exit(mock_position, bars.back())) {
        should_exit = true;

        // Determine which condition triggered
        if (profit_pct >= default_params.take_profit_pct * 100.0)
          exit_reason = "take_profit";
        else if (profit_pct <= -default_params.stop_loss_pct * 100.0)
          exit_reason = "stop_loss";
        else
          exit_reason = "trailing_stop";
      }
    }

    if (should_exit) {
      std::println("   ✅ Exit signal: {}", exit_reason);

      // Generate FIX sell order
      auto order_id = std::format(
          "EXIT_{}_{}_{}", pos.symbol, seq_num,
          std::chrono::system_clock::now().time_since_epoch().count());

      sell_orders.push_back(fix::new_order_single(
          order_id, pos.symbol, fix::SIDE_SELL, static_cast<int>(pos.qty),
          seq_num, fix::ORD_TYPE_MARKET, 0.0, exit_reason));
      seq_num++;
    } else {
      std::println("   ⏭️  No exit signal - holding position");
    }
  }

  // Write sell.fix file
  if (!sell_orders.empty()) {
    auto ofs = std::ofstream{paths::sell_fix};
    for (const auto &order : sell_orders)
      ofs << order;

    std::println("\n✓ Generated {} sell order(s) in docs/sell.fix",
                 sell_orders.size());
  } else {
    std::println("\n✓ No exit signals - all positions held");

    // Create empty file to indicate module ran
    auto ofs = std::ofstream{paths::sell_fix};
  }

  return 0;
}
