<script>
  import Router from 'svelte-spa-router';
  import Dashboard from './routes/Dashboard.svelte';
  import About from './routes/About.svelte';

  const routes = {
    '/': Dashboard,
    '/about': About,
  };
</script>

<main>
  <h1>Low Frequency Trader</h1>

  <nav class="nav">
    <a href="#/" class="nav-link">Dashboard</a>
    <a href="#/about" class="nav-link">About</a>
  </nav>

  <Router {routes} />
</main>
