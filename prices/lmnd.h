#pragma once
#include <string_view>
#include <cstddef>

namespace {
constexpr char lmnd_json_data[] = {
#embed "lmnd.json"
};
}

constexpr std::string_view lmnd_json{lmnd_json_data, std::size(lmnd_json_data)};
