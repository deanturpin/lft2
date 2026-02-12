#pragma once
#include <string_view>
#include <cstddef>

namespace {
constexpr char unh_json_data[] = {
#embed "unh.json"
};
}

constexpr std::string_view unh_json{unh_json_data, std::size(unh_json_data)};
