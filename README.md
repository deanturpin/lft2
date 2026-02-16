# Low Frequency Trader

LFT is a fully automated algorithmic trading platform, written in the latest C++ (26) and designed from the ground up to be `constexpr`, thereby allowing compile-time unit testing and guaranteed memory safety.

**Live Dashboard**: <https://lft.turpin.dev>

**Deployment Guide**: [DEPLOYMENT.md](DEPLOYMENT.md)

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

LFTv1 was a good proof-of-concept, but when you get into the details of _why_ a trade happened it was quite hard to reason about. So this time we'll get all our strategies solid up front in unit tests and ensure that backtest and live behaviours are indistinguishable.

## System architecture

The trading system is decomposed into focused modules:

### Infrastructure

- **Frontend**: Svelte 5 SPA with Chart.js for portfolio visualisation
- **API Layer**: Cloudflare Workers for serverless API endpoints
- **Hosting**: Cloudflare Pages with automatic GitHub deployments
- **Trading Engine**: C++26 application running on Fasthosts VPS
- **Data Source**: Alpaca paper trading API (transitioning to live trading)

### Modules

|Module|Description|Input Data|Output Data|Frequency|Tech Stack|
|------|-----------|----------|-----------|---------|----------|
|**test**|Static unit testing of all trading logic|Source code|Compiler errors/success|On every build|C++26, constexpr, static_assert|
|**profile**|Performance analysis of core trading logic|`prices/*.h` (precompiled historical data)|Performance metrics, backtest results, `stats.json`, `top_stocks.json`|On demand (development)|C++26, CMake, gprof, JSON|
|**fetch**|Retrieve latest market data|Alpaca API|`snapshots.json` (latest bars for selected symbols)|Every 5min (market hours)|Go, Alpaca API, JSON|
|**filter**|Identify candidate stocks with suitable price action|Live snapshots for all assets (from Alpaca)|`candidates.json` (filtered stock symbols)|Continuous (every 5min)|Go, JSON|
|**backtest**|Daily strategy evaluation on live data|`candidates.json`, live historical bars|`strategies.json` (profitable strategy/symbol pairs)|Daily (pre-market)|C++, JSON|
|**evaluate**|Generate trading signals from market data|`snapshots.json`, `strategies.json`, `positions.json`|`signals.json` (entry/exit signals)|Every 5min (market hours)|C++26, JSON|
|**execute**|Place orders based on signals|`signals.json`, Alpaca account state|`positions.json` (updated positions), order confirmations|Every 5min (market hours)|Go, Alpaca API, JSON|
|**account**|Account monitoring and data aggregation (Cloudflare Worker)|Alpaca API|Real-time dashboard data via `/api/dashboard` and `/api/history`|Every 60s (frontend poll)|JavaScript, Cloudflare Workers|
|**website**|Interactive dashboard and performance visualisation|API endpoints|Multi-page SPA with live charts and navigation|Real-time updates|Svelte 5, Chart.js, svelte-spa-router|

### Data flow

```text
┌─────────────────────────────────────────────────────────────┐
│  Daily (pre-market) - VPS                                   │
│  filter → backtest → strategies.json                        │
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│  Every 5min (market hours) - VPS                            │
│  fetch → evaluate → execute → positions.json                │
│           ↑          ↑                                       │
│           │          └── strategies.json                    │
│           └────────────── positions.json                    │
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│  Real-time dashboard - Cloudflare                           │
│  Browser → Cloudflare Pages (Svelte)                        │
│              ↓                                               │
│         Cloudflare Worker (/api/*)                          │
│              ↓                                               │
│         Alpaca API → JSON → Browser (auto-refresh 60s)     │
└─────────────────────────────────────────────────────────────┘
```

### Design principles

- **JSON as lingua franca** - All modules communicate via JSON, enabling language flexibility
- **Platform isolation** - Alpaca-specific code contained in fetch/execute/account modules
- **Pure trading logic** - Core strategies (evaluate) are platform-agnostic and testable
- **Incremental builds** - Each module can be developed/tested independently
- **Edge-first delivery** - Cloudflare's global network for sub-100ms dashboard response times
- **Right tool for the job** - C++ for constexpr guarantees, Cloudflare Workers for serverless APIs, Svelte for reactive UI

### Technology rationale

**C++26 modules** (test, profile, evaluate - running on Fasthosts VPS):

- Constexpr guarantees ensure trading logic correctness at compile-time
- Zero runtime overhead for strategy calculations
- Static assertions catch bugs before deployment
- VPS deployment for live trading execution

**Go modules** (filter, backtest, fetch, execute - planned):

- Native JSON marshaling with struct tags
- Built-in HTTP client for Alpaca API
- Single binary deployment (no runtime dependencies)
- Excellent error handling for API failures
- Goroutines for concurrent symbol processing

**Cloudflare Workers** (account API):

- Serverless API endpoints with zero cold starts
- Secret management for Alpaca API credentials
- Global edge network for low-latency responses
- Automatic scaling with zero infrastructure management

**Svelte 5 website** (deployed to Cloudflare Pages):

- Reactive components with automatic updates
- Minimal bundle size (compiles to vanilla JS)
- Chart.js for portfolio history visualisation
- svelte-spa-router for client-side navigation
- Vite for fast development and optimised builds
- Automatic deployments on git push

### Implementation status

- [x] test - Compile-time unit tests operational
- [x] profile - Backtest with historical data working
- [x] account - Cloudflare Worker providing `/api/dashboard` and `/api/history` endpoints
- [x] website - Svelte 5 SPA with real-time dashboard, portfolio chart, and navigation
- [ ] filter - Not yet implemented (Go)
- [ ] backtest - Not yet implemented (Go, daily strategy evaluation)
- [ ] fetch - C++ stub exists, needs rewrite in Go
- [ ] evaluate - C++ stub exists, needs signal generation logic
- [ ] execute - C++ stub exists, needs rewrite in Go
