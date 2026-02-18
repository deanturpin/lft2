#pragma once
#include <string_view>
#include <cstddef>

namespace {
constexpr char wmt_json_data[] = {
#embed "wmt.json"
};
}

constexpr std::string_view wmt_json{wmt_json_data, std::size(wmt_json_data)};
