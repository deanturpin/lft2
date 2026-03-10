# LFT2 Technology Stack

> "Is this data from before lunch or after lunch? Hard to tell!"

Complete technology stack for the Low Frequency Trader algorithmic trading platform.

## Core Languages

### C++26 (Trading Logic & Analysis)

**Compiler:** gcc-15 (first compiler with full C++26 support)

**Purpose:** Constexpr-validated trading strategies and backtesting engine

**Modules:**
- `backtest` - Strategy backtesting engine
- `entries` - Entry signal generation
- `exits` - Exit signal generation and position management
- `evaluate` - Real-time signal evaluation
- `test` - Compile-time unit tests

**Key C++ Features Used:**
- `constexpr` functions - Compile-time strategy validation
- `static_assert` - Unit tests run at compile time
- `std::println` (C++23) - Modern formatted output
- `std::span` (C++20) - Zero-copy array views
- `std::string_view` (C++17) - Non-owning string references
- `uz` suffix (C++20) - Type-safe size_t literals

**Build System:** CMake 3.28+

**Compiler Flags:**
```bash
-std=c++26 -Wall -Wextra -Wpedantic -Werror
-O0 -g2 -fno-omit-frame-pointer --coverage -march=native
```

### Go 1.21+ (API Integration & Data Fetching)

**Purpose:** Interface with Alpaca Markets API, data pipeline orchestration

**Modules:**
- `cmd/account` - Fetch account balance and positions
- `cmd/fetch` - Retrieve market snapshots (5-minute bars)
- `cmd/execute` - Place buy/sell orders
- `cmd/filter` - Identify candidate stocks from watchlist
- `cmd/backtest` - Daily strategy evaluation

**Dependencies:**
- Custom `internal/alpaca` package for API client
- Standard library only (no external dependencies)

**Build System:** Go modules with workspace (`go.work`)

### JavaScript/TypeScript (Dashboard & API)

**Runtime:** Node.js 20+

**Purpose:** User interface and edge API

#### Frontend Stack

**Framework:** Svelte 5.0

- Reactive component framework with minimal runtime overhead
- Compiles to vanilla JavaScript (no virtual DOM)
- Significantly smaller bundle size vs React/Vue
- Built-in reactivity without hooks or state management libraries

**Build Tool:** Vite 5.0

- Lightning-fast ES modules-based dev server
- Hot Module Replacement (HMR) for instant updates
- Optimised production builds with tree-shaking and code-splitting
- Native ES modules support (no bundler in dev mode)

**Router:** svelte-spa-router 4.0

- Hash-based client-side routing (`#/`, `#/about`)
- No server-side configuration needed
- Works seamlessly with GitHub Pages static hosting

**Visualisation:** Chart.js 4.5

- Interactive performance charts
- Strategy win rates and profit visualisations
- Lightweight canvas-based rendering

**Build Output:**

- Static HTML/CSS/JS to `web/public/`
- Copied to `docs/` for GitHub Pages deployment
- Single-page application with code-splitting
- Typical bundle size: ~270KB JS (92KB gzipped)

#### Development Workflow

**Dev Server:**

```bash
cd web && npm run dev
# Runs on http://localhost:5173
# HMR updates in <100ms
```

**Production Build:**

```bash
cd web && npm run build
# Outputs to web/public/
# Optimised, minified, tree-shaken
```

**Preview:**

```bash
cd web && npm run preview
# Test production build locally
```

#### Edge API (Cloudflare Workers)

**Runtime:** V8 isolates (not Node.js)

- Runs JavaScript at Cloudflare's edge network
- 300+ locations worldwide
- Cold start: <5ms, warm: <1ms

**Purpose:**

- Proxy Alpaca API requests from dashboard
- Hide API credentials from client-side code
- Add CORS headers for browser access
- Rate limiting and request validation

**Configuration:** wrangler.toml

- Route pattern: `lft.turpin.dev/api/*`
- Secrets managed via `npx wrangler secret put`
- Auto-deploy on push via Cloudflare integration

**Performance:**

- Sub-100ms response times globally
- Zero cold starts (always-warm isolates)
- Automatic global load balancing

## Infrastructure & Deployment

### Version Control & CI/CD

**Git:** GitHub repository (private)

**CI/CD:** GitHub Actions (`.github/workflows/pages.yml`)

**Pipeline Schedule:**
- Every 5 minutes during market hours (09:30-17:00 ET / 14:30-22:00 UTC)
- Manual trigger via workflow_dispatch

**Build Environment:** Ubuntu 26.04 container (ships gcc-15 by default)

