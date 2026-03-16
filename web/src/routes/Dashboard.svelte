<script>
  import { onMount, onDestroy } from 'svelte';
  import Chart from 'chart.js/auto';
  import { marketOpen } from '../lib/market.js';

  let dashboard = null;
  let history = [];
  let error = null;
  let loading = true;
  let interval;
  let chartCanvas;
  let chart;
  let dailySummary = null;
  let isMarketOpen = false;

  const API_URL = import.meta.env.VITE_API_URL ||
    (import.meta.env.PROD ? 'https://lft.turpin.dev' : 'http://localhost:8080');

  // Update favicon based on market status
  function updateFavicon(open) {
    let link = document.querySelector("link[rel*='icon']");
    if (!link) {
      link = document.createElement('link');
      link.rel = 'icon';
      document.head.appendChild(link);
    }
    link.href = open ? '/favicon-open.svg' : '/favicon-closed.svg';
  }

  // Update market status based on latest position timestamp
  $: if (dashboard?.positions?.length > 0) {
    // Use the most recent position timestamp
    const latestTimestamp = dashboard.positions[0]?.updated_at;
    if (latestTimestamp) {
      isMarketOpen = marketOpen(latestTimestamp);
      updateFavicon(isMarketOpen);
    }
  } else {
    // No positions - use current time to check market status
    const now = new Date().toISOString();
    isMarketOpen = marketOpen(now);
    updateFavicon(isMarketOpen);
  }

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

  async function fetchHistory() {
    try {
      const response = await fetch(`${API_URL}/api/history?period=1W&timeframe=1H`);
      if (!response.ok) return;
      const data = await response.json();

      // Alpaca returns arrays: timestamp[], equity[], profit_loss[]
      if (data.timestamp && data.equity && data.timestamp.length > 0) {
        history = data.timestamp.map((ts, i) => ({
          timestamp: ts,
          equity: data.equity[i],
          profit_loss: data.profit_loss ? data.profit_loss[i] : 0
        }));
      }
    } catch (err) {
      console.error('Error fetching history:', err);
    }
  }

  async function fetchDailySummary() {
    try {
      const response = await fetch(`${API_URL}/api/daily-summary`);
      if (!response.ok) return;
      dailySummary = await response.json();
    } catch (err) {
      console.error('Error fetching daily summary:', err);
    }
  }

  function updateChart() {
    if (!chartCanvas || history.length === 0) return;

    if (chart) {
      chart.destroy();
    }

    const ctx = chartCanvas.getContext('2d');
    chart = new Chart(ctx, {
      type: 'line',
      data: {
        labels: history.map(h => {
          const date = new Date(h.timestamp * 1000); // Unix timestamp to milliseconds
          return date.toLocaleString('en-US', { month: 'short', day: 'numeric', hour: 'numeric', timeZone: 'America/New_York' });
        }),
        datasets: [{
          label: 'Equity',
          data: history.map(h => h.equity),
          borderColor: '#58a6ff',
          backgroundColor: 'rgba(88, 166, 255, 0.1)',
          tension: 0.1,
          fill: true,
        }]
      },
      options: {
        responsive: true,
        maintainAspectRatio: false,
        plugins: {
          legend: {
            display: false
          }
        },
        scales: {
          y: {
            ticks: {
              callback: function(value) {
                return '$' + value.toLocaleString();
              },
              color: '#8b949e'
            },
            grid: {
              color: '#30363d'
            }
          },
          x: {
            ticks: {
              color: '#8b949e',
              maxRotation: 45,
              minRotation: 45,
              maxTicksLimit: 8
            },
            grid: {
              color: '#30363d'
            }
          }
        }
      }
    });
  }

  onMount(() => {
    fetchDashboard();
    fetchHistory();
    fetchDailySummary();
    // Refresh every minute
    interval = setInterval(() => {
      fetchDashboard();
      fetchHistory();
      fetchDailySummary();
    }, 60000);
  });

  onDestroy(() => {
    if (interval) clearInterval(interval);
    if (chart) chart.destroy();
  });

  // Reactively update chart when history or chartCanvas changes
  $: if (chartCanvas && history.length > 0) {
    updateChart();
  }

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

  // Parse client_order_id to extract trade parameters
  // Buy format: AAPL_mean_reversion_tp3_sl2_tsl1_20260218T143000
  // Sell format (new): EXIT_AAPL_take_profit_1773171991902440579
  // Sell format (old): EXIT_AAPL_2_1773171991902440579 (sequence number)
  function parseOrderID(orderId) {
    if (!orderId) return null;
    const parts = orderId.split('_');

    // Check if it's an exit order
    if (parts[0] === 'EXIT' && parts.length >= 4) {
      const exitReason = parts[2];

      // Old format: sequence number (single digit or small number)
      // New format: exit reason (contains underscores or text)
      if (/^\d+$/.test(exitReason) && parseInt(exitReason) < 100) {
        // Old format with sequence number - no exit reason available
        return null;
      }

      // New format with exit reason
      // Format exit reason nicely: take_profit → Take Profit
      const formatted = exitReason.split('_')
        .map(word => word.charAt(0).toUpperCase() + word.slice(1))
        .join(' ');
      return { exitReason: formatted };
    }

    // Parse buy order format
    if (parts.length < 6) return null;

    const strategy = parts.slice(1, parts.length - 4).join('_');
    const tp = parts[parts.length - 4].replace('tp', '');
    const sl = parts[parts.length - 3].replace('sl', '');
    const tsl = parts[parts.length - 2].replace('tsl', '');

    return { strategy, tp, sl, tsl };
  }
