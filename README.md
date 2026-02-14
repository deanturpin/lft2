# Low Frequency Trader

LFT is a fully automated algorithmic trading platform, written in the latest C++ (26) and designed from the ground up to be `constexpr`, thereby allowing compile-time unit testing and guaranteed memory safety.

## Constexpr-first architecture

All core trading logic is implemented as `constexpr` functions:

- Entry strategies (volume surge, mean reversion, SMA crossover)
- Exit management (take profit, stop loss, trailing stops)
- Price bar validation and indicator calculations
- Compile-time unit tests with `static_assert`

This approach provides:

- **Guaranteed correctness** - Strategies are validated at compile time
- **Zero runtime overhead** - Calculations can be performed during compilation
- **Memory safety** - No dynamic allocation in critical paths
- **Reproducibility** - Identical behaviour between backtest and live trading

### Non-standard library (nstd.h)

When standard library functions aren't yet `constexpr`, we provide our own implementations in the `nstd` namespace:

- `nstd::sqrt()` - Newton-Raphson constexpr square root (replaces `std::sqrt` until C++26)
- Easy migration path: simple find/replace `nstd::` → `std::` when upgrading C++ versions
- All implementations include compile-time unit tests

## Project status

This is a total redesign of the original [LFT project](https://github.com/deanturpin/lft), which will continue to run in the background whilst this second revision is being developed.

LFTv1 was a good proof of concept, but when you get into the details of why a trade happened it was quite hard to reason about. So this time we'll get all our strategies solid up front in unit tests and ensure that backtest and live behaviours are indistinguishable.

## The main event loop

1. Calibration (hourly) - recalculate indicators, update parameters, refresh state
1. Exit management (every 5min bar) - check existing positions for exit signals
1. Entry signals (every 5min bar) - scan for new opportunities

## Avoid conditionals

```cpp
while (not eod){
    risk_off = test_entries();
    // ...
}
```

### To be done

- [ ] Compile time strategy implementation
- [ ] Alpaca exchange integration
- [ ] GitHub Actions pipeline (rather than VPS)
