#pragma once
#include <string_view>
#include <cstddef>

namespace {
constexpr char cvx_json_data[] = {
#embed "cvx.json"
};
}

constexpr std::string_view cvx_json{cvx_json_data, std::size(cvx_json_data)};
