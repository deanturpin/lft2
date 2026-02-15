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

    return new Response('Not Found', { status: 404 });
  },
};

async function fetchDashboard(env) {
  const baseUrl = env.ALPACA_BASE_URL || 'https://paper-api.alpaca.markets';
  const apiKey = env.ALPACA_API_KEY;
  const apiSecret = env.ALPACA_API_SECRET;

  console.log('Debug - API Key exists:', !!apiKey, 'API Secret exists:', !!apiSecret);
  console.log('Debug - env keys:', Object.keys(env));

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
