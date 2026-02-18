#include "entry.h"

// Maps strategy name strings to entry functions — the single place to update
// when adding a new strategy. Both entries.cxx and evaluate.cxx call this.
bool dispatch_entry(std::string_view strategy, std::span<const bar> history) {
  if (strategy == "volume_surge")
    return volume_surge_dip(history);
  if (strategy == "mean_reversion")
    return mean_reversion(history);
  if (strategy == "sma_crossover")
    return sma_crossover(history);
  if (strategy == "price_dip")
    return price_dip(history);
  if (strategy == "volatility_breakout")
    return volatility_breakout(history);
  return false;
}
