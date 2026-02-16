#pragma once
#include <array>
#include <string_view>
#include <format>

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
constexpr auto TIME_IN_FORCE_IOC = "3";  // Immediate or cancel
constexpr auto TIME_IN_FORCE_FOK = "4";  // Fill or kill

// Simple FIX message builder
// Usage: auto msg = fix::message("D").add(55, "AAPL").add(54, "1").build();
class message {
	std::string body;
	std::string msg_type;

public:
	constexpr explicit message(std::string_view type) : msg_type{type} {}

	// Add a field to the message
	constexpr message& add(int tag, std::string_view value) {
		body += std::format("{}={}|", tag, value);
		return *this;
	}

	constexpr message& add(int tag, int value) {
		body += std::format("{}={}|", tag, value);
		return *this;
	}

	constexpr message& add(int tag, double value) {
		body += std::format("{}={:.2f}|", tag, value);
		return *this;
	}

	// Build complete FIX message with header and checksum
	std::string build(int seq_num = 1) const {
		auto msg_body = std::format("{}={}|{}={}|{}=LFT2|{}=ALPACA|{}",
			MSG_TYPE, msg_type,
			MSG_SEQ_NUM, seq_num,
			SENDER_COMP_ID, TARGET_COMP_ID,
			body);

		// Calculate simple checksum (sum of all bytes mod 256)
		auto checksum = 0;
		for (auto c : msg_body)
			checksum += static_cast<unsigned char>(c);
		checksum %= 256;

		return std::format("{}=FIX.5.0SP2|{}={}|{}{}={:03d}|\n",
			BEGIN_STRING,
			BODY_LENGTH, msg_body.size(),
			msg_body,
			CHECKSUM, checksum);
	}
};

// Helper to create a new order single message
inline auto new_order_single(std::string_view order_id,
                               std::string_view symbol,
                               std::string_view side,
                               int quantity,
                               std::string_view ord_type = ORD_TYPE_MARKET,
                               double price = 0.0,
                               std::string_view text = "") {
	auto msg = message{NEW_ORDER_SINGLE}
		.add(CL_ORD_ID, order_id)
		.add(HANDL_INST, "1")  // Automated execution
		.add(SYMBOL, symbol)
		.add(SIDE, side)
		.add(ORDER_QTY, quantity)
		.add(ORD_TYPE, ord_type)
		.add(TIME_IN_FORCE, TIME_IN_FORCE_DAY);

	if (price > 0.0)
		msg.add(PRICE, price);

	if (!text.empty())
		msg.add(TEXT, text);

	return msg;
}

} // namespace fix
