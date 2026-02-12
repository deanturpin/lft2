#pragma once
#include <string_view>
#include <cstddef>

namespace {
constexpr char lly_json_data[] = {
#embed "lly.json"
};
}

constexpr std::string_view lly_json{lly_json_data, std::size(lly_json_data)};
