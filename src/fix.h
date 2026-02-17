#pragma once
#include <format>
#include <string>
#include <string_view>

// FIX 5.0 SP2 protocol implementation
// Simple text-based format: tag=value|tag=value|...
// Both C++ and Go can read/write this format without linking

namespace fix {

// FIX message types
constexpr auto NEW_ORDER_SINGLE = "D";
constexpr auto ORDER_CANCEL_REQUEST = "F";

// Common FIX tags
constexpr auto BEGIN_STRING = 8;    // FIX version
constexpr auto BODY_LENGTH = 9;     // Message length
constexpr auto MSG_TYPE = 35;       // Message type
constexpr auto SENDER_COMP_ID = 49; // Sender ID
constexpr auto TARGET_COMP_ID = 56; // Target ID
constexpr auto MSG_SEQ_NUM = 34;    // Sequence number
constexpr auto SENDING_TIME = 52;   // UTC timestamp
constexpr auto CL_ORD_ID = 11;      // Client order ID
constexpr auto HANDL_INST = 21;     // Order handling (1=automated)
constexpr auto SYMBOL = 55;         // Ticker symbol
constexpr auto SIDE = 54;           // Buy(1) or Sell(2)
constexpr auto TRANSACT_TIME = 60;  // Transaction time
constexpr auto ORDER_QTY = 38;      // Number of shares
constexpr auto ORD_TYPE = 40;       // Order type (1=market, 2=limit)
constexpr auto PRICE = 44;          // Limit price
constexpr auto TIME_IN_FORCE = 59;  // Time validity (0=day, 3=IOC, 4=FOK)
constexpr auto TEXT = 58;           // Free text comment
constexpr auto CHECKSUM = 10;       // Message checksum

// Side values
constexpr auto SIDE_BUY = "1";
constexpr auto SIDE_SELL = "2";

// Order type values
constexpr auto ORD_TYPE_MARKET = "1";
constexpr auto ORD_TYPE_LIMIT = "2";

// Time in force values
constexpr auto TIME_IN_FORCE_DAY = "0";
constexpr auto TIME_IN_FORCE_IOC = "3"; // Immediate or cancel
constexpr auto TIME_IN_FORCE_FOK = "4"; // Fill or kill

// Build a complete FIX message from a pre-assembled body string.
// Adds standard header (BeginString, BodyLength) and trailer (Checksum).
inline std::string build(std::string_view msg_type, std::string_view body,
                         int seq_num = 1) {
  auto msg_body =
      std::format("{}={}|{}={}|{}=LFT2|{}=ALPACA|{}", MSG_TYPE, msg_type,
                  MSG_SEQ_NUM, seq_num, SENDER_COMP_ID, TARGET_COMP_ID, body);

  // Calculate simple checksum (sum of all bytes mod 256)
  auto checksum = 0;
  for (auto c : msg_body)
    checksum += static_cast<unsigned char>(c);
  checksum %= 256;

  return std::format("{}=FIX.5.0SP2|{}={}|{}{}={:03d}|\n", BEGIN_STRING,
                     BODY_LENGTH, msg_body.size(), msg_body, CHECKSUM,
                     checksum);
}

// Build a NewOrderSingle (D) FIX message for a market or limit order.
// price > 0 adds tag 44; text non-empty adds tag 58.
inline std::string
new_order_single(std::string_view order_id, std::string_view symbol,
                 std::string_view side, int quantity, int seq_num = 1,
                 std::string_view ord_type = ORD_TYPE_MARKET,
                 double price = 0.0, std::string_view text = "") {
  auto body = std::format("{}={}|{}=1|{}={}|{}={}|{}={}|{}={}|{}={}|",
                          CL_ORD_ID, order_id, HANDL_INST, SYMBOL, symbol, SIDE,
                          side, ORDER_QTY, quantity, ORD_TYPE, ord_type,
                          TIME_IN_FORCE, TIME_IN_FORCE_DAY);

  if (price > 0.0)
    body += std::format("{}={:.2f}|", PRICE, price);

  if (!text.empty())
    body += std::format("{}={}|", TEXT, text);

  return build(NEW_ORDER_SINGLE, body, seq_num);
}

} // namespace fix
