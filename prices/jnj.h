#pragma once
#include <string_view>
#include <cstddef>

namespace {
constexpr char jnj_json_data[] = {
#embed "jnj.json"
};
}

constexpr std::string_view jnj_json{jnj_json_data, std::size(jnj_json_data)};
