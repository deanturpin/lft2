#pragma once
#include <string>
#include <string_view>

// Centralised file paths for the JSON pipeline.
// All modules read/write to docs/ so the Svelte dashboard and GitHub Pages
// deployment can pick them up.

namespace paths {

constexpr auto strategies = "docs/strategies.json";
constexpr auto candidates = "docs/candidates.json";
constexpr auto account = "docs/account.json";
constexpr auto positions = "docs/positions.json";
constexpr auto signals = "docs/signals.json";
constexpr auto buy_fix = "docs/buy.fix";
constexpr auto sell_fix = "docs/sell.fix";

// Per-symbol bar data written by the fetch module
constexpr std::string bars(std::string_view symbol) {
  return "docs/bars/" + std::string{symbol} + ".json";
}

static_assert(bars("AAPL") == "docs/bars/AAPL.json");
static_assert(bars("TSLA") == "docs/bars/TSLA.json");

} // namespace paths
