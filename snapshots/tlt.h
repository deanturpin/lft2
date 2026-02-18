#pragma once
#include <string_view>
#include <cstddef>

namespace {
constexpr char tlt_json_data[] = {
#embed "tlt.json"
};
}

constexpr std::string_view tlt_json{tlt_json_data, std::size(tlt_json_data)};
