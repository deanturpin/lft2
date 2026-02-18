#pragma once
#include <string_view>
#include <cstddef>

namespace {
constexpr char qqq_json_data[] = {
#embed "qqq.json"
};
}

constexpr std::string_view qqq_json{qqq_json_data, std::size(qqq_json_data)};
