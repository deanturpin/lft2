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
├── cmd/          # Go executables (fetch, filter, execute)
├── src/          # C++ modules (backtest, evaluate)
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

## Modern C++ Features Showcase

This project pushes the boundaries of modern C++ (C++23/26) to achieve compile-time trading logic validation and zero-overhead abstractions:

### C++26 Features

**`#embed` directive** - Embeds external files at compile time

```cpp
// Load 1000 bars of historical data directly into binary
static constexpr char aapl_data[] = {
#embed "aapl.json"
};
```

Eliminates runtime file I/O for test data and enables compile-time data validation.

**`constexpr std::format`** - Compile-time string formatting

```cpp
constexpr std::string build_fix_message(std::string_view symbol, int qty) {
  return std::format("35=D|55={}|38={}|", symbol, qty);  // Evaluated at compile time
}
```

Generates FIX protocol messages with zero runtime overhead.

**`constexpr std::string`** - Compile-time string operations

```cpp
constexpr auto parse_bars(std::string_view json) {
  // Full JSON parsing at compile time
  return std::array<bar, 1000>{...};
}
static_assert(parse_bars(embedded_json)[0].close > 0.0);
```

Entire JSON parser runs at compile time with `static_assert` validation.

### C++23 Features

**`std::println`** - Modern output formatting

```cpp
std::println("✓ Processed {} bars for {}", count, symbol);
```

Replaces verbose `std::cout <<` chains with clean, format-string based output.

### Why This Matters for Trading

- **Compile-time validation** - Trading logic bugs caught before deployment
- **Zero runtime cost** - All formatting and parsing optimized away
- **Type safety** - `constexpr` guarantees ensure correctness
- **Fast iteration** - `static_assert` tests run instantly during compilation

## License

Private project - not open source.
