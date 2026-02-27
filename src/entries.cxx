#include "bar.h"
#include "entry.h"
#include "fix.h"
#include "json.h"
#include "market.h"
#include "params.h"
#include "paths.h"
#include <chrono>
#include <fstream>
#include <print>
#include <sstream>
#include <string>
#include <vector>

// Candidate from strategies.json
struct Candidate {
  std::string symbol;
  std::string strategy;
};

// Account info
struct AccountInfo {
  double cash;
  double portfolio_value;
  double buying_power;
};

// Load recommended candidates from strategies.json
std::vector<Candidate> load_candidates() {
  auto ifs = std::ifstream{paths::strategies};
  if (!ifs)
    return {};

  auto content = std::string{std::istreambuf_iterator<char>(ifs), {}};
  auto candidates = std::vector<Candidate>{};

  // Find the "recommendations" array
  auto rec_start = content.find(R"("recommendations")");
  if (rec_start == std::string::npos)
    return {};

  auto array_start = content.find('[', rec_start);
  if (array_start == std::string::npos)
    return {};

  // Parse objects in the array
  json_foreach_object(
      std::string_view{content}.substr(array_start), [&](std::string_view obj) {
        auto c = Candidate{std::string{json_string(obj, "symbol")},
                           std::string{json_string(obj, "strategy")}};
        if (!c.symbol.empty() && !c.strategy.empty())
          candidates.push_back(c);
      });

  return candidates;
}

// Load account balance
AccountInfo load_account_info() {
  auto ifs = std::ifstream{paths::account};
  if (!ifs)
    return {};

  auto content = std::string{std::istreambuf_iterator<char>(ifs), {}};
  auto obj = std::string_view{content};

  // json_number scans object content (inside braces); skip the opening '{'
  if (auto brace = obj.find('{'); brace != std::string_view::npos)
    obj.remove_prefix(brace + 1);

  return {json_number(obj, "cash"), json_number(obj, "portfolio_value"),
          json_number(obj, "buying_power")};
}

// Load existing positions to avoid duplicates
std::vector<std::string> load_existing_symbols() {
  auto ifs = std::ifstream{paths::positions};
  if (!ifs)
    return {};

  auto content = std::string{std::istreambuf_iterator<char>(ifs), {}};
  auto symbols = std::vector<std::string>{};

  auto pos = 0uz;
  while (pos < content.size()) {
    auto obj_start = content.find('{', pos);
    if (obj_start == std::string::npos)
      break;
    auto obj_end = content.find('}', obj_start);
    if (obj_end == std::string::npos)
      break;

    auto obj = std::string_view{content}.substr(obj_start + 1,
                                                obj_end - obj_start - 1);
    auto sym = json_string(obj, "symbol");
    if (!sym.empty())
      symbols.emplace_back(sym);

    pos = obj_end + 1;
  }

  return symbols;
}

