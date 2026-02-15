#include <cstdlib>
#include <print>
#include <string_view>

// Trade executor
// Alpaca-specific order execution that reads signals and places trades
// Run this after trade to execute the generated trading signals

int main() {
  using namespace std::string_view_literals;

  std::println("Low Frequency Trader v2 - Trade Executor\n");

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

  // Read trading signals from trade binary output
  // Execute orders on Alpaca platform

  std::println("\nTrade execution not yet implemented");

  return 0;
}