</script>

{#if dashboard && dashboard.clock}
  <div class="timestamp">
    Market: <span class={dashboard.clock.is_open ? 'positive' : 'negative'}>
      {dashboard.clock.is_open ? 'OPEN' : 'CLOSED'}
    </span>
    | {new Date(dashboard.clock.timestamp).toLocaleString('en-US', {
      month: 'short',
      day: 'numeric',
      year: 'numeric',
      hour: 'numeric',
      minute: '2-digit',
      second: '2-digit',
      timeZone: 'America/New_York',
      timeZoneName: 'short'
    })}
    {#if error}
      <span style="color: #f85149;"> (offline)</span>
    {/if}
  </div>
{:else if error}
  <div class="timestamp" style="color: #f85149;">
    (offline)
  </div>
{/if}

{#if loading}
  <div class="loading">Loading account data...</div>
{:else if dashboard}
  <div class="card">
    <h2>Account Summary</h2>
    <div class="grid">
      <div class="metric">
        <div class="metric-label">Equity</div>
        <div class="metric-value">
          {formatCurrency(dashboard.account.equity)}
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
    </div>
  </div>

  {#if history.length > 0}
    <div class="card">
      <h2>Portfolio History (7 days)</h2>
      <div class="chart-container">
        <canvas bind:this={chartCanvas}></canvas>
      </div>
    </div>
  {/if}

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

  {#if dailySummary && dailySummary.activities && dailySummary.activities.length > 0}
    <div class="card">
      <h2>Today's Trades ({dailySummary.activities.length})</h2>
      <table>
        <thead>
          <tr>
            <th>Time</th>
            <th>Symbol</th>
            <th>Side</th>
            <th>Quantity</th>
            <th>Price</th>
            <th>Strategy / Exits</th>
          </tr>
        </thead>
        <tbody>
          {#each dailySummary.activities as activity}
            {@const params = parseOrderID(activity.order_id)}
            <tr>
              <td style="color: #8b949e; font-size: 0.9em;">
                {activity.transaction_time.substring(11, 19)}
              </td>
              <td><strong>{activity.symbol}</strong></td>
              <td style="color: {activity.side === 'buy' ? '#56d364' : '#8b949e'};">
                {activity.side}
              </td>
              <td>{activity.qty}</td>
              <td>{formatCurrency(activity.price)}</td>
              <td style="color: #8b949e; font-size: 0.9em;">
                {#if params && params.exitReason}
                  {params.exitReason}
                {:else if params}
                  {params.strategy} (TP:{params.tp}% SL:{params.sl}% TSL:{params.tsl}%)
                {:else}
                  —
                {/if}
              </td>
            </tr>
          {/each}
        </tbody>
      </table>
    </div>
  {:else if dailySummary}
    <div class="card">
      <h2>Today's Trades</h2>
      <p style="color: #8b949e;">No trades executed today</p>
    </div>
  {/if}
{/if}
