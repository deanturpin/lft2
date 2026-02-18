<script>
  const BASE = 'https://deanturpin.github.io/lft2';

  const analysisFiles = [
    { name: 'candidates.json',        description: 'Filtered candidate stocks',             type: 'JSON' },
    { name: 'strategies.json',        description: 'Backtested strategy recommendations',   type: 'JSON' },
    { name: 'profile.json',           description: 'Strategy profiler — per-symbol stats',  type: 'JSON' },
    { name: 'callgraph.svg',          description: 'C++ call graph (gprof → gprof2dot)',    type: 'SVG'  },
    { name: 'coverage/index.html',    description: 'Code coverage report (lcov)',           type: 'HTML' },
    { name: 'pipeline-metadata.json', description: 'Pipeline execution metadata',           type: 'JSON' },
    { name: 'tech-stack.json',        description: 'Build environment and tool versions',   type: 'JSON' },
  ];

  async function loadMetadata() {
    const res = await fetch(`${BASE}/pipeline-metadata.json`);
    if (!res.ok) return null;
    const data = await res.json();
    return new Date(data.timestamp).toLocaleString('en-GB', {
      year: 'numeric', month: 'short', day: 'numeric',
      hour: '2-digit', minute: '2-digit',
      timeZone: 'UTC', timeZoneName: 'short',
    });
  }
</script>

<div class="card">
  <h2 style="color: #58a6ff; margin-bottom: 0.5rem;">Analysis and Historical Data</h2>

  {#await loadMetadata()}
    <p class="timestamp">Loading...</p>
  {:then timestamp}
    <p class="timestamp">{timestamp ? `Last updated: ${timestamp}` : 'Pipeline not yet run'}</p>
  {/await}

  <ul style="list-style: none; margin-top: 1rem;">
    {#each analysisFiles as file}
      <li style="padding: 0.5rem 0; border-bottom: 1px solid #21262d;">
        <a
          href="{BASE}/{file.name}"
          target="_blank"
          rel="noopener noreferrer"
          style="color: #58a6ff; text-decoration: none; display: flex; justify-content: space-between; align-items: center;"
        >
          <span>{file.description}</span>
          <span style="color: #8b949e; font-size: 0.75rem; text-transform: uppercase;">{file.type}</span>
        </a>
      </li>
    {/each}
  </ul>
</div>