### Hosting & Deployment

**Dashboard:** Cloudflare Pages
- Primary domain: `lft.turpin.dev`
- Auto-deploy on git push to main
- Global CDN with edge caching

**API Documentation:** GitHub Pages
- Doxygen-generated C++ documentation
- Published to `deanturpin.github.io/lft2/`
- Auto-deployed via GitHub Actions

**Edge API:** Cloudflare Workers

- Global edge network (300+ locations)
- Runs at `lft.turpin.dev/api/*`

**Trading Execution:** GitHub Actions

- Automated pipeline runs every 5 minutes during market hours
- Direct access to Alpaca API via repository secrets
- No VPS required - fully serverless architecture

### Trading Platform

**Broker:** Alpaca Markets

**Account Type:** Paper trading (sandbox)

**API Access:**
- Trading API: Account, positions, orders
- Data API: 5-minute bars, snapshots, quotes

**Data Frequency:** 5-minute bars (15-minute delay on free tier)

**Market Hours:** NYSE 09:30-16:00 ET (14:30-21:00 UTC in EST)

## Data Flow & Architecture

### Pipeline Architecture

```text
┌─────────────────────────────────────────────────────────────┐
│ Daily Pipeline (Pre-market)                                 │
├─────────────────────────────────────────────────────────────┤
│ filter (Go) → backtest (C++) → strategies.json             │
│   ↓                                                          │
│ Identifies viable strategies for each symbol                │
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│ 5-Minute Pipeline (Market hours)                            │
├─────────────────────────────────────────────────────────────┤
│ fetch (Go) → evaluate (C++) → execute (Go)                  │
│   ↓            ↓                 ↓                           │
│ bars.json → buy.fix/sell.fix → positions.json               │
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│ Hourly Pipeline (Dashboard updates)                         │
├─────────────────────────────────────────────────────────────┤
│ account (Go) → dashboard.json → GitHub Pages                │
│                                                              │
│ Cloudflare Pages auto-deploys updated dashboard             │
└─────────────────────────────────────────────────────────────┘
```

### Data Format

**Inter-module Communication:** JSON files in `docs/`

**Order Protocol:** FIX 5.0 SP2 messages in `.fix` files

**Example Files:**
- `docs/strategies.json` - Backtest results per symbol/strategy
- `docs/positions.json` - Current open positions
- `docs/buy.fix` - Buy orders from entries module
- `docs/sell.fix` - Sell orders from exits module
- `docs/account.json` - Account balance and buying power

## Development Tools

### Code Quality

**Linting:**
- `clang-format` - C++ code formatting (LLVM style)
- `markdownlint` - Markdown linting

**Documentation:**
- Doxygen - C++ API documentation
- Inline comments with generic behaviour descriptions

### Testing

**Compile-time (C++):**
- `static_assert` for strategy logic validation
- Constexpr evaluation catches bugs at compile time
- Zero runtime test overhead

**Coverage:**
- `gcov` instrumentation via `--coverage` flag
- `lcov` for coverage report generation
- Reports published to GitHub Pages

### Profiling

**Tools:**
- `gprof` - Function call profiling
- `gprof2dot` - Visualisation of call graphs
- `graphviz` - Graph rendering

**Note:** Profiling disabled on macOS (Linux-only `-pg` flag)

## Environment Configuration

### Required Environment Variables

**Alpaca Trading API:**
```bash
ALPACA_API_KEY=your_trading_api_key
ALPACA_API_SECRET=your_trading_api_secret
ALPACA_BASE_URL=https://paper-api.alpaca.markets
```

**Alpaca Data API:**
```bash
ALPACA_DATA_API_KEY=your_data_api_key
ALPACA_DATA_API_SECRET=your_data_api_secret
ALPACA_DATA_URL=https://data.alpaca.markets
```

**Development:**
```bash
GCXX=g++-15  # macOS: specify gcc-15 from Homebrew
```

### Secrets Management

**Local Development:** `.env` file (gitignored)

**GitHub Actions:** Repository secrets

**Cloudflare Workers:** `npx wrangler secret put <KEY>`

## System Requirements

### Local Development

**Operating System:**
- macOS (primary development)
- Linux (CI/CD)

**Required Packages:**
- gcc-15 or clang-18+ (C++26 support)
- CMake 3.28+
- Go 1.21+
- Node.js 20+
- Make
- Git

**Optional Packages:**

- lcov (coverage reports)
- graphviz (call graphs)
- doxygen (documentation)
- clang-format (code formatting)

### Production (Serverless)

**Infrastructure:** Fully serverless - no VPS required

