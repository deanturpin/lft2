# Cloudflare Deployment Guide

## Overview

- **Cloudflare Workers**: Serverless API backend (`lft-api`)
- **Cloudflare Pages**: Static Svelte dashboard (`lft`)
- **Domain**: lft.turpin.dev

## Prerequisites

1. Cloudflare account with `turpin.dev` domain
2. Node.js installed locally
3. Wrangler CLI: `npm install -g wrangler`

## Step 1: Deploy Workers (API Backend)

```bash
cd workers
npm install

# Login to Cloudflare
wrangler login

# Set secrets (API keys)
wrangler secret put ALPACA_API_KEY
wrangler secret put ALPACA_API_SECRET
wrangler secret put ALPACA_DATA_API_KEY
wrangler secret put ALPACA_DATA_API_SECRET
wrangler secret put ALPACA_BASE_URL
wrangler secret put ALPACA_DATA_URL

# Deploy
npm run deploy
```

The Worker will be available at: `https://lft.turpin.dev/api/*`

## Step 2: Deploy Pages (Dashboard)

### Option A: CLI Deployment

```bash
cd web
npm install
npm run build

# Deploy to Pages
npx wrangler pages deploy ../docs --project-name=lft
```

### Option B: GitHub Integration (Recommended)

1. Go to Cloudflare Dashboard → Pages
2. Connect to GitHub repository: `deanturpin/lft2`
3. Configure build settings:
   - **Build command**: `cd web && npm install && npm run build`
   - **Build output directory**: `docs`
   - **Root directory**: `/`
4. Set custom domain: `lft.turpin.dev`
5. Deploy

Pages will auto-deploy on every push to main.

## Step 3: Configure DNS

In Cloudflare DNS for `turpin.dev`:

```
Type    Name    Content                          Proxy
CNAME   lft     lft.pages.dev                    Proxied
```

Cloudflare will automatically route:
- `/api/*` → Workers
- `/*` → Pages

## Step 4: Verify

Open `https://lft.turpin.dev` - you should see:
- Account balance and positions
- Live updates every 60 seconds
- No CORS errors (both served from same domain)

## Local Development

```bash
# Terminal 1: Run Workers locally
cd workers
npm run dev

# Terminal 2: Run Svelte dev server
cd web
npm run dev
```

Dashboard: http://localhost:5173
Workers: http://localhost:8787

## Secrets Management

**Never commit secrets**. All API keys are stored in Cloudflare:

- Workers: `wrangler secret put <KEY_NAME>`
- Pages: Set in Dashboard → Settings → Environment variables

## Updating

**Workers**:
```bash
cd workers && npm run deploy
```

**Pages**:
- Automatic via GitHub integration
- Or: `cd web && npm run build && npx wrangler pages deploy ../docs`

## Monitoring

- **Workers**: Dashboard → Workers & Pages → lft-api → Metrics
- **Pages**: Dashboard → Pages → lft → Analytics
- **Logs**: Use `wrangler tail` for live Worker logs

## Cost

Both Workers and Pages are free tier:
- Workers: 100k requests/day
- Pages: Unlimited requests, 500 builds/month

Perfect for a low-frequency trading bot.
