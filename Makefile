BUILD_DIR := build
BINARY := $(BUILD_DIR)/lft2
PROFILE := $(BUILD_DIR)/profile
FETCH := $(BUILD_DIR)/fetch
EVALUATE := $(BUILD_DIR)/evaluate
EXECUTE := $(BUILD_DIR)/execute

.PHONY: all build run clean profile fetch evaluate execute

all: run

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

clean:
	rm -rf $(BUILD_DIR)
