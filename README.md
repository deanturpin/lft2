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

## System architecture

The trading system is decomposed into focused modules that communicate via JSON:

|Module|Description|Input Data|Output Data|Frequency|Tech Stack|
|------|-----------|----------|-----------|---------|----------|
|**test**|Static unit testing of all trading logic|Source code|Compiler errors/success|On every build|C++26, constexpr, static_assert|
|**profile**|Performance analysis of core trading logic|`prices/*.h` (precompiled historical data)|Performance metrics, backtest results, `stats.json`, `top_stocks.json`|On demand (development)|C++26, CMake, gprof, JSON|
|**filter**|Identify candidate stocks with suitable price action|Live snapshots for all assets (from Alpaca)|`candidates.json` (filtered stock symbols)|Continuous (every 5min)|C++/Python/JS (TBD), Alpaca API, JSON|
|**backtest**|Daily strategy evaluation on live data|`candidates.json`, live historical bars|`strategies.json` (profitable strategy/symbol pairs)|Daily (pre-market)|C++26, Alpaca API, JSON|
|**fetch**|Retrieve latest market data|Alpaca API|`snapshots.json` (latest bars for selected symbols)|Every 5min (market hours)|C++/Python (TBD), Alpaca API, JSON|
|**evaluate**|Generate trading signals from market data|`snapshots.json`, `strategies.json`, `positions.json`|`signals.json` (entry/exit signals)|Every 5min (market hours)|C++26, JSON|
|**execute**|Place orders based on signals|`signals.json`, Alpaca account state|`positions.json` (updated positions), order confirmations|Every 5min (market hours)|C++/Python (TBD), Alpaca API, JSON|
|**account**|Account monitoring and visualisation|Alpaca account history|`account.json`, balance graphs (HTML)|Hourly|Python/JS (TBD), Alpaca API, Charts.js, GitHub Pages|

### Data flow

```text
┌─────────────────────────────────────────────────────────────┐
│  Daily (pre-market)                                         │
│  filter → backtest → strategies.json                        │
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│  Every 5min (market hours)                                  │
│  fetch → evaluate → execute → positions.json                │
│           ↑          ↑                                       │
│           │          └── strategies.json                    │
│           └────────────── positions.json                    │
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│  Hourly                                                      │
│  account → account.json → GitHub Pages                      │
└─────────────────────────────────────────────────────────────┘
```

### Design principles

- **JSON as lingua franca** - All modules communicate via JSON files, enabling language flexibility
- **Platform isolation** - Alpaca-specific code contained in fetch/execute/account modules
- **Pure trading logic** - Core strategies (evaluate) are platform-agnostic and testable
- **Incremental builds** - Each module can be developed/tested independently
- **Web-first output** - All data formatted for GitHub Pages visualisation

### Implementation status

- [x] test - Compile-time unit tests operational
- [x] profile - Backtest with historical data working, publishing to Pages
- [ ] filter - Not yet implemented (language choice pending)
- [ ] backtest - Not yet implemented (daily strategy evaluation)
- [ ] fetch - Stub created, Alpaca integration pending
- [ ] evaluate - Stub created, signal generation pending
- [ ] execute - Stub created, order execution pending
- [ ] account - Not yet implemented (monitoring dashboard)
