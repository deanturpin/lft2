#pragma once
#include <string_view>
#include <cstddef>

namespace {
constexpr char dia_json_data[] = {
#embed "dia.json"
};
}

constexpr std::string_view dia_json{dia_json_data, std::size(dia_json_data)};
