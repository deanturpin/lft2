#pragma once
#include <string_view>
#include <cstddef>

namespace {
constexpr char cop_json_data[] = {
#embed "cop.json"
};
}

constexpr std::string_view cop_json{cop_json_data, std::size(cop_json_data)};
