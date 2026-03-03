#pragma once
#include <string>

// Alpaca API position data structure
// Parsed from positions.json (account module output)
struct alpaca_position {
  std::string symbol;
  double qty;
  double avg_entry_price;
  std::string side;
  std::string client_order_id; // Original buy order ID (contains strategy +
                               // exit params)
};
