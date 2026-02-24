#include "fix.h"
#include <chrono>
#include <format>

namespace fix {

// Heartbeat with UTC timestamp (tag 52) and free-text status (tag 58).
// Always written as seq_num=0 to distinguish from order messages.
// NOT constexpr because it uses std::chrono::system_clock::now()
std::string heartbeat(std::string_view text) {
  auto now = std::chrono::system_clock::now();
  auto ts = std::format("{:%Y%m%d-%H:%M:%S}", now);
  return build(HEARTBEAT,
               std::format("{}={}|{}={}|", SENDING_TIME, ts, TEXT, text), 0);
}

} // namespace fix
