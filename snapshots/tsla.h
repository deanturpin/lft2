#pragma once
#include <string_view>
#include <cstddef>

namespace {
constexpr char tsla_json_data[] = {
#embed "tsla.json"
};
}

constexpr std::string_view tsla_json{tsla_json_data, std::size(tsla_json_data)};
