#pragma once
#include <string_view>
#include <cstddef>

namespace {
constexpr char ief_json_data[] = {
#embed "ief.json"
};
}

constexpr std::string_view ief_json{ief_json_data, std::size(ief_json_data)};
