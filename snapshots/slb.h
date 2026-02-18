#pragma once
#include <string_view>
#include <cstddef>

namespace {
constexpr char slb_json_data[] = {
#embed "slb.json"
};
}

constexpr std::string_view slb_json{slb_json_data, std::size(slb_json_data)};