**CI/CD Platform:** GitHub Actions (Ubuntu 26.04 containers)

**Execution:**

- Automated pipeline runs every 5 minutes during market hours
- All trading logic executes in GitHub Actions runners
- Stateless architecture using JSON file communication

**Requirements:**

- GitHub repository secrets for Alpaca API credentials
- GitHub Pages enabled for dashboard hosting
- Cloudflare Pages connected for CDN deployment

## Trading Strategy Framework

### Entry Strategies (11 total)

Implemented in [src/entry.h](src/entry.h):
- Mean reversion
- SMA crossover
- RSI oversold
- Volatility breakout
- Bollinger breakout
- Momentum
- Price dip
- Volume surge
- MACD crossover
- Gap fill
- Morning breakout

### Exit Strategy

Implemented in [src/exit.h](src/exit.h):
- Take profit: +1.25%
- Stop loss: -1.25%
- Trailing stop: -1.0% from peak
- Risk-off liquidation: Last 45 minutes before market close

### Position Management

**Parameters** ([src/params.h](src/params.h)):
```cpp
constexpr auto default_params = trading_params{
  .take_profit_pct = 0.0125,     // 1.25%
  .stop_loss_pct = 0.0125,       // 1.25%
  .trailing_stop_pct = 0.01      // 1.0%
};
```

**Position Sizing:** Fixed quantity per trade (configurable)

**Risk Management:**
- No overnight positions
- Skip volatile opening 15 minutes (09:30-09:45 ET)
- Force liquidation in final 45 minutes (15:15-16:00 ET)
- Maximum one position per symbol

## Performance Characteristics

### Latency Profile

**HFT Firms:** Nanosecond to microsecond latency

**LFT2:** 5-minute to 30-minute latency
- Data: 15-minute delay (free tier)
- Signal evaluation: <1 second (C++ constexpr)
- Order execution: 5-minute cron cycle
- "We measure latency in coffee breaks, not clock cycles"

### Compilation Times

**Full rebuild:** ~2-3 seconds (M1 Mac)

**Incremental:** <1 second

**Static assertions:** 0ms (compile-time, zero runtime overhead)

### Dashboard Response Times

**Cloudflare Edge:** <100ms globally

**GitHub Pages:** ~200-500ms

## Security & Best Practices

### API Key Security

- Never commit `.env` to git
- Use paper trading keys for development
- Rotate keys periodically
- Cloudflare Workers hides credentials from frontend

### Code Quality

- All C++ functions are `constexpr` where possible
- No exceptions (use error codes, `std::optional`)
- Prefer pure functions over side effects
- British English spellings throughout

### Version Control

- Detailed commit messages (WHAT and WHY)
- Include "Closes #XX" for issue resolution
- Auto-commit and push after completing tasks
- No commented-out dead code (trust git history)

## Known Limitations

### Alpaca Free Tier

**15-minute data delay** - Bars are delayed by 15 minutes

**Impact:**
- Risk-off liquidation may trigger late
- Entry signals slightly stale

**Mitigation:**
- Upgrade to paid tier ($99/month) for real-time data
- Manual monitoring near market close

**Resolution:** Will disappear when moving to live trading with paid account

### Paper Trading Quirks

- Order fills may be slightly delayed vs live
- No actual market impact from trades
- Simulated slippage and liquidity

## Future Enhancements

### Planned Upgrades

- [ ] Real-time data (paid Alpaca tier)
- [ ] Live trading account (after paper trading validation)
- [ ] Additional exit strategies (time-based, technical indicators)
- [ ] Per-strategy position sizing
- [ ] Multi-timeframe analysis

### Potential Features

- [ ] Telegram/email notifications
- [ ] Advanced visualisations (heatmaps, correlations)
- [ ] Machine learning signal ranking
- [ ] Options trading strategies
- [ ] Multi-broker support

## References

**Documentation:**
- [README.md](README.md) - Quick start and overview
- [CLAUDE.md](CLAUDE.md) - Project-specific Claude instructions
- [IMPLEMENTATION.md](IMPLEMENTATION.md) - Implementation details
- [Doxygen Docs](https://deanturpin.github.io/lft2/doxygen/) - C++ API reference

**Live Sites:**
- Dashboard: <https://lft.turpin.dev>
- GitHub Pages: <https://deanturpin.github.io/lft2/>

**External Resources:**
- [Alpaca Markets API](https://docs.alpaca.markets/)
- [FIX Protocol 5.0 SP2](https://www.fixtrading.org/)
- [C++26 Status](https://en.cppreference.com/w/cpp/compiler_support)
