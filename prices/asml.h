#pragma once
#include <string_view>
#include <cstddef>

namespace {
constexpr char asml_json_data[] = {
#embed "asml.json"
};
}

constexpr std::string_view asml_json{asml_json_data, std::size(asml_json_data)};
