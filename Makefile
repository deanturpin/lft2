# ============================================================
# LFT2 Build System
#
# Two distinct roles:
#   cmake  - compiles C++ strategy modules (exits, entries, backtest, etc.)
#   make   - sequences Go and C++ modules in the correct order
#
# Live trading loop:  make run      (requires API credentials)
# Backtest pipeline:  make backtest (requires API credentials, run manually)
# ============================================================

BUILD_DIR := build
BACKTEST  := $(BUILD_DIR)/backtest
ANALYSE   := $(BUILD_DIR)/analyse
EXITS     := $(BUILD_DIR)/exits
ENTRIES   := $(BUILD_DIR)/entries

.PHONY: all build run backtest analyse clean \
        fetch-go filter-go backtest-cpp help

# Default: compile then run live trading loop
all: run

# ============================================================
# cmake: compile all C++ modules into build/
# ============================================================
build:
	cmake -S . -B $(BUILD_DIR) -DCMAKE_CXX_COMPILER=$(GCXX)
	cmake --build $(BUILD_DIR) -j

# g++-15 required for C++26; override: GCXX=g++ make build
GCXX ?= g++-15

# ============================================================
# GNU make: live trading loop (module sequencing)
#   account  - fetch positions and cash balance from Alpaca → account.json, positions.json
#   exits    - check open positions for exit signals → sell.fix
#   wait-for-bar - sleep until next 5-min bar (+ 35s Alpaca publish delay)
#   fetch    - get latest 25 bars for strategy candidates → docs/bars/
#   entries  - evaluate entry signals → buy.fix
#   execute  - submit buy.fix and sell.fix orders to Alpaca
# ============================================================
run: build
	@echo "=== LFT2 Live Trading Loop ==="
	@echo ""
	@echo "→ account"
	@cd cmd/account && $(MAKE) --no-print-directory run
	@echo ""
	@echo "→ exits"
	@./$(EXITS)
	@echo ""
	@echo "→ fetch"
	@cd cmd/fetch && $(MAKE) --no-print-directory run
	@echo ""
	@echo "→ entries"
	@./$(ENTRIES)
	@echo ""
	@echo "→ execute"
	@cd cmd/execute && $(MAKE) --no-print-directory run
	@echo ""
	@echo "=== loop complete ==="

# ============================================================
# GNU make: backtest pipeline (module sequencing)
# Run manually to regenerate strategies.json and GitHub Pages data.
# Not triggered on push - use scheduled CI or run locally.
#   fetch    - fetch 1000 bars per watchlist symbol → docs/bars/
#   filter   - score and rank candidates → docs/strategies.json
#   backtest - run C++ strategies and write results → docs/
# ============================================================
backtest: fetch-go filter-go backtest-cpp analyse
	@echo ""
	@echo "=== backtest complete ==="
	@echo "{\"timestamp\": \"$$(date -u +%Y-%m-%dT%H:%M:%SZ)\"}" > docs/pipeline-metadata.json
	@echo '{'                                                                        > docs/tech-stack.json
	@echo "  \"go\":     \"$$(go version | cut -d' ' -f3)\","                      >> docs/tech-stack.json
	@echo "  \"gcc\":    \"$$(g++ --version | head -1 | awk '{print $$NF}')\"," >> docs/tech-stack.json
	@echo "  \"cmake\":  \"$$(cmake --version | head -1 | awk '{print $$3}')\"," >> docs/tech-stack.json
	@echo "  \"make\":   \"$$(make --version | head -1 | awk '{print $$3}')\"," >> docs/tech-stack.json
	@echo "  \"os\":     \"$$(uname -s)\"," >> docs/tech-stack.json
	@echo "  \"kernel\": \"$$(uname -r)\"" >> docs/tech-stack.json
	@echo '}'                                                                       >> docs/tech-stack.json

fetch-go:
	@echo "→ fetch (backtest: 1000 bars)"
	@cd cmd/fetch && $(MAKE) --no-print-directory run-backtest

filter-go:
	@echo "→ filter"
	@cd cmd/filter && $(MAKE) --no-print-directory run

backtest-cpp: build
	@echo "→ backtest"
	@./$(BACKTEST)

# Run analyse binary, then generate:
#   docs/analyse.json   - per-symbol strategy stats (JSON)
#   docs/coverage/       - gcov HTML coverage report (lcov + genhtml)
#   docs/callgraph.svg   - gprof call graph as SVG (Linux only, via gprof2dot + dot)
analyse: build
	@echo "→ analyse"
	@./$(ANALYSE) --json > docs/analyse.json
	@echo "→ wrote docs/analyse.json"
	@lcov --capture --directory $(BUILD_DIR) --output-file docs/coverage.info \
	      --gcov-tool gcov-15 --ignore-errors mismatch 2>/dev/null || true
	@genhtml docs/coverage.info --output-directory docs/coverage \
	         --title "LFT2 Coverage" --quiet 2>/dev/null || true
	@echo "→ wrote docs/coverage/"
	@gprof $(ANALYSE) gmon.out 2>/dev/null \
	    | gprof2dot -f gprof 2>/dev/null \
	    | dot -Tsvg -o docs/callgraph.svg 2>/dev/null || true
	@echo "→ wrote docs/callgraph.svg"

# ============================================================
# Housekeeping
# ============================================================
clean:
	rm -rf $(BUILD_DIR)
	cd cmd/fetch && $(MAKE) clean
	cd cmd/filter && $(MAKE) clean

help:
	@echo "LFT2 Build System"
	@echo ""
	@echo "  make          - compile and run live trading loop"
	@echo "  make backtest - compile and run full backtest pipeline"
	@echo "  make build    - cmake: compile C++ modules only"
	@echo "  make clean    - remove all build artefacts and fetched data"
