#pragma once
#include <string_view>
#include <cstddef>

namespace {
constexpr char jpm_json_data[] = {
#embed "jpm.json"
};
}

constexpr std::string_view jpm_json{jpm_json_data, std::size(jpm_json_data)};
