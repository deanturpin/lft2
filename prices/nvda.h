#pragma once
#include <string_view>
#include <cstddef>

namespace {
constexpr char nvda_json_data[] = {
#embed "nvda.json"
};
}

constexpr std::string_view nvda_json{nvda_json_data, std::size(nvda_json_data)};
