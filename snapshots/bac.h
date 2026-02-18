#pragma once
#include <string_view>
#include <cstddef>

namespace {
constexpr char bac_json_data[] = {
#embed "bac.json"
};
}

constexpr std::string_view bac_json{bac_json_data, std::size(bac_json_data)};
