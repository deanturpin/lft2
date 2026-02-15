// Cloudflare Worker for account API
// Fetches account data and positions from Alpaca

export default {
  async fetch(request, env) {
    const url = new URL(request.url);

    // CORS headers
    const corsHeaders = {
      'Access-Control-Allow-Origin': '*',
      'Access-Control-Allow-Methods': 'GET, OPTIONS',
      'Access-Control-Allow-Headers': 'Content-Type',
    };

    // Handle CORS preflight
    if (request.method === 'OPTIONS') {
      return new Response(null, { headers: corsHeaders });
    }

    // Route handling
    if (url.pathname === '/api/dashboard') {
      try {
        const dashboard = await fetchDashboard(env);

        // Store snapshot in KV (once per hour to avoid excessive writes)
        if (env.HISTORY) {
          const now = new Date();
          const hourKey = `snapshot-${now.toISOString().substring(0, 13)}`; // YYYY-MM-DDTHH
          await env.HISTORY.put(hourKey, JSON.stringify({
            timestamp: now.toISOString(),
            equity: dashboard.account.equity,
            cash: dashboard.account.cash,
            portfolio_value: dashboard.account.portfolio_value,
          }));
        }

        return new Response(JSON.stringify(dashboard), {
          headers: {
            'Content-Type': 'application/json',
            ...corsHeaders,
          },
        });
      } catch (error) {
        return new Response(JSON.stringify({ error: error.message }), {
          status: 500,
          headers: {
            'Content-Type': 'application/json',
            ...corsHeaders,
          },
        });
      }
    }

    if (url.pathname === '/api/history') {
      try {
        const days = parseInt(url.searchParams.get('days') || '30');
        const history = await fetchHistory(env, days);
        return new Response(JSON.stringify(history), {
          headers: {
            'Content-Type': 'application/json',
            ...corsHeaders,
          },
        });
      } catch (error) {
        return new Response(JSON.stringify({ error: error.message }), {
          status: 500,
          headers: {
            'Content-Type': 'application/json',
            ...corsHeaders,
          },
        });
      }
    }

    return new Response('Not Found', { status: 404 });
  },
};

async function fetchDashboard(env) {
  const baseUrl = env.ALPACA_BASE_URL || 'https://paper-api.alpaca.markets';
  const apiKey = env.ALPACA_API_KEY;
  const apiSecret = env.ALPACA_API_SECRET;

  if (!apiKey || !apiSecret) {
    throw new Error('Alpaca API credentials not configured');
  }

  const headers = {
    'APCA-API-KEY-ID': apiKey,
    'APCA-API-SECRET-KEY': apiSecret,
  };

  // Fetch account, positions, and clock in parallel
  const [accountRes, positionsRes, clockRes] = await Promise.all([
    fetch(`${baseUrl}/v2/account`, { headers }),
    fetch(`${baseUrl}/v2/positions`, { headers }),
    fetch(`${baseUrl}/v2/clock`, { headers }),
  ]);

  if (!accountRes.ok) {
    throw new Error(`Alpaca account API returned ${accountRes.status}`);
  }

  if (!positionsRes.ok) {
    throw new Error(`Alpaca positions API returned ${positionsRes.status}`);
  }

  if (!clockRes.ok) {
    throw new Error(`Alpaca clock API returned ${clockRes.status}`);
  }

  const [account, positions, clock] = await Promise.all([
    accountRes.json(),
    positionsRes.json(),
    clockRes.json(),
  ]);

  return {
    account: {
      ...account,
      last_fetched: new Date().toISOString(),
    },
    positions,
    clock: {
      timestamp: clock.timestamp,
      is_open: clock.is_open,
      next_open: clock.next_open,
      next_close: clock.next_close,
    },
  };
}

async function fetchHistory(env, days) {
  if (!env.HISTORY) {
    return [];
  }

  const history = [];
  const now = new Date();

  // Fetch hourly snapshots for the requested number of days
  for (let i = 0; i < days * 24; i++) {
    const time = new Date(now.getTime() - i * 60 * 60 * 1000);
    const hourKey = `snapshot-${time.toISOString().substring(0, 13)}`;
    const snapshot = await env.HISTORY.get(hourKey);

    if (snapshot) {
      history.push(JSON.parse(snapshot));
    }
  }

  return history.reverse(); // Oldest first
}
