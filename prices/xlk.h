#pragma once
#include <string_view>
#include <cstddef>

namespace {
constexpr char xlk_json_data[] = {
#embed "xlk.json"
};
}

constexpr std::string_view xlk_json{xlk_json_data, std::size(xlk_json_data)};
