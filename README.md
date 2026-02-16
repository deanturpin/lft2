# Low Frequency Trader

Fully automated algorithmic trading platform with constexpr-first C++26 architecture.

**Live Dashboard**: <https://lft.turpin.dev>
**About & Documentation**: <https://lft.turpin.dev/#/about>

## Quick Start

```bash
# Run analysis pipeline (fetch → filter → backtest)
make pipeline

# View help
make help
```

## Project Structure

```
lft2/
├── cmd/          # Go executables (fetch, execute)
├── src/          # C++ modules (filter, backtest, evaluate)
├── web/          # Svelte dashboard
├── workers/      # Cloudflare Workers API
└── docs/         # GitHub Pages output
```

## Development

**Requirements:**
- C++26 compiler (g++-15 or clang-18+)
- Go 1.21+
- Node.js 20+

**Build:**
```bash
# Analysis pipeline
make pipeline

# Web dashboard
cd web && npm run dev

# Legacy CMake build
make build
```

## Deployment

- **Dashboard**: Cloudflare Pages (auto-deploy on push)
- **API**: Cloudflare Workers
- **Analysis**: GitHub Pages
- **Trading**: Fasthosts VPS

See [DEPLOYMENT.md](DEPLOYMENT.md) for details.

## Architecture Highlights

- **Constexpr trading logic** - Compile-time validation with `static_assert`
- **Multi-language** - C++26 for logic, Go for APIs, JavaScript for edge functions
- **Edge-first** - Sub-100ms dashboard response times via Cloudflare
- **Type-safe** - Zero runtime overhead, guaranteed correctness

Full architecture details at <https://lft.turpin.dev/#/about>

## License

Private project - not open source.
