#pragma once
#include <string_view>
#include <cstddef>

namespace {
constexpr char baba_json_data[] = {
#embed "baba.json"
};
}

constexpr std::string_view baba_json{baba_json_data, std::size(baba_json_data)};
