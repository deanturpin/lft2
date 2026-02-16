BUILD_DIR := build
BINARY := $(BUILD_DIR)/lft2
PROFILE := $(BUILD_DIR)/profile
FETCH := $(BUILD_DIR)/fetch
EVALUATE := $(BUILD_DIR)/evaluate
BACKTEST := $(BUILD_DIR)/backtest

.PHONY: all build run clean profile cmake-build fetch-go filter-go backtest-cpp backtest evaluate account web-dev web-build pipeline help

# Default: CMake build for legacy C++ code
all: run

# New analysis pipeline: fetch (Go) → filter (Go) → backtest (C++) → docs/
pipeline: fetch-go filter-go backtest-cpp
	@echo "✓ Pipeline complete! Generated files in docs/"
	@echo "{\"timestamp\": \"$$(date -u +%Y-%m-%dT%H:%M:%SZ)\"}" > docs/pipeline-metadata.json
	@echo "Generated docs/pipeline-metadata.json"
	@echo "Generating tech stack metadata..."
	@echo "{" > docs/tech-stack.json
	@echo "  \"go\": \"$$(go version | cut -d' ' -f3)\"," >> docs/tech-stack.json
	@echo "  \"gcc\": \"$$(g++ --version | head -1 | awk '{print $$NF}')\"," >> docs/tech-stack.json
	@echo "  \"cmake\": \"$$(cmake --version | head -1 | awk '{print $$3}')\"," >> docs/tech-stack.json
	@echo "  \"make\": \"$$(make --version | head -1 | awk '{print $$3}')\"," >> docs/tech-stack.json
	@echo "  \"os\": \"$$(if [ -f /etc/os-release ]; then . /etc/os-release && echo $$PRETTY_NAME $$VERSION_ID; else uname -s; fi)\"," >> docs/tech-stack.json
	@echo "  \"kernel\": \"$$(uname -r)\"" >> docs/tech-stack.json
	@echo "}" >> docs/tech-stack.json
	@echo "Generated docs/tech-stack.json"

# Backtest target: full pipeline (fetch → filter → backtest)
backtest: fetch-go filter-go backtest-cpp

# Fetch 1000 bars per symbol using Go (Alpaca pagination limit)
fetch-go:
	@echo "→ Fetching bar data (1000 bars per symbol - Alpaca limit)..."
	cd cmd/fetch && $(MAKE) run

# Filter candidates using Go
filter-go:
	@echo "→ Filtering candidate stocks..."
	cd cmd/filter && $(MAKE) run

# Backtest strategies using C++ (constexpr entry/exit logic)
backtest-cpp: build
	@echo "→ Backtesting strategies..."
	./$(BACKTEST)

# Legacy CMake build
build:
	cmake -S . -B $(BUILD_DIR) -DCMAKE_CXX_COMPILER=g++-15
	cmake --build $(BUILD_DIR) -j

run: build
	./$(BINARY)

profile: build
	./$(PROFILE)

fetch: build
	./$(FETCH)

evaluate: build
	./$(EVALUATE)

execute: build
	./$(EXECUTE)

account:
	@echo "Starting account service..."
	@if [ ! -f .env ]; then echo "Error: .env file not found"; exit 1; fi
	@export $$(cat .env | xargs) && go run cmd/account/main.go

web-dev:
	cd web && npm install && npm run dev

web-build:
	cd web && npm install && npm run build

clean:
	rm -rf $(BUILD_DIR)
	cd cmd/fetch && $(MAKE) clean
	cd cmd/filter && $(MAKE) clean

help:
	@echo "LFT2 Build System"
	@echo ""
	@echo "Main: make pipeline  (fetch → filter → backtest → docs/)"
	@echo "      make clean     (remove all generated data)"
