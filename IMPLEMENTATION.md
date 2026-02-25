# Implementation Checklist

## Completed Modules ✅

### test

- [x] Compile-time unit tests operational
- [x] Static assertions for trading logic validation
- [x] Constexpr function testing

### profile

- [x] Backtest with historical data working
- [x] Performance metrics generation
- [x] `stats.json` and `top_stocks.json` output

### account (Cloudflare Worker)

- [x] `/api/dashboard` endpoint (account, positions, clock)
- [x] `/api/history` endpoint (portfolio history)
- [x] Secret management for Alpaca API credentials
- [x] CORS headers for browser access

### website

- [x] Svelte 5 SPA with reactive components
- [x] Real-time dashboard with auto-refresh (60s)
- [x] Portfolio history chart (7 days, hourly)
- [x] Positions table with P&L
- [x] Market status indicator
- [x] Client-side routing with About page
- [x] Cloudflare Pages deployment

## Modules To Implement 🚧

### filter (Go - VPS)

**Purpose**: Identify candidate stocks with suitable price action

**Tasks**:

- [ ] Fetch live snapshots for all assets from Alpaca
- [ ] Implement filtering criteria (volume, volatility, price range)
- [ ] Output `candidates.json` with filtered stock symbols
- [ ] Schedule to run every 5 minutes during market hours
- [ ] Error handling for API failures
- [ ] Logging and monitoring

**Output**: `candidates.json`

```json
{
  "timestamp": "2026-02-15T14:30:00Z",
  "symbols": ["AAPL", "MSFT", "GOOGL"],
  "criteria": {
    "min_volume": 1000000,
    "min_price": 10,
    "max_price": 500
  }
}
```

### backtest (Go - VPS)

**Purpose**: Daily strategy evaluation on live data

**Tasks**:

- [ ] Read `candidates.json` for symbols to test
- [ ] Fetch historical bars for each candidate from Alpaca
- [ ] Run all strategies against historical data
- [ ] Identify profitable strategy/symbol pairs
- [ ] Output `strategies.json` with recommendations
- [ ] Schedule to run daily pre-market (e.g., 8:00 AM ET)
- [ ] Performance metrics and win rate calculation

**Input**: `candidates.json`, Alpaca historical data
**Output**: `strategies.json`

```json
{
  "timestamp": "2026-02-15T08:00:00Z",
  "recommendations": [
    {
      "symbol": "AAPL",
      "strategy": "mean_reversion",
      "win_rate": 0.67,
      "avg_profit": 0.023
    }
  ]
}
```

### fetch (Go - VPS)

**Purpose**: Retrieve latest market data

**Tasks**:

- [ ] Rewrite existing C++ stub in Go
- [ ] Read symbols from `strategies.json`
- [ ] Fetch latest bars from Alpaca API
- [ ] Output `snapshots.json` with current market data
- [ ] Schedule to run every 5 minutes during market hours
- [ ] Handle rate limiting and API errors
- [ ] Cache previous snapshots for comparison

**Input**: `strategies.json`
**Output**: `snapshots.json`

```json
{
  "timestamp": "2026-02-15T14:35:00Z",
  "bars": [
    {
      "symbol": "AAPL",
      "close": 175.23,
      "volume": 45678900,
      "timestamp": "2026-02-15T14:35:00Z"
    }
  ]
}
```

### evaluate (C++26 - VPS)

**Purpose**: Generate trading signals from market data

**Tasks**:

- [ ] Enhance existing C++ stub with signal generation
- [ ] Read `snapshots.json`, `strategies.json`, `positions.json`
- [ ] Run constexpr strategy functions
- [ ] Calculate entry/exit signals based on strategies
- [ ] Output `signals.json` with trading recommendations
- [ ] Ensure platform-agnostic (no Alpaca-specific code)
- [ ] Comprehensive unit tests with static_assert

**Input**: `snapshots.json`, `strategies.json`, `positions.json`
**Output**: `signals.json`

```json
{
  "timestamp": "2026-02-15T14:35:00Z",
  "signals": [
    {
      "symbol": "AAPL",
      "action": "buy",
      "strategy": "mean_reversion",
      "quantity": 10,
      "limit_price": 175.00,
      "stop_loss": 170.00,
      "take_profit": 180.00
    }
  ]
}
```

### execute (Go - VPS)

**Purpose**: Place orders based on signals

**Tasks**:

- [ ] Rewrite existing C++ stub in Go
- [ ] Read `signals.json`
- [ ] Fetch current account state from Alpaca
- [ ] Validate signals against buying power and risk limits
- [ ] Submit orders to Alpaca API
- [ ] Handle order confirmations and rejections
- [ ] Update `positions.json` with new positions
- [ ] Implement retry logic for failed orders
- [ ] Comprehensive logging for audit trail

**Input**: `signals.json`, Alpaca account state
**Output**: `positions.json`

```json
{
  "timestamp": "2026-02-15T14:36:00Z",
  "positions": [
    {
      "symbol": "AAPL",
      "quantity": 10,
      "avg_entry_price": 175.10,
      "current_price": 175.23,
      "unrealized_pl": 1.30,
      "strategy": "mean_reversion"
    }
  ]
}
```

## Infrastructure Tasks 🔧

### VPS Setup (Fasthosts)

- [ ] Configure VPS environment
- [ ] Install C++26 compiler (GCC 14+ or Clang 18+)
- [ ] Install Go 1.21+
- [ ] Set up systemd services for each module
- [ ] Configure cron jobs for scheduled tasks
- [ ] Set up log rotation
- [ ] Configure firewall rules
- [ ] Set up monitoring and alerting

### Deployment

- [ ] Create deployment scripts for VPS
- [ ] Set up environment variables for Alpaca credentials
- [ ] Configure automatic restarts on failure
- [ ] Set up backup and recovery procedures
- [ ] Document deployment process

### Monitoring

- [ ] Set up health checks for all modules
- [ ] Create dashboard for VPS module status
- [ ] Configure alerts for failures
- [ ] Log aggregation and analysis
- [ ] Performance monitoring

## Testing Strategy 📋

### Unit Tests

- [x] C++ constexpr functions (test module)
- [ ] Go module unit tests (filter, backtest, fetch, execute)
- [ ] Integration tests between modules

### End-to-End Testing

- [ ] Full pipeline test with paper trading
- [ ] Verify JSON data flow between modules
- [ ] Test failure scenarios and recovery
- [ ] Load testing for market hours

### Validation

- [ ] Backtest results match live execution
- [ ] Order execution matches signals
- [ ] Position tracking accuracy
- [ ] P&L calculation verification

## Documentation 📚

- [x] README.md with architecture overview
- [x] DEPLOYMENT.md for Cloudflare setup
- [ ] VPS deployment guide
- [ ] Module API documentation (JSON schemas)
- [ ] Troubleshooting guide
- [ ] Trading strategy documentation
