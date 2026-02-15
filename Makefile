BUILD_DIR := build
BINARY := $(BUILD_DIR)/lft2
PROFILE := $(BUILD_DIR)/profile
TRADE := $(BUILD_DIR)/trade

.PHONY: all build run clean profile trade

all: run

build:
	cmake -S . -B $(BUILD_DIR) -DCMAKE_CXX_COMPILER=g++-15
	cmake --build $(BUILD_DIR) -j

run: build
	./$(BINARY)

profile: build
	./$(PROFILE)

trade: build
	./$(TRADE)

clean:
	rm -rf $(BUILD_DIR)
