#pragma once
#include <string_view>
#include <cstddef>

namespace {
constexpr char de_json_data[] = {
#embed "de.json"
};
}

constexpr std::string_view de_json{de_json_data, std::size(de_json_data)};
