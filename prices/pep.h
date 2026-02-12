#pragma once
#include <string_view>
#include <cstddef>

namespace {
constexpr char pep_json_data[] = {
#embed "pep.json"
};
}

constexpr std::string_view pep_json{pep_json_data, std::size(pep_json_data)};
