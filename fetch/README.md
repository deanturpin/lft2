# Fetch Module

Fetches historical 5-minute bar data for watchlist symbols from Alpaca API.

## Features

- Concurrent fetching with goroutines for fast execution
- Fetches most recent 1000 bars per symbol (configurable)
- Outputs both JSON and CSV formats
- Saves to `public/bars/` for web access
- Error handling and retry logic
- Progress reporting

## Usage

```bash
# Set environment variables
export ALPACA_API_KEY="your_key"
export ALPACA_API_SECRET="your_secret"

# Run with defaults (1000 bars, 5min timeframe)
go run . -watchlist ../watchlist.json -output ../docs/bars

# Customise parameters
go run . -watchlist ../watchlist.json -output ../docs/bars -bars 2000 -timeframe 15
```

## Command-line Flags

- `-watchlist` - Path to watchlist JSON file (default: `watchlist.json`)
- `-output` - Output directory for bar data (default: `docs/bars`)
- `-bars` - Number of bars to fetch per symbol (default: 1000)
- `-timeframe` - Timeframe in minutes (default: 5)

## Input Format

Watchlist JSON file:

```json
{
  "symbols": ["AAPL", "GOOGL", "MSFT"]
}
```

## Output Format

For each symbol, creates two files:

**JSON** (`AAPL.json`):

```json
{
  "symbol": "AAPL",
  "bars": [
    {
      "t": "2026-02-15T14:30:00Z",
      "o": 175.10,
      "h": 175.50,
      "l": 175.00,
      "c": 175.23,
      "v": 1234567
    }
  ],
  "count": 1000,
  "fetched_at": "2026-02-15T20:45:00Z"
}
```

**CSV** (`AAPL.csv`):

```csv
timestamp,open,high,low,close,volume
2026-02-15T14:30:00Z,175.10,175.50,175.00,175.23,1234567
```

## Integration

Output files in `docs/bars/` can be:

- Served by GitHub Pages
- Accessed from the web for analysis
- Used by the evaluate module for signal generation
- Archived for backtesting

## Scheduling

Run periodically during market hours (e.g., via cron on VPS):

```bash
# Update every 5 minutes during market hours (9:30 AM - 4:00 PM ET)
*/5 9-16 * * 1-5 cd /path/to/lft2/fetch && ./fetch >> logs/fetch.log 2>&1
```
