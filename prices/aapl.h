#pragma once
#include <string_view>
#include <cstddef>

namespace {
constexpr char aapl_json_data[] = {
#embed "aapl.json"
};
}

constexpr std::string_view aapl_json{aapl_json_data, std::size(aapl_json_data)};
