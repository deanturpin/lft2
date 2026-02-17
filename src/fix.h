#pragma once
#include <string>
#include <string_view>

// FIX 5.0 SP2 protocol implementation
// Simple text-based format: tag=value|tag=value|...
// Both C++ and Go can read/write this format without linking

namespace fix {

// FIX message types
constexpr auto HEARTBEAT = "0"; // Session alive / pipeline status
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
std::string build(std::string_view, std::string_view, int = 1);

// Heartbeat message — prepended to every .fix file so execute can confirm
// the C++ binary ran successfully even when there are no orders.
std::string heartbeat(std::string_view);

// Build a NewOrderSingle (D) FIX message for a market or limit order.
// price > 0 adds tag 44; text non-empty adds tag 58.
std::string new_order_single(std::string_view, std::string_view,
                             std::string_view, int, int = 1,
                             std::string_view = ORD_TYPE_MARKET, double = 0.0,
                             std::string_view = "");

} // namespace fix
