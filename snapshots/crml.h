#pragma once
#include <string_view>
#include <cstddef>

namespace {
constexpr char crml_json_data[] = {
#embed "crml.json"
};
}

constexpr std::string_view crml_json{crml_json_data, std::size(crml_json_data)};
