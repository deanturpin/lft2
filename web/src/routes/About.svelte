<div class="card">
  <h2>About Low Frequency Trader</h2>
  <p style="margin-bottom: 1rem;">
    LFT is a fully automated algorithmic trading platform with a constexpr-first C++26 architecture, designed for compile-time validation and guaranteed correctness. This is a total redesign of the original <a href="https://github.com/deanturpin/lft" target="_blank" rel="noopener noreferrer" style="color: #58a6ff; text-decoration: none;">LFT project</a>, focusing on making strategy behaviour transparent and reproducible between backtest and live trading.
  </p>

  <h3 style="margin-top: 1.5rem; margin-bottom: 0.5rem; color: #8b949e;">Architecture</h3>
  <ul style="color: #8b949e; margin-left: 1.5rem; margin-bottom: 1rem;">
    <li><strong>Frontend</strong>: Svelte 5 SPA with Chart.js for portfolio visualisation</li>
    <li><strong>API Layer</strong>: Cloudflare Workers for serverless API endpoints</li>
    <li><strong>Hosting</strong>: Cloudflare Pages with automatic GitHub deployments</li>
    <li><strong>Trading Engine</strong>: C++26 application running on Fasthosts VPS</li>
    <li><strong>Analysis</strong>: GitHub Pages for backtest results and bar data</li>
    <li><strong>Data Source</strong>: Alpaca paper trading API (transitioning to live)</li>
  </ul>

  <h3 style="margin-top: 1.5rem; margin-bottom: 0.5rem; color: #8b949e;">Constexpr-First Design</h3>
  <p style="color: #8b949e; margin-bottom: 0.5rem;">
    All core trading logic is implemented as constexpr functions for compile-time validation:
  </p>
  <ul style="color: #8b949e; margin-left: 1.5rem; margin-bottom: 1rem;">
    <li>Entry strategies (volume surge, mean reversion, SMA crossover)</li>
    <li>Exit management (take profit, stop loss, trailing stops)</li>
    <li>Price bar validation and indicator calculations</li>
    <li>Compile-time unit tests with static_assert</li>
  </ul>
  <p style="color: #8b949e; margin-bottom: 0.5rem;">
    This approach provides:
  </p>
  <ul style="color: #8b949e; margin-left: 1.5rem; margin-bottom: 1rem;">
    <li><strong>Guaranteed correctness</strong> - Strategies validated at compile time</li>
    <li><strong>Zero runtime overhead</strong> - Calculations performed during compilation</li>
    <li><strong>Memory safety</strong> - No dynamic allocation in critical paths</li>
    <li><strong>Reproducibility</strong> - Identical behaviour between backtest and live trading</li>
  </ul>

  <h3 style="margin-top: 1.5rem; margin-bottom: 0.5rem; color: #8b949e;">System Modules</h3>
  <ul style="color: #8b949e; margin-left: 1.5rem; margin-bottom: 1rem;">
    <li><strong>fetch</strong> (Go) - Retrieves 1000 bars per symbol from Alpaca (pagination limit)</li>
    <li><strong>filter</strong> (C++) - Identifies candidate stocks from bar data</li>
    <li><strong>backtest</strong> (C++) - Tests strategies using constexpr entry/exit logic</li>
    <li><strong>evaluate</strong> (C++) - Generates trading signals from market data</li>
    <li><strong>execute</strong> (Go) - Places orders with Alpaca</li>
    <li><strong>account</strong> (Worker) - Real-time API for dashboard</li>
    <li><strong>website</strong> (Svelte) - Interactive dashboard with live charts</li>
  </ul>

  <h3 style="margin-top: 1.5rem; margin-bottom: 0.5rem; color: #8b949e;">Data Flow</h3>
  <p style="color: #8b949e; margin-bottom: 0.5rem;">
    <strong>Daily (pre-market on VPS):</strong>
  </p>
  <p style="color: #8b949e; font-family: monospace; font-size: 0.875rem; margin-left: 1rem; margin-bottom: 1rem;">
    filter → backtest → strategies.json
  </p>
  <p style="color: #8b949e; margin-bottom: 0.5rem;">
    <strong>Every 5min (market hours on VPS):</strong>
  </p>
  <p style="color: #8b949e; font-family: monospace; font-size: 0.875rem; margin-left: 1rem; margin-bottom: 1rem;">
    fetch → evaluate → execute → positions.json
  </p>
  <p style="color: #8b949e; margin-bottom: 0.5rem;">
    <strong>Real-time (Cloudflare edge):</strong>
  </p>
  <p style="color: #8b949e; font-family: monospace; font-size: 0.875rem; margin-left: 1rem; margin-bottom: 1rem;">
    Browser → Cloudflare Pages → Worker (/api/*) → Alpaca → JSON → Browser (60s refresh)
  </p>

  <h3 style="margin-top: 1.5rem; margin-bottom: 0.5rem; color: #8b949e;">Design Principles</h3>
  <ul style="color: #8b949e; margin-left: 1.5rem; margin-bottom: 1rem;">
    <li><strong>JSON as lingua franca</strong> - All modules communicate via JSON</li>
    <li><strong>Platform isolation</strong> - Alpaca-specific code in fetch/execute/account</li>
    <li><strong>Pure trading logic</strong> - Core strategies are platform-agnostic and testable</li>
    <li><strong>Incremental builds</strong> - Each module developed/tested independently</li>
    <li><strong>Edge-first delivery</strong> - Sub-100ms dashboard response times via Cloudflare</li>
    <li><strong>Right tool for the job</strong> - C++ for constexpr, Go for APIs, Svelte for UI</li>
  </ul>

  <h3 style="margin-top: 1.5rem; margin-bottom: 0.5rem; color: #8b949e;">Features</h3>
  <ul style="color: #8b949e; margin-left: 1.5rem; margin-bottom: 1rem;">
    <li>Real-time account summary with equity, cash, and buying power</li>
    <li>7-day portfolio history chart with hourly granularity</li>
    <li>Live positions table with unrealised P&L and percentage gains</li>
    <li>Market status indicator (OPEN/CLOSED) with Eastern Time</li>
    <li>Auto-refresh every 60 seconds</li>
    <li>Client-side routing with navigation</li>
  </ul>

  <h3 style="margin-top: 1.5rem; margin-bottom: 0.5rem; color: #8b949e;">Tech Stack</h3>
  <ul style="color: #8b949e; margin-left: 1.5rem; margin-bottom: 1rem;">
    <li><strong>C++26</strong> - Trading logic with constexpr guarantees and compile-time validation</li>
    <li><strong>Go</strong> - Data fetching, order execution, and API integration</li>
    <li><strong>JavaScript</strong> - Cloudflare Workers serverless functions</li>
    <li><strong>Svelte 5</strong> - Reactive frontend framework with automatic updates</li>
    <li><strong>Chart.js</strong> - Portfolio visualisation and equity curves</li>
    <li><strong>Cloudflare Pages</strong> - Edge-first hosting with automatic deployments</li>
    <li><strong>GitHub Pages</strong> - Analysis results and historical data publishing</li>
    <li><strong>GitHub Actions</strong> - CI/CD pipeline for automated builds</li>
    <li><strong>CMake</strong> - C++ build system</li>
    <li><strong>Vite</strong> - Fast development and optimised production builds</li>
  </ul>

  <h3 style="margin-top: 1.5rem; margin-bottom: 0.5rem; color: #8b949e;">Implementation Status</h3>
  <ul style="color: #8b949e; margin-left: 1.5rem; margin-bottom: 1rem;">
    <li>✓ <strong>test</strong> - Compile-time unit tests operational</li>
    <li>✓ <strong>profile</strong> - Backtest with historical data working</li>
    <li>✓ <strong>account</strong> - Worker providing /api/dashboard and /api/history</li>
    <li>✓ <strong>website</strong> - Svelte 5 SPA with real-time dashboard</li>
    <li>✓ <strong>fetch</strong> - Go module fetching 1000 bars per symbol</li>
    <li>⚙ <strong>filter</strong> - C++ module (basic implementation, needs full JSON parsing)</li>
    <li>⚙ <strong>backtest</strong> - C++ module (skeleton, needs strategy integration)</li>
    <li>○ <strong>evaluate</strong> - Signal generation logic needed</li>
    <li>○ <strong>execute</strong> - Order execution not yet implemented</li>
  </ul>

  <h3 style="margin-top: 1.5rem; margin-bottom: 0.5rem; color: #8b949e;">Project Links</h3>
  <p style="color: #8b949e;">
    <a href="https://github.com/deanturpin/lft2" target="_blank" rel="noopener noreferrer" style="color: #58a6ff; text-decoration: none;">Source Code (GitHub)</a><br>
    <a href="https://deanturpin.github.io/lft2/" target="_blank" rel="noopener noreferrer" style="color: #58a6ff; text-decoration: none;">Analysis Results (GitHub Pages)</a>
  </p>
</div>
