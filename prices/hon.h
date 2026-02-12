#pragma once
#include <string_view>
#include <cstddef>

namespace {
constexpr char hon_json_data[] = {
#embed "hon.json"
};
}

constexpr std::string_view hon_json{hon_json_data, std::size(hon_json_data)};
