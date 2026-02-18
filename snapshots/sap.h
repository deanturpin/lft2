#pragma once
#include <string_view>
#include <cstddef>

namespace {
constexpr char sap_json_data[] = {
#embed "sap.json"
};
}

constexpr std::string_view sap_json{sap_json_data, std::size(sap_json_data)};
