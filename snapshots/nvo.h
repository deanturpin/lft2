#pragma once
#include <string_view>
#include <cstddef>

namespace {
constexpr char nvo_json_data[] = {
#embed "nvo.json"
};
}

constexpr std::string_view nvo_json{nvo_json_data, std::size(nvo_json_data)};
