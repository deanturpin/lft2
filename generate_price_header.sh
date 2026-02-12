#!/bin/bash
# Generate constexpr price data header from CSV

symbol="$1"
csv_file="example/${symbol}_5min.csv"

if [[ ! -f "$csv_file" ]]; then
    echo "Error: $csv_file not found"
    exit 1
fi

cat << EOF
#pragma once
#include <string_view>

constexpr std::string_view $(echo "$symbol" | tr '[:upper:]' '[:lower:]')_5min_csv = R"(
EOF

cat "$csv_file"

cat << 'EOF'
)";
EOF
