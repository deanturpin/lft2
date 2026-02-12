#pragma once
#include <string_view>
#include <cstddef>

namespace {
constexpr char rsp_json_data[] = {
#embed "rsp.json"
};
}

constexpr std::string_view rsp_json{rsp_json_data, std::size(rsp_json_data)};
