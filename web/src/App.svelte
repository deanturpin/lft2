<script>
  import { onMount, onDestroy } from 'svelte';

  let dashboard = null;
  let error = null;
  let loading = true;
  let interval;

  const API_URL = import.meta.env.VITE_API_URL ||
    (import.meta.env.PROD ? 'https://lft.turpin.dev' : 'http://localhost:8080');

  async function fetchDashboard() {
    try {
      const response = await fetch(`${API_URL}/api/dashboard`);
      if (!response.ok) {
        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
      }
      dashboard = await response.json();
      error = null;
      loading = false;
    } catch (err) {
      error = err.message;
      loading = false;
      console.error('Error fetching dashboard:', err);
    }
  }

  onMount(() => {
    fetchDashboard();
    // Refresh every minute
    interval = setInterval(fetchDashboard, 60000);
  });

  onDestroy(() => {
    if (interval) clearInterval(interval);
  });

  function formatCurrency(value) {
    return new Intl.NumberFormat('en-GB', {
      style: 'currency',
      currency: 'USD',
      minimumFractionDigits: 2,
      maximumFractionDigits: 2
    }).format(parseFloat(value));
  }

  function formatPercent(value) {
    const num = parseFloat(value) * 100;
    const sign = num >= 0 ? '+' : '';
    return `${sign}${num.toFixed(2)}%`;
  }

  function formatTimestamp(timestamp) {
    return new Date(timestamp).toLocaleString('en-GB', {
      dateStyle: 'medium',
      timeStyle: 'medium'
    });
  }
</script>

<a href="https://github.com/deanturpin/lft2" class="github-ribbon" target="_blank" rel="noopener noreferrer">
  Fork me on GitHub
</a>

<main>
  <h1>Low Frequency Trader</h1>

  {#if dashboard}
    <div class="timestamp">
      Last updated: {formatTimestamp(dashboard.account.last_fetched)}
      {#if dashboard.clock}
        | Market: <span class={dashboard.clock.is_open ? 'positive' : 'negative'}>
          {dashboard.clock.is_open ? 'OPEN' : 'CLOSED'}
        </span>
        | Server time: {new Date(dashboard.clock.timestamp).toLocaleString('en-GB', { timeStyle: 'medium' })}
      {/if}
    </div>
  {/if}

  {#if error}
    <div class="error">
      Error: {error}
    </div>
  {/if}

  {#if loading}
    <div class="loading">Loading account data...</div>
  {:else if dashboard}
    <div class="card">
      <h2>Account Summary</h2>
      <div class="grid">
        <div class="metric">
          <div class="metric-label">Portfolio Value</div>
          <div class="metric-value">
            {formatCurrency(dashboard.account.portfolio_value)}
          </div>
        </div>
        <div class="metric">
          <div class="metric-label">Cash</div>
          <div class="metric-value">
            {formatCurrency(dashboard.account.cash)}
          </div>
        </div>
        <div class="metric">
          <div class="metric-label">Buying Power</div>
          <div class="metric-value">
            {formatCurrency(dashboard.account.buying_power)}
          </div>
        </div>
        <div class="metric">
          <div class="metric-label">Equity</div>
          <div class="metric-value">
            {formatCurrency(dashboard.account.equity)}
          </div>
        </div>
      </div>
    </div>

    {#if dashboard.positions && dashboard.positions.length > 0}
      <div class="card">
        <h2>Open Positions ({dashboard.positions.length})</h2>
        <table>
          <thead>
            <tr>
              <th>Symbol</th>
              <th>Qty</th>
              <th>Avg Entry</th>
              <th>Current</th>
              <th>Market Value</th>
              <th>Unrealised P&L</th>
              <th>P&L %</th>
              <th>Today</th>
            </tr>
          </thead>
          <tbody>
            {#each dashboard.positions as position}
              <tr>
                <td><strong>{position.symbol}</strong></td>
                <td>{position.qty}</td>
                <td>{formatCurrency(position.avg_entry_price)}</td>
                <td>{formatCurrency(position.current_price)}</td>
                <td>{formatCurrency(position.market_value)}</td>
                <td class={parseFloat(position.unrealized_pl) >= 0 ? 'positive' : 'negative'}>
                  {formatCurrency(position.unrealized_pl)}
                </td>
                <td class={parseFloat(position.unrealized_plpc) >= 0 ? 'positive' : 'negative'}>
                  {formatPercent(position.unrealized_plpc)}
                </td>
                <td class={parseFloat(position.change_today) >= 0 ? 'positive' : 'negative'}>
                  {formatPercent(position.change_today)}
                </td>
              </tr>
            {/each}
          </tbody>
        </table>
      </div>
    {:else}
      <div class="card">
        <h2>Open Positions</h2>
        <p style="color: #8b949e;">No open positions</p>
      </div>
    {/if}

    <div class="card">
      <h2>Account Details</h2>
      <div class="grid">
        <div class="metric">
          <div class="metric-label">Account Number</div>
          <div class="metric-value" style="font-size: 1rem;">
            {dashboard.account.account_number}
          </div>
        </div>
        <div class="metric">
          <div class="metric-label">Status</div>
          <div class="metric-value" style="font-size: 1rem;">
            {dashboard.account.status}
          </div>
        </div>
        <div class="metric">
          <div class="metric-label">Currency</div>
          <div class="metric-value" style="font-size: 1rem;">
            {dashboard.account.currency}
          </div>
        </div>
        <div class="metric">
          <div class="metric-label">Day Trading Buying Power</div>
          <div class="metric-value">
            {formatCurrency(dashboard.account.daytrading_buying_power)}
          </div>
        </div>
      </div>
    </div>
  {/if}
</main>