int main() {
  std::println("Low Frequency Trader v2 - Entry Module\n");

  // Open buy.fix immediately — heartbeat confirms entries ran, truncates stale
  // data
  {
    std::ofstream{paths::buy_fix} << fix::heartbeat("entries");
  }

  // Load candidates
  auto candidates = load_candidates();
  if (candidates.empty()) {
    std::println("No candidates to evaluate");
    return 0;
  }

  std::println("Evaluating {} candidate(s)...", candidates.size());

  // Load account info — abort if buying_power is zero (likely a parse/API
  // failure)
  auto account = load_account_info();
  if (account.buying_power <= 0.0) {
    std::println("\n❌ ERROR: buying power is zero — docs/account.json "
                 "missing or invalid");
    std::println("   Run the account module first: make account");
    return 1;
  }
  std::println("\nAccount Balance:");
  std::println("  Cash: ${:.2f}", account.cash);
  std::println("  Portfolio Value: ${:.2f}", account.portfolio_value);
  std::println("  Buying Power: ${:.2f}", account.buying_power);

  constexpr auto max_order_value = 2000.0;

  // Load existing positions to avoid duplicates
  auto existing_symbols = load_existing_symbols();
  std::println("\nCurrently holding {} position(s)", existing_symbols.size());

  // Collect buy orders
  auto buy_orders = std::vector<std::string>{};
  auto seq_num = 1;

  std::println("\n{:<6} {:<24} {:>8}  {}", "Symbol", "Strategy", "Price",
               "Status");
  std::println("{}", std::string(60, '-'));

  for (const auto &candidate : candidates) {
    auto prefix =
        std::format("{:<6} {:<24}", candidate.symbol, candidate.strategy);

    if (std::ranges::find(existing_symbols, candidate.symbol) !=
        existing_symbols.end()) {
      std::println("{}           ⏭️  holding", prefix);
      continue;
    }

    auto bars = load_bars(candidate.symbol);
    if (bars.size() < 25) {
      std::println("{}           ⚠️  {} bars", prefix, bars.size());
      continue;
    }

    auto latest_price = bars.back().close;
    auto last_ts = bars.back().timestamp;

    // During market hours, skip if latest bar is more than 20 minutes old.
    // Alpaca free tier has a 15-minute data delay, so allow up to 20 minutes.
    if (market::market_open(last_ts)) {
      auto now = std::chrono::system_clock::now();
      auto bar_time = std::chrono::sys_seconds{};
      std::istringstream ss{std::string{last_ts}};
      std::chrono::from_stream(ss, "%Y-%m-%dT%H:%M:%SZ", bar_time);
      auto age =
          std::chrono::duration_cast<std::chrono::minutes>(now - bar_time);
      if (age > std::chrono::minutes{20}) {
        std::println("{} {:>8.2f}  ⏭️  stale ({}m)", prefix, latest_price,
                     age.count());
        continue;
      }
    }

    if (!market::market_open(last_ts)) {
      std::println("{} {:>8.2f}  ⏭️  market closed (last bar: {})", prefix,
                   latest_price, last_ts);
      continue;
    }
    if (market::risk_off(last_ts)) {
      std::println("{} {:>8.2f}  ⏭️  risk-off (last bar: {})", prefix,
                   latest_price, last_ts);
      continue;
    }

    auto should_enter = dispatch_entry(candidate.strategy, bars);
    if (!should_enter) {
      std::println("{} {:>8.2f}  ⏭️  no signal", prefix, latest_price);
      continue;
    }

    auto shares = static_cast<int>(max_order_value / latest_price);
    if (shares < 1) {
      std::println("{} {:>8.2f}  ❌ too expensive (< 1 share for ${})", prefix,
                   latest_price, static_cast<int>(max_order_value));
      continue;
    }

    auto order_value = shares * latest_price;
    if (order_value > max_order_value) {
      // This should never happen — abort loudly if it does
      std::println("❌ ABORT: order value ${:.2f} exceeds limit ${} — BUG",
                   order_value, static_cast<int>(max_order_value));
      return 1;
    }
    if (order_value > account.buying_power) {
      std::println("{} {:>8.2f}  ❌ insufficient buying power", prefix,
                   latest_price);
      continue;
    }

    std::println("   ✅ Entry signal! Buying {} shares (${:.2f})", shares,
                 order_value);

    // Generate FIX buy order — encode symbol, strategy and risk params in the
    // client_order_id (tag 11) so every field is visible in Alpaca's order
    // history without needing a separate lookup.
    // Format: AAPL_mean_reversion_tp3_sl2_tsl1_20260218T143000
    auto now_ts = std::format("{:%Y%m%dT%H%M%S}",
                              std::chrono::floor<std::chrono::seconds>(
                                  std::chrono::system_clock::now()));
    auto order_id = std::format(
        "{}_{}_tp{:.2f}_sl{:.2f}_tsl{:.2f}_{}", candidate.symbol, candidate.strategy,
        default_params.take_profit_pct * 100,
        default_params.stop_loss_pct * 100,
        default_params.trailing_stop_pct * 100, now_ts);

    buy_orders.push_back(fix::new_order_single(
        order_id, candidate.symbol, fix::SIDE_BUY, shares, seq_num,
        fix::ORD_TYPE_MARKET, 0.0, candidate.strategy));
    seq_num++;

    std::println("{} {:>8.2f}  ✅ buy {} shares (${:.2f})", prefix,
                 latest_price, shares, order_value);

    account.buying_power -= order_value;
  }

  // Write buy.fix — heartbeat always first so execute knows the module ran
  auto ofs = std::ofstream{paths::buy_fix};
  ofs << fix::heartbeat(std::format("{} buy order(s)", buy_orders.size()));
  for (const auto &order : buy_orders)
    ofs << order;

  std::println("\n✓ Generated {} buy order(s) in docs/buy.fix",
               buy_orders.size());

  std::println("Remaining cash: ${:.2f}", account.cash);
  return 0;
}
