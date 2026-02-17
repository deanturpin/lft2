#pragma once
#include <string>
#include <string_view>

// Centralised file paths for the JSON pipeline.
// All modules read/write to docs/ so the Svelte dashboard and GitHub Pages
// deployment can pick them up.

namespace paths {

using namespace std::string_view_literals;

// Output root — all pipeline files live here for GitHub Pages pickup
constexpr auto root = "docs/"sv;

// Helper to build a path from root at compile time
constexpr std::string path(std::string_view name) {
  return std::string{root} + std::string{name};
}

const auto strategies = path("strategies.json");
const auto candidates = path("candidates.json");
const auto account    = path("account.json");
const auto positions  = path("positions.json");
const auto signals    = path("signals.json");
const auto buy_fix    = path("buy.fix");
const auto sell_fix   = path("sell.fix");

// Correctness guaranteed structurally — all paths are built via path()

// Per-symbol bar data written by the fetch module
constexpr std::string bars(std::string_view symbol) {
  return std::string{root} + "bars/" + std::string{symbol} + ".json";
}

static_assert(bars("AAPL") == "docs/bars/AAPL.json");
static_assert(bars("TSLA") == "docs/bars/TSLA.json");

} // namespace paths
