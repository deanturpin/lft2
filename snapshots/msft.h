#pragma once
#include <string_view>
#include <cstddef>

namespace {
constexpr char msft_json_data[] = {
#embed "msft.json"
};
}

constexpr std::string_view msft_json{msft_json_data, std::size(msft_json_data)};
