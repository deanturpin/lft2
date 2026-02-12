#pragma once
#include <string_view>
#include <cstddef>

namespace {
constexpr char ge_json_data[] = {
#embed "ge.json"
};
}

constexpr std::string_view ge_json{ge_json_data, std::size(ge_json_data)};
