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
};

// Load recommended candidates from strategies.json
std::vector<Candidate> load_candidates() {
  auto ifs = std::ifstream{paths::strategies};
  if (!ifs)
    return {};

  auto content = std::string{std::istreambuf_iterator<char>(ifs), {}};
  auto candidates = std::vector<Candidate>{};

  auto rec_start = content.find("\"recommendations\"");
  if (rec_start == std::string::npos)
    return {};

  auto pos = content.find('[', rec_start) + 1;
  while (pos < content.size()) {
    auto obj_start = content.find('{', pos);
    if (obj_start == std::string::npos)
      break;
    auto obj_end = content.find('}', obj_start);
    if (obj_end == std::string::npos)
      break;

    auto obj = std::string_view{content}.substr(obj_start + 1,
                                                obj_end - obj_start - 1);
    auto c = Candidate{std::string{json_string(obj, "symbol")},
                       std::string{json_string(obj, "strategy")}};

    if (!c.symbol.empty() && !c.strategy.empty())
      candidates.push_back(c);

    pos = obj_end + 1;
  }

  return candidates;
}

// Load account balance
AccountInfo load_account_info() {
  auto ifs = std::ifstream{paths::account};
  if (!ifs)
    return {};

  auto content = std::string{std::istreambuf_iterator<char>(ifs), {}};
  auto obj = std::string_view{content};

  return {json_number(obj, "cash"), json_number(obj, "portfolio_value")};
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

  // Load account info
  auto account = load_account_info();
  std::println("\nAccount Balance:");
  std::println("  Cash: ${:.2f}", account.cash);
  std::println("  Portfolio Value: ${:.2f}", account.portfolio_value);

  // Calculate position size (2% of portfolio)
  auto position_size = account.portfolio_value * 0.02;
  std::println("  Position Size (2%): ${:.2f}", position_size);

  // Load existing positions to avoid duplicates
  auto existing_symbols = load_existing_symbols();
  std::println("\nCurrently holding {} position(s)", existing_symbols.size());

  // Collect buy orders
  auto buy_orders = std::vector<std::string>{};
  auto seq_num = 1;

  for (const auto &candidate : candidates) {
    std::println("\n📊 Evaluating {}", candidate.symbol);
    std::println("   Strategy: {}", candidate.strategy);

    // Skip if already holding
    if (std::ranges::find(existing_symbols, candidate.symbol) !=
        existing_symbols.end()) {
      std::println("   ⏭️  Already holding position");
      continue;
    }

    // Load bars
    auto bars = load_bars(candidate.symbol);
    if (bars.size() < 25) {
      std::println("   ⚠️  Insufficient data ({} bars)", bars.size());
      continue;
    }

    auto latest_price = bars.back().close;
    auto first_ts = bars.front().timestamp;
    auto last_ts = bars.back().timestamp;
    std::println("   Current price: ${:.2f}  bars: {} → {}", latest_price,
                 first_ts, last_ts);

    // During market hours, skip if latest bar is more than 10 minutes old —
    // stale data produces nonsense signals. Outside hours the last bar will
    // always be old, so the check is meaningless and skipped.
    if (market::market_open(last_ts)) {
      auto now = std::chrono::system_clock::now();
      auto bar_time = std::chrono::sys_seconds{};
      std::istringstream ss{std::string{last_ts}};
      std::chrono::from_stream(ss, "%Y-%m-%dT%H:%M:%SZ", bar_time);
      auto age =
          std::chrono::duration_cast<std::chrono::minutes>(now - bar_time);
      if (age > std::chrono::minutes{10}) {
        std::println("   ⏭️  Stale data ({} minutes old), skipping",
                     age.count());
        continue;
      }
    }

    // Skip if market is closed or in risk-off window (first hour / last 30 min)
    if (market::risk_off(bars.back().timestamp)) {
      std::println("   ⏭️  Risk-off period (too close to open/close)");
      continue;
    }

    // Check entry signal using recommended strategy
    auto should_enter = dispatch_entry(candidate.strategy, bars);
    if (!should_enter) {
      std::println("   ⏭️  No entry signal");
      continue;
    }

    // Calculate shares
    auto shares = static_cast<int>(position_size / latest_price);
    if (shares < 1) {
      std::println("   ❌ Position too small (${:.2f} / ${:.2f} = {} shares)",
                   position_size, latest_price, shares);
      continue;
    }

    auto order_value = shares * latest_price;
    if (order_value > account.cash) {
      std::println("   ❌ Insufficient cash (need ${:.2f}, have ${:.2f})",
                   order_value, account.cash);
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
        "{}_{}_tp{}_sl{}_tsl{}_{}", candidate.symbol, candidate.strategy,
        static_cast<int>(default_params.take_profit_pct * 100),
        static_cast<int>(default_params.stop_loss_pct * 100),
        static_cast<int>(default_params.trailing_stop_pct * 100), now_ts);

    buy_orders.push_back(fix::new_order_single(
        order_id, candidate.symbol, fix::SIDE_BUY, shares, seq_num,
        fix::ORD_TYPE_MARKET, 0.0, candidate.strategy));
    seq_num++;

    // Deduct from available cash
    account.cash -= order_value;
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
