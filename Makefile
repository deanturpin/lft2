BUILD_DIR := build
BINARY := $(BUILD_DIR)/lft2
PROFILE := $(BUILD_DIR)/profile
FETCH := $(BUILD_DIR)/fetch
EVALUATE := $(BUILD_DIR)/evaluate
EXECUTE := $(BUILD_DIR)/execute

.PHONY: all build run clean profile fetch evaluate execute account web-dev web-build

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
