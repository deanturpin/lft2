BUILD_DIR := build
BINARY := $(BUILD_DIR)/lft2
PROFILE := $(BUILD_DIR)/profile
FETCH := $(BUILD_DIR)/fetch
EVALUATE := $(BUILD_DIR)/evaluate
EXECUTE := $(BUILD_DIR)/execute

.PHONY: all build run clean profile cmake-build fetch-go filter-go backtest-cpp backtest evaluate execute account web-dev web-build pipeline help

# Default: CMake build for legacy C++ code
all: run

# New analysis pipeline: fetch (Go) → filter (C++) → backtest (C++) → docs/
pipeline: fetch-go filter-go backtest-cpp
	@echo "✓ Pipeline complete! Generated files in docs/"

# Backtest target: build C++ and run pipeline
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
backtest-cpp:
	@echo "→ Backtesting strategies..."
	cd src/backtest && $(MAKE) run

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
	cd src/backtest && $(MAKE) clean

help:
	@echo "LFT2 Build System"
	@echo ""
	@echo "Main: make pipeline  (fetch → filter → backtest → docs/)"
	@echo "      make clean     (remove all generated data)"
