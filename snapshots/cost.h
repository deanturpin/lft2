#pragma once
#include <string_view>
#include <cstddef>

namespace {
constexpr char cost_json_data[] = {
#embed "cost.json"
};
}

constexpr std::string_view cost_json{cost_json_data, std::size(cost_json_data)};
