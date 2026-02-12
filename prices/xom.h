#pragma once
#include <string_view>
#include <cstddef>

namespace {
constexpr char xom_json_data[] = {
#embed "xom.json"
};
}

constexpr std::string_view xom_json{xom_json_data, std::size(xom_json_data)};
