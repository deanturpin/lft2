#pragma once
#include <string_view>
#include <cstddef>

namespace {
constexpr char spy_json_data[] = {
#embed "spy.json"
};
}

constexpr std::string_view spy_json{spy_json_data, std::size(spy_json_data)};
