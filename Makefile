BUILD_DIR := build
BINARY := $(BUILD_DIR)/lft2

.PHONY: all build run clean

all: run

build:
	cmake -S . -B $(BUILD_DIR) -DCMAKE_CXX_COMPILER=g++-15
	cmake --build $(BUILD_DIR) -j

run: build
	./$(BINARY)

clean:
	rm -rf $(BUILD_DIR)
