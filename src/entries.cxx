#include "bar.h"
#include "entry.h"
#include "fix.h"
#include "json.h"
#include "market.h"
#include <chrono>
#include <fstream>
#include <print>
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
  auto ifs = std::ifstream{"docs/strategies.json"};
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
                       std::string{json_string(obj, "recommended_strategy")}};

    if (!c.symbol.empty() && !c.strategy.empty())
      candidates.push_back(c);

    pos = obj_end + 1;
  }

  return candidates;
}

// Load account balance
AccountInfo load_account_info() {
  auto ifs = std::ifstream{"docs/account.json"};
  if (!ifs)
    return {};

  auto content = std::string{std::istreambuf_iterator<char>(ifs), {}};
  auto obj = std::string_view{content};

  return {json_number(obj, "cash"), json_number(obj, "portfolio_value")};
}

// Load existing positions to avoid duplicates
std::vector<std::string> load_existing_symbols() {
  auto ifs = std::ifstream{"docs/positions.json"};
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

  // Load candidates
  auto candidates = load_candidates();
  if (candidates.empty()) {
    std::println("No candidates to evaluate");
    auto ofs = std::ofstream{"docs/buy.fix"};
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
    std::println("   Current price: ${:.2f}", latest_price);

    // Skip if market is closed or in risk-off window (first hour / last 30 min)
    if (market::risk_off(bars.back().timestamp)) {
      std::println("   ⏭️  Risk-off period (too close to open/close)");
      continue;
    }

    // Check entry signal using recommended strategy
    auto should_enter = false;

    if (candidate.strategy == "volume_surge_dip")
      should_enter = volume_surge_dip(bars);
    else if (candidate.strategy == "mean_reversion")
      should_enter = mean_reversion(bars);
    else if (candidate.strategy == "sma_crossover")
      should_enter = sma_crossover(bars);
    else {
      std::println("   ⚠️  Unknown strategy: {}", candidate.strategy);
      continue;
    }

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

    // Generate FIX buy order
    auto order_id = std::format(
        "ENTRY_{}_{}_{}", candidate.symbol, seq_num,
        std::chrono::system_clock::now().time_since_epoch().count());

    buy_orders.push_back(fix::new_order_single(
        order_id, candidate.symbol, fix::SIDE_BUY, shares, seq_num,
        fix::ORD_TYPE_MARKET, 0.0, candidate.strategy));
    seq_num++;

    // Deduct from available cash
    account.cash -= order_value;
  }

  // Write buy.fix file
  if (!buy_orders.empty()) {
    auto ofs = std::ofstream{"docs/buy.fix"};
    for (const auto &order : buy_orders)
      ofs << order;

    std::println("\n✓ Generated {} buy order(s) in docs/buy.fix",
                 buy_orders.size());
  } else {
    std::println("\n✓ No entry signals");

    // Create empty file to indicate module ran
    auto ofs = std::ofstream{"docs/buy.fix"};
  }

  std::println("Remaining cash: ${:.2f}", account.cash);
  return 0;
}
