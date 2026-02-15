#include <cstdlib>
#include <print>
#include <string_view>

// Live trading application
// Fetches real-time data from Alpaca and executes trading strategies

int main() {
  using namespace std::string_view_literals;

  std::println("Low Frequency Trader v2 - Live Trading\n");

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

  // TODO: Implement AlpacaClient integration
  // TODO: Fetch live snapshots
  // TODO: Run entry strategies
  // TODO: Execute trades

  std::println("\nLive trading not yet implemented");
  std::println("Use 'profile' target for strategy testing with historical data");

  return 0;
}
