#pragma once
#include "json.h"
#include <span>

// Simple moving average crossover entry strategy
// Evaluated every 5 minutes on price history
// Returns true if should enter long position (short crosses above long)
template<size_t short_period = 10, size_t long_period = 20>
constexpr bool is_entry(std::span<const bar> history) {
    constexpr auto min_bars = long_period + 1;

    if (history.size() < min_bars)
        return false;

    // Validate all bars in the required window
    for (auto i = 0uz; i < min_bars; ++i)
        if (!is_valid(history[history.size() - 1 - i]))
            return false;

    // Calculate current SMAs
    auto short_sma = 0.0;
    auto long_sma = 0.0;

    for (auto i = 0uz; i < short_period; ++i)
        short_sma += history[history.size() - 1 - i].close;
    short_sma /= short_period;

    for (auto i = 0uz; i < long_period; ++i)
        long_sma += history[history.size() - 1 - i].close;
    long_sma /= long_period;

    // Calculate previous SMAs for crossover detection
    auto prev_short_sma = 0.0;
    auto prev_long_sma = 0.0;

    for (auto i = 1uz; i <= short_period; ++i)
        prev_short_sma += history[history.size() - 1 - i].close;
    prev_short_sma /= short_period;

    for (auto i = 1uz; i <= long_period; ++i)
        prev_long_sma += history[history.size() - 1 - i].close;
    prev_long_sma /= long_period;

    // Detect bullish crossover (short crosses above long)
    return prev_short_sma <= prev_long_sma && short_sma > long_sma;
}

// Unit tests
namespace {
    // Test: insufficient history returns false
    static_assert([] {
        auto bars = std::array<bar, 5>{};
        for (auto i = 0uz; i < bars.size(); ++i)
            bars[i] = bar{.close = 100.0, .high = 101.0, .low = 99.0, .open = 100.0, .vwap = 100.0,
                          .volume = 1000, .num_trades = 50, .timestamp = "2025-01-01T10:00:00Z"};
        return !is_entry<10, 20>(bars);
    }());

    // Test: no crossover returns false
    static_assert([] {
        auto bars = std::array<bar, 25>{};
        for (auto i = 0uz; i < bars.size(); ++i)
            bars[i] = bar{.close = 100.0, .high = 101.0, .low = 99.0, .open = 100.0, .vwap = 100.0,
                          .volume = 1000, .num_trades = 50, .timestamp = "2025-01-01T10:00:00Z"};
        return !is_entry<10, 20>(bars);
    }());
}
