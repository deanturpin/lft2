#include "alpaca.h"
#include "bar.h"
#include "exit.h"
#include "fix.h"
#include "json.h"
#include "market.h"
#include "params.h"
#include "paths.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <print>
#include <string>
#include <vector>

// Parse positions.json from account module
std::vector<alpaca_position> load_positions() {
  auto ifs = std::ifstream{paths::positions};
  if (!ifs)
    return {};

  auto content = std::string{std::istreambuf_iterator<char>(ifs), {}};
  auto positions = std::vector<alpaca_position>{};

  json_foreach_object(content, [&](std::string_view obj) {
    positions.push_back({
        .symbol = std::string{json_string(obj, "symbol")},
        .qty = json_number(obj, "qty"),
        .avg_entry_price = json_number(obj, "avg_entry_price"),
        .side = std::string{json_string(obj, "side")},
        .client_order_id = std::string{json_string(obj, "client_order_id")},
    });
  });

  return positions;
}

int main() {
  std::println("Low Frequency Trader v2 - Exit Module\n");

  // Open sell.fix immediately — heartbeat confirms exits ran, truncates stale
  // data
  {
    std::ofstream{paths::sell_fix} << fix::heartbeat("exits");
  }

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

    std::println("   Latest bar: {} (${:.2f}, {:+.2f}%)",
                 std::string{bars.back().timestamp}, latest_price, profit_pct);

    // Check exit conditions
    auto should_exit = false;
    auto exit_reason = std::string{};

    // Force exit during risk-off period (last 45 min of trading day)
    // NOTE: Alpaca free tier has 15-minute delayed data, so this check may not
    // trigger until after market close. This will be resolved when upgrading to
    // paid real-time data ($99/month). For now, manual monitoring recommended
    // near EOD.
    auto is_risk_off = market::risk_off(bars.back().timestamp);
    if (is_risk_off) {
      should_exit = true;
      exit_reason = "risk_off_liquidation";
      std::println("⚠️  Risk-off period - liquidating at {}",
                   std::string{bars.back().timestamp});
    }
    // Check normal exit conditions using our exit logic
    else {
      // Create position using shared params from params.h
      auto levels = calculate_levels(pos.avg_entry_price, default_params);

      // Calculate proper trailing stop: track 1% below peak price
      // Scan all bars since entry to find peak, then set trailing stop below it
      auto peak_price = pos.avg_entry_price;
      for (const auto &b : bars) {
        if (b.close > peak_price)
          peak_price = b.close;
      }

      // Trailing stop: 1% below peak (only active if peak > entry)
      auto trailing_stop_price = peak_price * (1.0 - default_params.trailing_stop_pct);

      // Don't let trailing stop fall below initial stop loss
      if (trailing_stop_price < levels.stop_loss)
        trailing_stop_price = levels.stop_loss;

      auto mock_position = position{
          .entry_price = pos.avg_entry_price,
          .take_profit = levels.take_profit,
          .stop_loss = levels.stop_loss,
          .trailing_stop = trailing_stop_price,
      };

      // Debug: show exit levels
      std::println("   Exit levels: TP=${:.2f}, SL=${:.2f}, Trail=${:.2f} (peak=${:.2f})",
                   levels.take_profit, levels.stop_loss, trailing_stop_price, peak_price);

      // Check exit strategy
      auto exit_check = check_exit(mock_position, bars.back());
      if (exit_check != exit_reason::none) {
        should_exit = true;

        // Use the specific exit reason from check_exit
        switch (exit_check) {
        case exit_reason::take_profit:
          exit_reason = "take_profit";
          break;
        case exit_reason::stop_loss:
          exit_reason = "stop_loss";
          break;
        case exit_reason::trailing_stop:
          exit_reason = "trailing_stop";
          break;
        default:
          exit_reason = "unknown";
        }
      }
    }

    if (should_exit) {
      std::println("   ✅ Exit signal: {}", exit_reason);

      // Build exit order ID with exit reason so dashboard can display it
      // Format: EXIT_SYMBOL_REASON_timestamp
      // This goes in client_order_id which Alpaca persists and returns in API
      auto order_id = std::format(
          "EXIT_{}_{}_{}",
          pos.symbol,
          exit_reason,
          std::chrono::system_clock::now().time_since_epoch().count());

      // Use IOC (Immediate-Or-Cancel) for risk-off to prevent overnight holds
      // Use DAY for normal exits (take profit, stop loss, trailing stop)
      auto tif = is_risk_off ? fix::TIME_IN_FORCE_IOC : fix::TIME_IN_FORCE_DAY;

      sell_orders.push_back(fix::new_order_single(
          order_id, pos.symbol, fix::SIDE_SELL, static_cast<int>(pos.qty),
          seq_num, fix::ORD_TYPE_MARKET, 0.0, exit_reason, tif));
      seq_num++;
    } else {
      std::println("   ⏭️  No exit signal - holding position");
    }
  }

  // Write sell.fix — heartbeat always first so execute knows the module ran
  auto ofs = std::ofstream{paths::sell_fix};
  ofs << fix::heartbeat(std::format("{} sell order(s)", sell_orders.size()));
  for (const auto &order : sell_orders)
    ofs << order;

  std::println("\n✓ Generated {} sell order(s) in docs/sell.fix",
               sell_orders.size());

  return 0;
}
