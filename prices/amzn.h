#pragma once
#include <string_view>
#include <cstddef>

namespace {
constexpr char amzn_json_data[] = {
#embed "amzn.json"
};
}

constexpr std::string_view amzn_json{amzn_json_data, std::size(amzn_json_data)};
