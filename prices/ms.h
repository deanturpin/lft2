#pragma once
#include <string_view>
#include <cstddef>

namespace {
constexpr char ms_json_data[] = {
#embed "ms.json"
};
}

constexpr std::string_view ms_json{ms_json_data, std::size(ms_json_data)};
