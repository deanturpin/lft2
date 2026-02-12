#pragma once
#include <string_view>
#include <cstddef>

namespace {
constexpr char pg_json_data[] = {
#embed "pg.json"
};
}

constexpr std::string_view pg_json{pg_json_data, std::size(pg_json_data)};
