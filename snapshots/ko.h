#pragma once
#include <string_view>
#include <cstddef>

namespace {
constexpr char ko_json_data[] = {
#embed "ko.json"
};
}

constexpr std::string_view ko_json{ko_json_data, std::size(ko_json_data)};
