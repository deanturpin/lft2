#include <cstdlib>
#include <print>
#include <string_view>

// Market data fetcher
// Alpaca-specific data fetching that writes market snapshots to files
// Run this before trade to get latest market data

int main() {
  using namespace std::string_view_literals;

  std::println("Low Frequency Trader v2 - Market Data Fetcher\n");

  // Load credentials from environment
  auto api_key = std::getenv("ALPACA_API_KEY");
  auto api_secret = std::getenv("ALPACA_API_SECRET");

  if (!api_key || !api_secret) {
    std::println("Error: ALPACA_API_KEY and ALPACA_API_SECRET must be set");
    std::println("Create a .env file with your credentials and source it");
    return 1;
  }

  std::println("Credentials loaded successfully");
  std::println("API Key: {}***", std::string_view{api_key}.substr(0, 8));

  // Fetch latest snapshots from Alpaca
  // Write market data to files for trade binary to consume

  std::println("\nMarket data fetching not yet implemented");

  return 0;
}
