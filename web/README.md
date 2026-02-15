# LFT2 Dashboard

Svelte-based interactive dashboard for Low Frequency Trader v2.

## Development

```bash
# From project root
make web-dev

# Or from web directory
npm install
npm run dev
```

The dashboard will be available at `http://localhost:5173` and will auto-refresh when files change.

## Environment variables

Create a `.env` file in the project root:

```bash
ALPACA_API_KEY=your_key_here
ALPACA_API_SECRET=your_secret_here
ALPACA_BASE_URL=https://paper-api.alpaca.markets
```

## Running the backend

The dashboard fetches data from the Go account service:

```bash
# From project root
make account
```

This starts the API server on `http://localhost:8080`.

## Production build

```bash
# From project root
make web-build
```

This builds the optimised static site to `docs/` for GitHub Pages deployment.

## Features

- Real-time account summary (portfolio value, cash, buying power)
- Open positions table with P&L tracking
- Auto-refresh every 60 seconds
- Responsive dark theme matching GitHub aesthetic
- Colour-coded gains/losses
