#pragma once
#include <string_view>
#include <cstddef>

namespace {
constexpr char iwm_json_data[] = {
#embed "iwm.json"
};
}

constexpr std::string_view iwm_json{iwm_json_data, std::size(iwm_json_data)};
