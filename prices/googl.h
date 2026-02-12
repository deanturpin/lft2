#pragma once
#include <string_view>
#include <cstddef>

namespace {
constexpr char googl_json_data[] = {
#embed "googl.json"
};
}

constexpr std::string_view googl_json{googl_json_data, std::size(googl_json_data)};
