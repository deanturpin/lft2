#pragma once
#include <string_view>
#include <cstddef>

namespace {
constexpr char gs_json_data[] = {
#embed "gs.json"
};
}

constexpr std::string_view gs_json{gs_json_data, std::size(gs_json_data)};
