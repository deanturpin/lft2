#pragma once
#include <string_view>
#include <cstddef>

namespace {
constexpr char meta_json_data[] = {
#embed "meta.json"
};
}

constexpr std::string_view meta_json{meta_json_data, std::size(meta_json_data)};
