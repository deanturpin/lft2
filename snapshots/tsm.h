#pragma once
#include <string_view>
#include <cstddef>

namespace {
constexpr char tsm_json_data[] = {
#embed "tsm.json"
};
}

constexpr std::string_view tsm_json{tsm_json_data, std::size(tsm_json_data)};
