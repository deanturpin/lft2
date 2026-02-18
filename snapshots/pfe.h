#pragma once
#include <string_view>
#include <cstddef>

namespace {
constexpr char pfe_json_data[] = {
#embed "pfe.json"
};
}

constexpr std::string_view pfe_json{pfe_json_data, std::size(pfe_json_data)};
