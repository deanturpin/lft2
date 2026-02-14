BUILD_DIR := build
BINARY := $(BUILD_DIR)/lft2
BACKTEST := $(BUILD_DIR)/backtest

.PHONY: all build run clean backtest

all: run

build:
	cmake -S . -B $(BUILD_DIR) -DCMAKE_CXX_COMPILER=g++-15
	cmake --build $(BUILD_DIR) -j

run: build
	./$(BINARY)

backtest: build
	./$(BACKTEST)

clean:
	rm -rf $(BUILD_DIR)
