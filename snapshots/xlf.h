#pragma once
#include <string_view>
#include <cstddef>

namespace {
constexpr char xlf_json_data[] = {
#embed "xlf.json"
};
}

constexpr std::string_view xlf_json{xlf_json_data, std::size(xlf_json_data)};
