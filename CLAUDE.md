# LFT2 - Low Frequency Trader v2

## Project Overview

Algorithmic trading system built with C++26 (constexpr strategies), Go (API integration), and Svelte (dashboard).

**Architecture**: JSON-based pipeline with focused modules
**Trading Platform**: Alpaca Markets (paper trading)
**Deployment**: GitHub Actions + GitHub Pages

## Critical Implementation Details

### API Keys

Alpaca provides **two separate sets of keys**:

1. **Trading API** (account, positions, orders): `ALPACA_API_KEY` / `ALPACA_API_SECRET`
2. **Data API** (bars, snapshots, quotes): `ALPACA_DATA_API_KEY` / `ALPACA_DATA_API_SECRET`

Both are required. See `.env.example` for full structure.

### Technology Stack

**C++26 modules** (`src/*.cxx`):

- `test` - Compile-time unit tests (static_assert)
- `profile` - Strategy profiler against snapshot bar data
- `evaluate` - Signal generation (platform-agnostic)

**Go modules** (`cmd/*/main.go`):

- `account` - Fetch account data from Alpaca
- `fetch` - Retrieve market snapshots
- `execute` - Place orders
- `filter` - Identify candidate stocks
- `backtest` - Daily strategy evaluation

**Svelte** (`web/`):

- Dashboard with real-time updates
- Builds to `docs/` for GitHub Pages

### Data Pipeline

```text
Daily:    filter → backtest → strategies.json
5min:     fetch → evaluate → execute → positions.json
Hourly:   account → dashboard.json → website
```

All modules communicate via JSON files.

### Build System

```bash
make build      # C++ modules (CMake + gcc-15)
make profile   # Profile strategies against snapshot data
make account    # Start Go account service
make web-dev    # Svelte dev server
make web-build  # Build static site to docs/
```

### Constexpr Trading Logic

All strategies must be `constexpr` for compile-time validation:

```cpp
constexpr bool is_entry(std::span<const bar> history) {
  if (volume_surge_dip(history)) return true;
  if (mean_reversion(history)) return true;
  if (sma_crossover(history)) return true;
  return false;
}
```

Use `nstd::` for functions not yet constexpr in C++20/23:

- `nstd::sqrt()` - Newton-Raphson square root (replace with `std::sqrt` in C++26)

### Common Issues

**Go modules fail to build**: Run `go mod tidy` to sync dependencies

**Web dashboard shows CORS errors**: Ensure account service is running on port 8080

**CMake can't find gcc-15**: Install via Homebrew: `brew install gcc@15`

**Profile build fails on macOS**: `-pg` flag is Linux-only, disabled via CMake check

## File Structure

```text
cmd/           - Go binaries (account, fetch, execute, filter, backtest)
src/           - C++ source (strategies, backtesting, signal generation)
web/           - Svelte dashboard (builds to docs/ for GitHub Pages)
docs/          - Built static site (GitHub Pages)
snapshots/     - Pre-compiled historical bar data headers (backtesting)
bin/           - Helper shell scripts (e.g. generate_data_headers.sh)
.github/workflows/pages.yml - CI/CD pipeline (build + deploy)
```

## Development Workflow

1. **Add strategy**: Implement as `constexpr` in `src/entry.h` or `src/exit.h`
2. **Unit test**: Add `static_assert` tests inline
3. **Profile**: Run `make profile` to profile strategies against snapshot data
4. **Deploy**: Push to main, GitHub Actions builds and publishes to Pages

## Security

- **Never commit `.env`** - Contains real API keys
- **Use paper trading keys** - Never use live trading keys in development
- **API keys in backend only** - Go services hide credentials from frontend
- **No secrets in GitHub Actions** - Use repository secrets for deployment

## Testing

**Compile-time** (`src/test.cxx`):

- Strategy logic validated via `static_assert`
- Bar parsing and validation
- Entry/exit conditions

**Runtime** (`make profile`):

- Backtest all strategies on 45 stocks
- Generate performance statistics
- Publish results to GitHub Pages

## Deployment

GitHub Actions workflow (`.github/workflows/pages.yml`):

1. Build C++ modules with gcc-15
2. Run strategy profiler to generate profile.json (per-symbol strategy stats)
3. Build Svelte dashboard
4. Deploy to GitHub Pages

Live site: <https://deanturpin.github.io/lft2/>

## Notes

- Positions closed before market close (no overnight gaps)
- Strategies run on 5min bars
- Exit management: take profit, stop loss, trailing stop
- British English spellings, date formats
