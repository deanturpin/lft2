#include "fix.h"
#include <format>

namespace fix {

// Build a complete FIX message from a pre-assembled body string.
// Adds standard header (BeginString, BodyLength) and trailer (Checksum).
std::string build(std::string_view msg_type, std::string_view body,
                  int seq_num) {
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
std::string new_order_single(std::string_view order_id, std::string_view symbol,
                             std::string_view side, int quantity, int seq_num,
                             std::string_view ord_type, double price,
                             std::string_view text) {
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

// Heartbeat with a free-text status field (tag 58).
// Always written as seq_num=0 to distinguish from order messages.
std::string heartbeat(std::string_view text) {
  return build(HEARTBEAT, std::format("{}={}|", TEXT, text), 0);
}

} // namespace fix
