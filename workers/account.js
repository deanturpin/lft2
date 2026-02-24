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
        const period = url.searchParams.get('period') || '1W';
        const timeframe = url.searchParams.get('timeframe') || '1H';
        const history = await fetchPortfolioHistory(env, period, timeframe);
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

    if (url.pathname === '/api/daily-summary') {
      try {
        const response = await fetch('https://deanturpin.github.io/lft2/daily-summary.json');
        if (!response.ok) {
          throw new Error(`GitHub Pages returned ${response.status}`);
        }
        const data = await response.json();
        return new Response(JSON.stringify(data), {
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

async function fetchPortfolioHistory(env, period, timeframe) {
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

  const url = `${baseUrl}/v2/account/portfolio/history?period=${period}&timeframe=${timeframe}`;
  const response = await fetch(url, { headers });

  if (!response.ok) {
    throw new Error(`Alpaca portfolio history API returned ${response.status}`);
  }

  return response.json();
}
