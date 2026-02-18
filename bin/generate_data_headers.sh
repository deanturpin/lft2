#!/bin/bash

# Generate C++ headers from JSON price files in snapshots/ directory
# Creates headers in snapshots/ with matching names (e.g., aapl.json -> aapl.h)
# Uses C++26 #embed for efficient compile-time embedding

for json_file in snapshots/*.json; do
    # Extract symbol from filename (e.g., aapl.json -> aapl)
    symbol=$(basename "$json_file" .json)

    header_file="snapshots/${symbol}.h"

    cat > "$header_file" << EOF
#pragma once
#include <string_view>
#include <cstddef>

namespace {
constexpr char ${symbol}_json_data[] = {
#embed "${symbol}.json"
};
}

constexpr std::string_view ${symbol}_json{${symbol}_json_data, std::size(${symbol}_json_data)};
EOF

    echo "Generated $header_file"
done
