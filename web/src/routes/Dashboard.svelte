<script>
  import { onMount, onDestroy } from 'svelte';
  import Chart from 'chart.js/auto';

  let dashboard = null;
  let history = [];
  let error = null;
  let loading = true;
  let interval;
  let chartCanvas;
  let chart;
  let dailySummary = null;
  let activities = [];

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

  async function fetchHistory() {
    try {
      const response = await fetch(`${API_URL}/api/history?period=1W&timeframe=1D`);
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

  async function fetchActivities() {
    try {
      const response = await fetch(`${API_URL}/api/activities`);
      if (!response.ok) return;
      activities = await response.json();
    } catch (err) {
      console.error('Error fetching activities:', err);
    }
  }

  function updateChart() {
    if (!chartCanvas || history.length === 0) return;

    if (chart) {
      chart.destroy();
    }

    // Aggregate trades by day from activities (last 7 days)
    const tradesByDay = {};
    if (activities && activities.length > 0) {
      activities.forEach(activity => {
        const date = activity.transaction_time.substring(0, 10); // YYYY-MM-DD
        tradesByDay[date] = (tradesByDay[date] || 0) + 1;
      });
    }

    // Match trade counts to history timestamps
    const tradeCounts = history.map(h => {
      const date = new Date(h.timestamp * 1000);
      const dateStr = date.toISOString().substring(0, 10);
      return tradesByDay[dateStr] || 0;
    });

    const ctx = chartCanvas.getContext('2d');
    chart = new Chart(ctx, {
      type: 'line',
      data: {
        labels: history.map(h => {
          const date = new Date(h.timestamp * 1000); // Unix timestamp to milliseconds
          return date.toLocaleString('en-US', { month: 'short', day: 'numeric', timeZone: 'America/New_York' });
        }),
        datasets: [{
          label: 'Equity',
          data: history.map(h => h.equity),
          borderColor: '#58a6ff',
          backgroundColor: 'rgba(88, 166, 255, 0.1)',
          tension: 0.1,
          fill: true,
          yAxisID: 'y',
        }, {
          label: 'Trades',
          data: tradeCounts,
          borderColor: '#56d364',
          backgroundColor: 'rgba(86, 211, 100, 0.1)',
          tension: 0.1,
          fill: false,
          yAxisID: 'y1',
        }]
      },
      options: {
        responsive: true,
        maintainAspectRatio: false,
        plugins: {
          legend: {
            display: true,
            labels: {
              color: '#8b949e'
            }
          }
        },
        scales: {
          y: {
            type: 'linear',
            position: 'left',
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
          y1: {
            type: 'linear',
            position: 'right',
            ticks: {
              color: '#8b949e',
              stepSize: 1
            },
            grid: {
              display: false
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
    fetchActivities();
    // Refresh every minute
    interval = setInterval(() => {
      fetchDashboard();
      fetchHistory();
      fetchDailySummary();
      fetchActivities();
    }, 60000);
  });

  onDestroy(() => {
    if (interval) clearInterval(interval);
    if (chart) chart.destroy();
  });

  // Reactively update chart when history, activities, or chartCanvas changes
  $: if (chartCanvas && history.length > 0) {
    updateChart();
  }

  // Also update when activities change (for trade counts)
  $: if (chartCanvas && history.length > 0 && activities) {
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
            <th>Net Amount</th>
          </tr>
        </thead>
        <tbody>
          {#each dailySummary.activities as activity}
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
              <td class={parseFloat(activity.net_amount || 0) >= 0 ? 'positive' : 'negative'}>
                {activity.net_amount ? formatCurrency(activity.net_amount) : '—'}
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
