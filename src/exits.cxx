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

// Find the most recent buy timestamp for a symbol from daily-summary.json
std::string_view find_entry_timestamp(std::string_view symbol, const std::string &activities_json) {
  // Find the activities array
  auto activities_start = activities_json.find(R"("activities")");
  if (activities_start == std::string::npos)
    return "";

  auto array_start = activities_json.find('[', activities_start);
  if (array_start == std::string::npos)
    return "";

  std::string_view latest_timestamp;

  // Parse activities array to find most recent buy for this symbol
  json_foreach_object(std::string_view{activities_json}.substr(array_start), [&](std::string_view obj) {
    auto activity_symbol = json_string(obj, "symbol");
    auto side = json_string(obj, "side");

    if (activity_symbol == symbol && side == "buy") {
      auto timestamp = json_string(obj, "transaction_time");
      if (timestamp > latest_timestamp) {
        latest_timestamp = timestamp;
      }
    }
  });

  return latest_timestamp;
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

  // Load daily-summary.json to find entry timestamps for each position
  auto activities_json = std::string{};
  {
    auto ifs = std::ifstream{paths::daily_summary};
    if (ifs) {
      activities_json = std::string{std::istreambuf_iterator<char>(ifs), {}};
    } else {
      std::println("⚠️  Warning: daily-summary.json not found - cannot determine entry timestamps");
    }
  }

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

    // Force exit during risk-off period (last 75 min of trading day, 2:45-4:00 PM ET)
    // With 15-minute delayed data, this gives ~60 minutes of real liquidation time
    // NOTE: Alpaca free tier delay means actual trigger may be later than ideal
    // Upgrading to real-time data ($99/month) would provide full 75-minute window
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
      // Only activate trailing stop after position becomes profitable
      // Find entry timestamp to only scan bars since this position was opened
      auto entry_timestamp = find_entry_timestamp(pos.symbol, activities_json);

      auto peak_price = pos.avg_entry_price;
      for (const auto &b : bars) {
        // Skip bars before this position was entered
        if (!entry_timestamp.empty() && b.timestamp < entry_timestamp)
          continue;

        if (b.close > peak_price)
          peak_price = b.close;
      }

      // Trailing stop: only activate after price rises above entry
      // Until then, only stop loss protects the position
      auto trailing_stop_price = levels.stop_loss; // Start disabled (at SL level)

      if (peak_price > pos.avg_entry_price) {
        // Position has been profitable - activate trailing stop
        trailing_stop_price = peak_price * (1.0 - default_params.trailing_stop_pct);

        // Don't let trailing stop fall below initial stop loss
        if (trailing_stop_price < levels.stop_loss)
          trailing_stop_price = levels.stop_loss;
      }

      auto mock_position = position{
          .entry_price = pos.avg_entry_price,
          .take_profit = levels.take_profit,
          .stop_loss = levels.stop_loss,
          .trailing_stop = trailing_stop_price,
      };

      // Debug: show exit levels
      auto trailing_active = peak_price > pos.avg_entry_price;
      std::println("   Exit levels: TP=${:.2f}, SL=${:.2f}, Trail=${:.2f} {} (peak=${:.2f})",
                   levels.take_profit, levels.stop_loss, trailing_stop_price,
                   trailing_active ? "ACTIVE" : "inactive", peak_price);

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

      // Fill Policy:
      // - Risk-off liquidation: IOC (Immediate-Or-Cancel) - fill within seconds or cancel
      //   * Must close before market close, can't risk overnight holds
      //   * If order doesn't fill immediately, cancel and retry on next pipeline run
      // - Normal exits: DAY (Good-Till-Day) - fill anytime before market close
      //   * Take profit, stop loss, trailing stop based on price levels
      //   * Timing within day matters less than getting target price
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
