#pragma once
#include <string_view>
#include <cstddef>

namespace {
constexpr char vnq_json_data[] = {
#embed "vnq.json"
};
}

constexpr std::string_view vnq_json{vnq_json_data, std::size(vnq_json_data)};
