#pragma once
#include <string_view>
#include <cstddef>

namespace {
constexpr char cat_json_data[] = {
#embed "cat.json"
};
}

constexpr std::string_view cat_json{cat_json_data, std::size(cat_json_data)};
